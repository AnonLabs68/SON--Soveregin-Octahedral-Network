#include "agent/ToolRegistry.h"

#include <sstream>
#include <utility>

namespace quantum {

void ToolRegistry::registerTool(ToolDefinition definition, ToolHandler handler) {
    definitions_.push_back(definition);
    slots_.push_back({std::move(definition), std::move(handler)});
}

const std::vector<ToolDefinition>& ToolRegistry::definitions() const {
    return definitions_;
}

std::string ToolRegistry::describeForPrompt() const {
    std::ostringstream out;
    for (const auto& definition : definitions_) {
        out << "- " << definition.name << ": " << definition.description << "\n";
        out << "  schema: " << definition.schema << "\n";
    }
    return out.str();
}

bool ToolRegistry::execute(const BrowserAction& action, IBrowserView& browser) const {
    const char* name = "wait";
    switch (action.type) {
    case BrowserAction::Type::Navigate: name = "navigate"; break;
    case BrowserAction::Type::Click: name = "click"; break;
    case BrowserAction::Type::TypeText: name = "type"; break;
    case BrowserAction::Type::FillForm: name = "fill_form"; break;
    case BrowserAction::Type::PressEnter: name = "press_enter"; break;
    case BrowserAction::Type::Wait: name = "wait"; break;
    }

    for (const auto& slot : slots_) {
        if (slot.definition.name == name) {
            slot.handler(action, browser);
            return true;
        }
    }
    return false;
}

} // namespace quantum
