#include "monitor.hpp"

#include "command.hpp"

#include <boost/asio/experimental/parallel_group.hpp>
#include <print>

using namespace boost::asio;
using namespace yoink;

namespace {

awaitable<void> do_monitor(Manager& manager, const Monitor& monitor, const Process& process)
try {
	auto generator = stream_process(co_await this_coro::executor, process);
	while (generator.is_open()) {
			const auto start     = std::chrono::high_resolution_clock::now();
		auto result = co_await generator.async_resume(use_awaitable);
			std::println("getting took: {}", std::chrono::high_resolution_clock::now() - start);
		if (const auto ptr = std::get_if<int>(&result); ptr != nullptr) {
			std::println("Input finished with status {}", *ptr);
			break;
		}

		for (const auto& pattern : monitor.patterns) {
			const auto variables = pattern.match(std::get<std::string_view>(result));
			if (!variables.has_value()) {
				continue;
			}

			const auto time = determine_time(*variables);
			manager.strike(ip::address_v4::from_string(variables->at("ip4")), time);
		}
	}
} catch (const std::exception& e) {
	std::println("do_monitor() failed: {}", e.what());
}

} // namespace

awaitable<void> yoink::monitor(Manager& manager, const Monitor& monitor)
{
	const auto executor = co_await this_coro::executor;

	std::vector<decltype(co_spawn(executor, std::declval<awaitable<void>>(), deferred))> coroutines{};
	for (const auto& input : monitor.inputs) {
		coroutines.push_back(co_spawn(executor, do_monitor(manager, monitor, input), deferred));
	}

	const auto& [order, exceptions] = co_await experimental::make_parallel_group(std::move(coroutines))
	                                    .async_wait(experimental::wait_for_all{}, use_awaitable);
	for (const auto& ex : exceptions) {
		if (ex != nullptr) {
			try {
				std::rethrow_exception(ex);
			} catch (const std::exception& e) {
				std::println("Input failed with: {}", e.what());
			}
		}
	}
}
