#include "config.hpp"

#include <CLI/CLI.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/process/args.hpp>
#include <boost/process/search_path.hpp>
#include <print>
#include <regex>

using namespace yoink;

#define YOINK_YAML(name) value.name = get<decltype(value.name)>(node, #name)

namespace {

template<typename Type>
Type get(const YAML::Node& node, std::string_view key)
try {
	if (const auto& value = node[key]; value.IsDefined()) {
		return value.as<Type>();
	}
	return node["<<"][key].as<Type>();
} catch (const std::exception& e) {
	throw std::runtime_error{ std::format("{}: {}", key, e.what()) };
}

} // namespace

namespace YAML {

template<>
struct convert<std::chrono::minutes> {
	static bool decode(const Node& node, std::chrono::minutes& value)
	{
		try {
			value = std::chrono::minutes{ node.as<std::uint64_t>() };
			return true;
		} catch (const BadConversion&) {
		}

		static std::regex pattern{ "\\s*([1-9]\\d*|0)\\s*(mins|min|m|hours|hour|h|days|day|d)\\s*" };
		std::smatch match{};
		if (const auto str = node.as<std::string>(); std::regex_match(str, match, pattern)) {
			std::int64_t v = boost::lexical_cast<std::int64_t>(match[1].str());
			if (match[2] == "hours" || match[2] == "hour" || match[2] == "h") {
				v *= 60;
			} else if (match[2] == "days" || match[2] == "day" || match[2] == "d") {
				v *= 60 * 24;
			}
			value = std::chrono::minutes{ v };
			return true;
		}
		return false;
	}
};

template<>
struct convert<BanSettings> {
	static bool decode(const Node& node, BanSettings& value)
	{
		YOINK_YAML(ban_time);
		YOINK_YAML(search_window);
		YOINK_YAML(strikes);
		return true;
	}
};

template<>
struct convert<Process> {
	static bool decode(const Node& node, Process& value)
	{
		value.program = boost::process::search_path(node["program"].as<std::string>()).string();
		if (const auto& tmp = node["args"]; tmp.IsSequence()) {
			value.args = tmp.as<std::vector<std::string>>();
		} else if (tmp.IsDefined()) {
			CLI::App generic_app{};
			generic_app.add_option("x", value.args);
			generic_app.parse("-- " + tmp.as<std::string>());
		}
		return true;
	}
};

bool convert<Config>::decode(const Node& node, yoink::Config& value)
{
	YOINK_YAML(ban_settings);

	for (const auto& el : node["monitors"]) {
		Monitor monitor{ .name = el.first.as<std::string>() };
		monitor.inputs = get<decltype(monitor.inputs)>(el.second, "inputs");

		for (const auto& in : el.second["patterns"]) {
			monitor.patterns.push_back(Pattern{ in.as<std::string>() });
		}
		value.monitors.push_back(std::move(monitor));
	}

	return true;
}

} // namespace YAML
