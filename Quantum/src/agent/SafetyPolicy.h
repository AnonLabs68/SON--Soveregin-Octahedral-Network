#pragma once

#include "browser/IBrowserView.h"

#include <string>

namespace quantum {

struct SafetyDecision {
    bool allowed = true;
    bool requiresUser = false;
    std::string reason;
};

class SafetyPolicy {
public:
    SafetyDecision assess(const BrowserAction& action, const std::string& currentUrl) const;

private:
    SafetyDecision assessNavigation(const BrowserAction& action) const;
    SafetyDecision assessInput(const BrowserAction& action, const std::string& currentUrl) const;
};

} // namespace quantum

