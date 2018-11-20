#include <string_utils.h>

#include <array>

void strtok(std::string_view s, std::string_view t,
    std::function<void(std::string_view)> fn)
{
	if (s.empty())
		return;

	auto start = s.find_first_not_of(t, 0);
	while (start != std::string_view::npos) {
		const auto p = s.find_first_of(t, start);
		if (p == std::string_view::npos) {
			fn(s.substr(start, p));
			return;
		} else {
			fn(s.substr(start, p - start));
			start = s.find_first_not_of(t, p);
		}
	}
}

int parse_options(std::string_view s,
    std::function<int(std::string_view, std::string_view)> fn)
{
	if (s.empty())
		return 0;

	size_t b[2]{};
	size_t e[2]{};
	int state = 0;
	auto flush = [&]{
		if (b[0] == e[0])
			return 0;
		const auto r = fn(s.substr(b[0], e[0] - b[0]),
		    s.substr(b[1], e[1] - b[1]));
		state = 0;
		b[0] = e[0] = b[1] = e[1] = 0;
		return r;
	};
	auto space = [](const char c) {
		return c == ' ' || (unsigned)c - '\t' < 5;
	};
	for (size_t i = 0; i != s.size(); ++i) {
		for (; space(s[i]) && i != s.size(); ++i);
		if (s[i] == '=') {
			state = 1;
			continue;
		}
		if (b[state] != e[state])
			if (auto r = flush(); r)
				return r;
		int sep;
		switch (s[i]) {
		case '\'':
		case '"':
			if (i + 1 == s.size())
				break;
			sep = s[i];
			b[state] = ++i;
			for (; i != s.size() && s[i] != sep; ++i);
			e[state] = i;
			break;
		default:
			b[state] = i;
			for (; i != s.size() && !space(s[i]) && s[i] != '='; ++i);
			e[state] = i;
			--i;
			break;
		}
	}
	return flush();
}
