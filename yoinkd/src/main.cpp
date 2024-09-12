#include "command.hpp"
#include "config.hpp"
#include "monitor.hpp"

#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/experimental/coro.hpp>
#include <boost/asio/experimental/use_coro.hpp>
#include <boost/process/search_path.hpp>
#include <boost/process/v2.hpp>
#include <boost/scope_exit.hpp>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <pcre2.h>
#include <print>
#include <yaml-cpp/yaml.h>

int main(int argc, char** argv)
{
	CLI::App app{};
	app.option_defaults()->always_capture_default();

	bool force_root = false;
	app.add_flag("--force-root", force_root)->envname("YOINK_FORCE_ROOT");

	bool foreground = false;
	app.add_flag("-f,--foreground", foreground)->envname("YOINK_FOREGROUND");

	bool flush = true;
	app.add_flag("!--no-flush", flush)->envname("YOINK_NO_FLUSH");

	std::filesystem::path pid_file = "/var/run/yoinkd.pid";
	app.add_option("--pid-file", pid_file)->envname("YOINK_PID_FILE");

	std::filesystem::path config_path{};
	app.add_option("-c,--config", config_path)->envname("YOINK_CONFIG")->check(CLI::ExistingFile)->required();

	CLI11_PARSE(app, argc, argv);

	if (!force_root && getuid() == 0) {
		std::println(
		  "Trying to run this as root. Consider https://github.com/terrakuh/yoink/README.md#Security.");
		return EXIT_FAILURE;
	}

	const auto config = YAML::LoadFile(config_path).as<yoink::Config>();
	std::filesystem::current_path(std::filesystem::canonical(config_path).parent_path());

	if (!foreground) {
		if (const auto pid = fork(); pid == -1) {
			std::println("Failed to fork");
			return EXIT_FAILURE;
		} else if (pid > 0) {
			return EXIT_SUCCESS;
		}
	}

	if (pid_file != "") {
		if (std::ifstream file{ pid_file, std::ios::in }; file.is_open()) {
			pid_t pid = 0;
			file >> pid;
			if (kill(pid, 0) != -1 || errno != ESRCH) {
				std::println("PID {} file already exists", pid_file.string());
				return EXIT_FAILURE;
			}
		}
		std::ofstream file{ pid_file, std::ios::out };
		file << getpid();
	}
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (pid_file != "") {
			std::filesystem::remove(pid_file);
		}
	};

	boost::asio::io_service service{};

	if (flush) {
		std::println("Flushing nft set");
		boost::asio::co_spawn(
		  service,
		  [&] -> boost::asio::awaitable<void> {
			  co_await yoink::process({
			    .program = boost::process::search_path("nft").string(),
			    .args    = { "flush", "set", "inet", "yoinkd", "blacklist4" },
			  });
		  }(),
		  boost::asio::detached);
	}

	yoink::Manager manager{ service.get_executor(), config.ban_settings };

	for (const auto& monitor : config.monitors) {
		boost::asio::co_spawn(service, yoink::monitor(manager, monitor), boost::asio::detached);
	}

	while (true) {
		try {
			service.run();
			break;
		} catch (const std::exception& e) {
			std::println("Exception from main: {}", e.what());
		}
	}

	return EXIT_SUCCESS;
}
