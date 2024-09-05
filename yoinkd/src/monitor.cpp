#include "monitor.hpp"

#include "command.hpp"

#include <boost/asio/experimental/parallel_group.hpp>
#include <print>

using namespace boost::asio;
using namespace yoink;

namespace {

awaitable<void> do_monitor(const Monitor& monitor, const Process& process)
{
	auto generator = stream_process(co_await this_coro::executor, make_runnable(process, {}).value());
	while (generator.is_open()) {
		auto result = co_await generator.async_resume(use_awaitable);
		if (const auto ptr = std::get_if<int>(&result); ptr != nullptr) {
			std::println("Input finished with status {}", *ptr);
			break;
		}

		for (const auto& pattern : monitor.patterns) {
			const auto variables = pattern.match(std::get<std::string_view>(result));
			if (!variables.has_value()) {
				continue;
			}

			for (const auto& action : monitor.actions) {
				auto runnable = make_runnable(std::get<Process>(action), *variables);
				if (runnable.has_value()) {
					co_await yoink::process(*runnable);
				}
			}
		}
	}
}

} // namespace

awaitable<void> yoink::monitor(const Monitor& monitor)
{
	const auto executor = co_await this_coro::executor;

	std::vector<decltype(co_spawn(executor, std::declval<awaitable<void>>(), deferred))> coroutines{};
	for (const auto& input : monitor.inputs) {
		coroutines.push_back(co_spawn(executor, do_monitor(monitor, std::get<Process>(input)), deferred));
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
