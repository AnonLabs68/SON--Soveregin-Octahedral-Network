#pragma once

#include <string>
#include <vector>

namespace quantum {

std::string trim(std::string value);
std::string lower(std::string value);
bool containsCaseInsensitive(const std::string& haystack, const std::string& needle);
std::vector<std::string> splitWords(const std::string& value);
std::string urlEncode(const std::string& value);
std::string replaceAll(std::string value, const std::string& from, const std::string& to);

} // namespace quantum

