#include "agent/AgentMemory.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace quantum {

void AgentMemory::remember(std::string kind, std::string text) {
    if (text.empty()) {
        return;
    }
    items_.push_back({std::move(kind), std::move(text), nowUnix()});
    if (items_.size() > 200) {
        items_.erase(items_.begin(), items_.begin() + static_cast<long long>(items_.size() - 200));
    }
}

std::vector<AgentMemoryItem> AgentMemory::recent(int limit) const {
    std::vector<AgentMemoryItem> result;
    const int count = std::min(limit, static_cast<int>(items_.size()));
    result.reserve(static_cast<size_t>(count));
    for (int i = static_cast<int>(items_.size()) - count; i < static_cast<int>(items_.size()); ++i) {
        if (i >= 0) {
            result.push_back(items_[static_cast<size_t>(i)]);
        }
    }
    return result;
}

std::string AgentMemory::summarizeForPrompt(int limit) const {
    std::ostringstream out;
    for (const auto& item : recent(limit)) {
        out << "- [" << item.kind << "] " << item.text << "\n";
    }
    return out.str();
}

void AgentMemory::clear() {
    items_.clear();
}

std::int64_t AgentMemory::nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace quantum
