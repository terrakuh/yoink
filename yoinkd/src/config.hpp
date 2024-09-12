#pragma once

#include "pattern.hpp"

#include <chrono>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace yoink {

struct Process {
	std::string program;
	std::vector<std::string> args;
};

struct BanSettings {
	std::chrono::minutes ban_time;
	std::chrono::minutes search_window;
	std::size_t strikes;
};

struct Monitor {
	std::string name;
	std::vector<Pattern> patterns;
	std::vector<Process> inputs;
};

struct Config {
	BanSettings ban_settings;
	std::vector<Monitor> monitors;
};

} // namespace yoink

namespace YAML {

template<>
struct convert<yoink::Config> {
	static bool decode(const Node& node, yoink::Config& value);
};

} // namespace YAML
