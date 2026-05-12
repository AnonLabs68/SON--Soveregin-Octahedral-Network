#pragma once

#include "agent/AgentMemory.h"
#include "agent/ToolRegistry.h"

#include <string>

namespace quantum {

struct PromptContext {
    std::string userTask;
    std::string snapshotJson;
    std::string currentUrl;
    std::string currentTitle;
    std::string memory;
    std::string toolDescriptions;
};

class PromptBuilder {
public:
    static std::string buildAgentPrompt(const PromptContext& context);
};

} // namespace quantum

