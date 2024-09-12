#pragma once

#include "config.hpp"

#include <boost/asio/experimental/coro.hpp>
#include <string_view>

namespace yoink {

boost::asio::awaitable<int> process(Process process);
boost::asio::experimental::coro<std::string_view, int> stream_process(boost::asio::any_io_executor executor,
                                                                      Process process);

} // namespace yoink
