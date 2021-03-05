#pragma once

#include <functional>
#include <string>

/*
 * strtok for string_view
 */
void strtok(std::string_view s, std::string_view t,
    std::function<void(std::string_view)> fn);

/*
 * parse_options
 *
 * parse a string containing options and name=value pairs, for example:
 * a = b c=d e=f,g "h'i" = 'j"k' l= m n
 * will result in the following calls to fn
 * fn("a", "b")
 * fn("c", "d")
 * fn("e", "f,g")
 * fn("h'i", "j\"k"
 * fn("l", "m")
 * fn("n", "")
 */
int parse_options(std::string_view s,
    std::function<int(std::string_view, std::string_view)> fn);

/*
 * format a size as human readable
 */
char *hr_size_fmt(uintmax_t sz, char *buf, size_t bufsz);
