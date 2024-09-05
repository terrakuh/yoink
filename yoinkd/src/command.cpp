#include "command.hpp"

#include <boost/asio.hpp>
#include <boost/asio/experimental/use_coro.hpp>
#include <boost/process/v2.hpp>

std::optional<yoink::RunnableProcess> yoink::make_runnable(const Process& process, const Variables& variables)
{
	RunnableProcess runnable{};
	runnable.args.reserve(process.args.size());
	for (const auto& arg : process.args) {
		if (const auto ptr = std::get_if<std::string>(&arg); ptr != nullptr) {
			runnable.args.push_back(*ptr);
			continue;
		}

		const auto it = variables.find(std::get<VariableRef>(arg).name);
		if (it == variables.end()) {
			return std::nullopt;
		}
		runnable.args.push_back(it->second);
	}
	return std::move(runnable);
}

boost::asio::awaitable<int> yoink::process(const RunnableProcess& process)
{
	boost::process::v2::process p{ co_await boost::asio::this_coro::executor, process.program, process.args,
		                             boost::process::v2::process_stdio{ .out = nullptr } };
	co_return co_await boost::process::v2::async_execute(std::move(p), boost::asio::use_awaitable);
}

boost::asio::experimental::coro<std::string_view, int>
  yoink::stream_process(boost::asio::any_io_executor executor, RunnableProcess process)
{
	boost::asio::readable_pipe pipe{ executor };
	boost::process::v2::process p{ executor, process.program, process.args,
		                             boost::process::v2::process_stdio{ .out = pipe } };

	std::string data{};
	while (true) {
		const std::size_t size = co_await async_read_until(pipe, boost::asio::dynamic_buffer(data), '\n',
		                                                   boost::asio::experimental::use_coro);
		co_yield std::string_view{ data.data(), size };
		data.erase(0, size);
	}

	co_return co_await p.async_wait(boost::asio::experimental::use_coro);
}
