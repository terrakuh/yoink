#include "command.hpp"

#include <boost/asio.hpp>
// #include <boost/asio/experimental/basic_concurrent_channel.hpp>static_cast<char*>(ptr)
#include <boost/asio/experimental/use_coro.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/process/v2.hpp>
#include <boost/scope_exit.hpp>
#include <cerrno>
#include <print>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

boost::asio::awaitable<int> yoink::process(Process process)
try {
	boost::process::v2::process p{ co_await boost::asio::this_coro::executor, process.program, process.args,
		                             boost::process::v2::process_stdio{} };
	co_return co_await boost::process::v2::async_execute(std::move(p), boost::asio::use_awaitable);
} catch (const std::exception& e) {
	std::println("Command '{}' failed: {}", process.program, e.what());
	throw;
}

boost::asio::experimental::coro<std::string_view, int>
  yoink::stream_process(boost::asio::any_io_executor executor, Process process)
{
	for (const auto& arg : process.args) {
		process.program += " ";
		process.program += arg;
	}
	process.program = "stdbuf -o0 -e0 " + process.program;
	const auto pipe = popen(process.program.c_str(), "r");
	if (pipe == nullptr) {
		throw std::runtime_error{ std::format("Failed to open pipe: {}", strerror(errno)) };
	}
	BOOST_SCOPE_EXIT_ALL(&) { pclose(pipe); };

	char data[32 * 1024]{};
	std::size_t pos = 0;

	// fcntl(fileno(pipe), F_SETFL, O_NONBLOCK);
	ioctl(fileno(pipe), F_SETPIPE_SZ, 64 * 1024);

	auto future = std::async(std::launch::async, [&] {
		while (true) {
			// timespec timeout{};
			// timeout.tv_nsec = 1'000'000;
			// fd_set rfds;
			// FD_ZERO(&rfds);
			// FD_SET(fileno(pipe), &rfds);

			// const int ready = pselect(fileno(pipe) + 1, &rfds, nullptr, nullptr, nullptr, nullptr);
			// if (ready < 0) {
			// 	throw std::runtime_error{ std::format("Failed to select pipe: {}", strerror(errno)) };
			// } else if (ready == 0) {
			// 	continue;
			// }
			const std::size_t read = fread(data + pos, 1, 1, pipe);
			auto ptr               = std::memchr(data + pos, '\n', read);
			pos += read;
			if (ptr == nullptr && read != 0) {
				continue;
			}
			if (ptr == nullptr) {
				ptr = data + pos;
			}

			std::println("read {} bytes from pipe", read);
			// co_yield std::string_view{ data, static_cast<char*>(ptr) };
			// channel.write(std::string{ data, static_cast<char*>(ptr) }, boost::asio::use_future);

			const std::size_t left = pos - (static_cast<char*>(ptr) - data);
			std::memmove(data, ptr, left);
			pos = left;
		}
	});

	while (true) {
		boost::asio::steady_timer t{ executor };
		t.expires_after(std::chrono::seconds{ 100 });
		t.async_wait(boost::asio::experimental::use_coro);
	}

	// setbuf(pipe, NULL);
	// while (true) {
	// 	timespec timeout{};
	// 	timeout.tv_nsec = 1'000'000;
	// 	fd_set rfds;
	// 	FD_ZERO(&rfds);
	// 	FD_SET(fileno(pipe), &rfds);

	// 	const int ready = pselect(fileno(pipe) + 1, &rfds, nullptr, nullptr, &timeout, nullptr);
	// 	if (ready < 0) {
	// 		throw std::runtime_error{ std::format("Failed to select pipe: {}", strerror(errno)) };
	// 	} else if (ready == 0) {
	// 		co_await boost::asio::post(executor, boost::asio::experimental::use_coro);
	// 		continue;
	// 	}

	// 	const std::size_t read = fread(data + pos, 1, sizeof(data) - pos, pipe);
	// 	std::println("read {} bytes from pipe", read);
	// 	auto ptr = std::memchr(data + pos, '\n', read);
	// 	pos += read;
	// 	if (ptr == nullptr && read != 0) {
	// 		continue;
	// 	}
	// 	if (ptr == nullptr) {
	// 		ptr = data + pos;
	// 	}

	// 	co_yield std::string_view{ data, static_cast<char*>(ptr) };

	// 	const std::size_t left = pos - (static_cast<char*>(ptr) - data);
	// 	std::memmove(data, ptr, left);
	// 	pos = left;
	// }

	// boost::asio::readable_pipe pipe{ executor };
	// fcntl(pipe.native_handle(), F_SETFL, O_NONBLOCK);
	// ioctl(pipe.native_handle(), F_SETPIPE_SZ, 64 * 1024);
	// boost::process::v2::process p{ executor, process.program, process.args,
	// 	                             boost::process::v2::process_stdio{ .out = pipe } };

	// std::string data{};
	// while (true) {
	// 	const std::size_t size = co_await async_read_until(pipe, boost::asio::dynamic_buffer(data), '\n',
	// 	                                                   boost::asio::experimental::use_coro);
	// 	co_yield std::string_view{ data.data(), size };
	// 	const auto start = std::chrono::high_resolution_clock::now();
	// 	data.erase(0, size);
	// 	std::println("asd took: {}", std::chrono::high_resolution_clock::now() - start);
	// }

	// char data[32 * 1024]{};
	// std::size_t pos = 0;
	// while (true) {
	// 	const std::size_t read =
	// 	  co_await async_read(pipe,buffer(data + pos, sizeof(data) - pos), experimental::use_coro);

	// 	auto ptr = std::memchr(data + pos, '\n', read);
	// 	pos += read;
	// 	if (ptr == nullptr && read != 0) {
	// 		continue;
	// 	}
	// 	if (ptr == nullptr) {
	// 		ptr = data + pos;
	// 	}

	// 	co_yield std::string_view{ data, static_cast<char*>(ptr) };

	// 	const std::size_t left = pos - (static_cast<char*>(ptr) - data);
	// 	std::memmove(data, ptr, left);
	// 	pos = left;
	// }
	// co_return co_await p.async_wait(boost::asio::experimental::use_coro);
}
