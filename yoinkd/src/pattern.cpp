#include "pattern.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/scope_exit.hpp>
#include <regex>
#include <stdexcept>

namespace yoink {

namespace {

const std::map<std::string, std::string> variables = {
	{ "${IP4}",
	  R"((?<ip4>(\b25[0-5]|\b2[0-4][0-9]|\b[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3}))" },
	{ "${ISO}",
	  R"((?<years>[1-9][0-9]*)-(?<months>1[0-2]|0[1-9])-(?<days>3[01]|0[1-9]|[12][0-9])T(?<hours>2[0-3]|[01][0-9]):(?<minutes>[0-5][0-9]):(?<seconds>[0-5][0-9])(?:\.[0-9]+)?(?<timezone>Z|[+-](?:2[0-3]|[01][0-9]):[0-5][0-9])?)" },
};

void apply_variables(std::string& pattern)
{
	static std::regex regex{ R"(\$\{\w+\})", std::regex::optimize };

	for (std::size_t pos = 0; true;) {
		std::match_results<decltype(pattern.begin())> match{};
		if (!std::regex_search(pattern.begin() + pos, pattern.end(), match, regex)) {
			break;
		}

		if (const auto it = variables.find(match[0].str()); it != variables.end()) {
			pos = match[0].first - pattern.begin() + it->second.size();
			pattern.replace(match[0].first, match[0].second, it->second);
		} else {
			// Variable does not exist -> skip.
			pos = match[0].second - pattern.begin();
		}
	}
}

} // namespace

Pattern::Pattern(std::string pattern)
{
	apply_variables(pattern);

	int error          = 0;
	std::size_t offset = 0;
	_code =
	  pcre2_compile(reinterpret_cast<PCRE2_SPTR8>(pattern.data()), pattern.size(), 0, &error, &offset, nullptr);
	if (_code == nullptr) {
		throw std::invalid_argument{ "bad pattern" };
	}
	pcre2_jit_compile(_code, PCRE2_JIT_COMPLETE);
}

Pattern::Pattern(Pattern&& move) noexcept { std::swap(_code, move._code); }

Pattern::~Pattern()
{
	if (_code != nullptr) {
		pcre2_code_free(_code);
	}
}

std::optional<std::map<std::string, std::string>> Pattern::match(std::string_view data) const
{
	if (data == "") {
		return std::nullopt;
	}

	auto match = pcre2_match_data_create_from_pattern(_code, nullptr);
	BOOST_SCOPE_EXIT_ALL(&) { pcre2_match_data_free(match); };

	if (pcre2_match(_code, reinterpret_cast<PCRE2_SPTR8>(data.data()), data.size(), 0, 0, match, nullptr) < 0) {
		return std::nullopt;
	}

	std::map<std::string, std::string> variables{};
	std::uint32_t count = 0;
	pcre2_pattern_info(_code, PCRE2_INFO_NAMECOUNT, &count);
	if (count == 0) {
		return variables;
	}

	PCRE2_SPTR table_ptr = nullptr;
	pcre2_pattern_info(_code, PCRE2_INFO_NAMETABLE, &table_ptr);

	std::uint32_t table_entry_size = 0;
	pcre2_pattern_info(_code, PCRE2_INFO_NAMEENTRYSIZE, &table_entry_size);

	const std::uint32_t ovector_count = pcre2_get_ovector_count(match);
	const std::size_t* ovector        = pcre2_get_ovector_pointer(match);
	for (std::uint32_t i = 0; i < count; ++i) {
		const int n = table_ptr[0] << 8 | table_ptr[1];
		if (n < ovector_count) {
			const std::size_t start = ovector[n * 2];
			const std::size_t end   = ovector[n * 2 + 1];
			if (start != PCRE2_UNSET && end != PCRE2_UNSET && start <= end && end <= data.size()) {
				variables[reinterpret_cast<const char*>(table_ptr + 2)] = data.substr(start, end - start);
			}
		}
		table_ptr += table_entry_size;
	}
	return variables;
}

Pattern& Pattern::operator=(Pattern&& move) noexcept
{
	this->~Pattern();
	return *new (this) Pattern{ std::move(move) };
}

} // namespace yoink

std::chrono::utc_clock::time_point yoink::determine_time(const Variables& variables)
{
	using namespace std::chrono;

	const auto date = year_month_day{ year{ boost::lexical_cast<int>(variables.at("years")) },
		                                month{ boost::lexical_cast<unsigned int>(variables.at("months")) },
		                                day{ boost::lexical_cast<unsigned int>(variables.at("days")) } };
	const seconds time{ hours{ boost::lexical_cast<hours::rep>(variables.at("hours")) } +
		                  minutes{ boost::lexical_cast<minutes::rep>(variables.at("minutes")) } +
		                  seconds{ boost::lexical_cast<seconds::rep>(variables.at("seconds")) } };
	minutes timezone{};
	if (const auto it = variables.find("timezone"); it != variables.end() && it->second != "Z") {
		const bool negative = it->second.at(0) == '-';
		timezone            = hours{ boost::lexical_cast<hours::rep>(it->second.substr(1, 2)) } +
		           minutes{ boost::lexical_cast<hours::rep>(it->second.substr(4, 2)) };
	}

	return utc_clock::from_sys(sys_days{ date } + time - timezone);
}
