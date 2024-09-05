#pragma once

#include "pattern.hpp"

#include <chrono>
#include <variant>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace yoink {

struct VariableRef {
	std::string name;
};

struct Process {
	std::string program;
	std::vector<std::variant<std::string, VariableRef>> args;
};

using Command = std::variant<Process>;

struct Action {
	Command add;
	Command remove;
};

struct BanSettings {
	std::chrono::minutes ban_time;
	std::chrono::minutes search_window;
	std::size_t strike_count;
};

struct Monitor {
	std::string name;
	BanSettings ban_settings;
	std::vector<Pattern> patterns;
	std::vector<Command> inputs;
	std::vector<Action> actions;
};

struct Config {
	std::vector<Monitor> monitors;
};

YAML::Node merge_defaults(const YAML::Node& yaml);

} // namespace yoink

namespace YAML {

template<>
struct convert<yoink::Config> {
	static bool decode(const Node& node, yoink::Config& value);
};

} // namespace YAML
