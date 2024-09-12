#include "manager.hpp"

#include "command.hpp"

#include <algorithm>
#include <boost/process/search_path.hpp>
#include <cassert>
#include <print>

namespace yoink {

Manager::Manager(boost::asio::any_io_executor executor, BanSettings ban_settings)
    : _timer{ std::move(executor) }, _nft_path{ boost::process::search_path("nft").string() },
      _ban_settings{ ban_settings }
{
	_ban_settings.strikes = std::max(_ban_settings.strikes, 1zu);
}

void Manager::strike(boost::asio::ip::address_v4 ip, std::chrono::utc_clock::time_point time,
                     std::size_t weight)
{
	// 127.0.0.0/8
	if (const std::uint32_t addr = ip.to_uint(); (addr & 0xff'00'00'00) == 127 << 24) {
		return;
	} else if ((addr & 0xff'ff'ff'00) == (192 << 24 | 168 << 16 | 178 << 8)) {
		return;
	}

	// Not in search window.
	if (time + _ban_settings.search_window < std::chrono::utc_clock::now()) {
		std::println("Out of window: {}", ip.to_string());
		return;
	}
	time += _ban_settings.ban_time;

	auto it = _strikes.lower_bound(ip);
	// IP is not monitored yet.
	if (it == _strikes.end() || it->first != ip) {
		it = _strikes.insert(it, { ip, StrikeMap::mapped_type{ _ban_settings.strikes } });
	}

	// IP is already banned.
	const std::size_t before_size = it->second.size();
	for (std::size_t i = 0; i < std::min(weight, _ban_settings.strikes) || i == 0; ++i) {
		it->second.push_back(time);
	}
	if (before_size >= _ban_settings.strikes) {
		return;
	}

	// Ban now.
	if (it->second.size() >= _ban_settings.strikes) {
		_ban(std::move(it));
	}
}

void Manager::_ban(StrikeMap::const_iterator it)
{
	using namespace boost::asio;

	std::println("Banning {}", it->first.to_string());

	co_spawn(_timer.get_executor(),
	         process(Process{
	           .program = _nft_path,
	           .args    = { "add", "element", "inet", "yoinkd", "blacklist4", "{", it->first.to_string(), "}" },
	         }),
	         detached);

	_banned.push_back(it);
	std::ranges::push_heap(_banned, {}, [](const auto& it) {
		assert(!it->second.empty());
		return it->second.back();
	});

	_timer.expires_at(_banned.front()->second.back());
	_timer.async_wait([this](this auto self, const auto& ec) {
		if (ec) {
			return;
		}
		// The timeout was extended.
		if (const auto time = _banned.front()->second.back(); time > std::chrono::utc_clock::now()) {
			_timer.expires_at(time);
			_timer.async_wait(self);
			return;
		}

		std::ranges::pop_heap(_banned, {}, [](const auto& it) {
			assert(!it->second.empty());
			return it->second.back();
		});

		const auto it = _banned.back();
		_banned.pop_back();

		co_spawn(
		  _timer.get_executor(),
		  process(Process{
		    .program = _nft_path,
		    .args    = { "delete", "element", "inet", "yoinkd", "blacklist4", "{", it->first.to_string(), "}" },
		  }),
		  detached);

		_strikes.erase(it);
	});
}

} // namespace yoink
