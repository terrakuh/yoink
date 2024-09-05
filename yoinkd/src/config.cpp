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
} catch (...) {
	std::throw_with_nested(std::runtime_error{ std::format("error on key: {}", key) });
}

void parse(const YAML::Node& node, Command& v, const Variables& variables)
{
	if (const auto type = get<std::string>(node, "type"); type == "process") {
		auto& value   = v.emplace<Process>();
		value.program = boost::process::search_path(get<std::string>(node, "program")).string();
		for (auto& el : get<std::vector<std::string>>(node, "args")) {
			if (el.starts_with("${") && el.ends_with("}")) {
				el.pop_back();
				el.erase(0, 2);

				if (variables.contains(el)) {
					value.args.push_back(VariableRef{ std::move(el) });
				} else {
					std::println("Variable ref '{}' does not exist; ignoring", el);
					value.args.push_back(std::format("${{{}}}", el));
				}
			} else {
				value.args.push_back(std::move(el));
			}
		}
	} else {
		throw std::runtime_error{ std::format("unsupported type '{}'", type) };
	}
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
		YOINK_YAML(strike_count);
		return true;
	}
};

// template<>
// struct convert<Process> {
// 	static bool decode(const Node& node, Process& value)
// 	{
// 		value.program = boost::process::search_path(node["program"].as<std::string>()).string();
// 		if (const auto& tmp = node["args"]; tmp.IsSequence()) {
// 			value.args = tmp.as<std::vector<std::string>>();
// 		} else if (tmp.IsDefined()) {
// 			CLI::App generic_app{};
// 			generic_app.add_option("x", value.args);
// 			generic_app.parse("-- " + tmp.as<std::string>());
// 		}
// 		return true;
// 	}
// };

bool convert<Config>::decode(const Node& node, yoink::Config& value)
{
	const auto variables = get<Variables>(node, "variables");

	for (const auto& el : node["monitors"]) {
		Monitor monitor{
			.name         = get<std::string>(el, "name"),
			.ban_settings = get<BanSettings>(el, "ban_settings"),
		};

		for (const auto& pattern : el["patterns"]) {
			monitor.patterns.push_back(Pattern{ variables, pattern.as<std::string>() });
		}
		for (const auto& command : el["inputs"]) {
			parse(command, monitor.inputs.emplace_back(), {});
		}
		for (const auto& action_el : el["actions"]) {
			Action action{};
			parse(action_el["add"], action.add, variables);
			parse(action_el["remove"], action.remove, variables);
			
		}
	}

	return true;
}

} // namespace YAML

YAML::Node yoink::merge_defaults(const YAML::Node& node)
{
	switch (node.Type()) {
	case YAML::NodeType::Undefined:
	case YAML::NodeType::Null:
	case YAML::NodeType::Scalar: return node;
	case YAML::NodeType::Sequence: {
		YAML::Node n{};
		for (const auto& el : node) {
			n.push_back(merge_defaults(el));
		}
		return n;
	}
	case YAML::NodeType::Map: {
		YAML::Node n{};
		if (const auto& v = node["<<"]; v.IsDefined()) {
			n = v;
		}
		for (const auto& el : node) {
			if (el.first.as<std::string>() != "<<") {
				n[el.first] = merge_defaults(el.second);
			}
		}
		return n;
	}
	}
	throw std::logic_error{ "bad node type" };
}
