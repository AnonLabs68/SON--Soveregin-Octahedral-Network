#include "core/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace quantum {

std::string trim(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return ch > ' ';
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return ch > ' ';
    }).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool containsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

std::vector<std::string> splitWords(const std::string& value) {
    std::vector<std::string> words;
    std::istringstream in(value);
    std::string word;
    while (in >> word) {
        words.push_back(lower(word));
    }
    return words;
}

std::string urlEncode(const std::string& value) {
    std::ostringstream out;
    out << std::hex << std::uppercase;
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out << static_cast<char>(ch);
        } else if (ch == ' ') {
            out << '+';
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return out.str();
}

std::string replaceAll(std::string value, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return value;
    }
    size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
    return value;
}

} // namespace quantum

