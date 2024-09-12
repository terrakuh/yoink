#pragma once

#include "config.hpp"
#include "manager.hpp"

#include <boost/asio.hpp>

namespace yoink {

boost::asio::awaitable<void> monitor(Manager& manager, const Monitor& monitor);

}
