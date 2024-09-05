#pragma once

#include <map>
#include <optional>
#include <pcre2.h>
#include <string>
#include <string_view>
#include <vector>

namespace yoink {

using Variables = std::map<std::string, std::string>;

class Pattern {
public:
	/// Compiles the pattern after replacing the given variables in the pattern.
	Pattern(const Variables& variables, std::string pattern);
	Pattern(const Pattern& copy) = delete;
	Pattern(Pattern&& move) noexcept;
	~Pattern();

	/// Matches the string and returns the captured variables. If the pattern does not match, `std::nullopt` is
	/// returned.
	std::optional<Variables> match(std::string_view data) const;
	const std::vector<std::string>& variables() const;

	Pattern& operator=(const Pattern& copy) = delete;
	Pattern& operator=(Pattern&& move) noexcept;

private:
	pcre2_code* _code = nullptr;
};

} // namespace yoink
