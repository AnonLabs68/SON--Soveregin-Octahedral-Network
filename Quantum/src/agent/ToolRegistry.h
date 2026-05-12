#pragma once

#include "browser/IBrowserView.h"

#include <functional>
#include <string>
#include <vector>

namespace quantum {

struct ToolDefinition {
    std::string name;
    std::string description;
    std::string schema;
};

class ToolRegistry {
public:
    using ToolHandler = std::function<void(const BrowserAction&, IBrowserView&)>;

    void registerTool(ToolDefinition definition, ToolHandler handler);
    const std::vector<ToolDefinition>& definitions() const;
    std::string describeForPrompt() const;
    bool execute(const BrowserAction& action, IBrowserView& browser) const;

private:
    struct ToolSlot {
        ToolDefinition definition;
        ToolHandler handler;
    };

    std::vector<ToolSlot> slots_;
    std::vector<ToolDefinition> definitions_;
};

ToolRegistry createBrowserToolRegistry();

} // namespace quantum

