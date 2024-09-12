#pragma once

#include "config.hpp"

#include <boost/asio.hpp>
#include <boost/circular_buffer.hpp>
#include <chrono>
#include <map>

namespace yoink {

class Manager {
public:
	Manager(boost::asio::any_io_executor executor, BanSettings ban_settings);
	void strike(boost::asio::ip::address_v4 ip, std::chrono::utc_clock::time_point time,
	            std::size_t weight = 1);

private:
	using StrikeMap =
	  std::map<boost::asio::ip::address_v4, boost::circular_buffer<std::chrono::utc_clock::time_point>>;

	boost::asio::basic_waitable_timer<std::chrono::utc_clock> _timer;
	std::string _nft_path;
	BanSettings _ban_settings;
	/// All monitored IPs.
	StrikeMap _strikes{};
	/// All IPs of `_strikes` that are banned.
	std::vector<StrikeMap::const_iterator> _banned{};

	void _ban(StrikeMap::const_iterator it);
};

} // namespace yoink
