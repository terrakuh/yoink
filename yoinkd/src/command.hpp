#pragma once

#include "config.hpp"

#include <boost/asio/experimental/coro.hpp>
#include <string_view>

namespace yoink {

struct RunnableProcess {
	std::string program;
	std::vector<std::string> args;
};

std::optional<RunnableProcess> make_runnable(const Process& process, const Variables& variables);

boost::asio::awaitable<int> process(const RunnableProcess& process);
boost::asio::experimental::coro<std::string_view, int> stream_process(boost::asio::any_io_executor executor,
                                                                      RunnableProcess process);

} // namespace yoink
