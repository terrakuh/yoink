#include "pattern.hpp"

#include <boost/scope_exit.hpp>
#include <regex>
#include <stdexcept>

namespace yoink {

namespace {

void apply_variables(const Variables& variables, std::string& pattern)
{
	static std::regex regex{ R"(\$\{(\w+)\})", std::regex::optimize };

	for (std::size_t pos = 0; true;) {
		std::match_results<decltype(pattern.begin())> match{};
		if (!std::regex_search(pattern.begin() + pos, pattern.end(), match, regex)) {
			break;
		}

		const auto name = match[1].str();
		if (const auto it = variables.find(name); it != variables.end()) {
			const auto replacement = std::format("(?P<{}>{})", name, it->second);
			pos                    = match[0].first - pattern.begin() + replacement.size();
			pattern.replace(match[0].first, match[0].second, replacement);
		} else {
			// Variable does not exist -> skip.
			pos = match[0].second - pattern.begin();
		}
	}
}

} // namespace

Pattern::Pattern(const Variables& variables, std::string pattern)
{
	apply_variables(variables, pattern);

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
				variables[reinterpret_cast<const char*>(table_ptr + 2)] = data.substr(start, end);
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
