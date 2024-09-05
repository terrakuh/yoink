#pragma once

#include "config.hpp"

#include <boost/asio.hpp>

namespace yoink {

boost::asio::awaitable<void> monitor(const Monitor& monitor);

}
