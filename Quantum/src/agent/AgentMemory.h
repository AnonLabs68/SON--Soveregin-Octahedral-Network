#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace quantum {

struct AgentMemoryItem {
    std::string kind;
    std::string text;
    std::int64_t createdUnix = 0;
};

class AgentMemory {
public:
    void remember(std::string kind, std::string text);
    std::vector<AgentMemoryItem> recent(int limit = 12) const;
    std::string summarizeForPrompt(int limit = 8) const;
    void clear();

private:
    static std::int64_t nowUnix();

    std::vector<AgentMemoryItem> items_;
};

} // namespace quantum

