#include "agent/SafetyPolicy.h"
#include "security/SecurityCortex.h"
#include "core/StringUtils.h"

#include <array>

namespace quantum {

namespace {
    SecurityCortex& getCortex() {
        static SecurityCortex cortex;
        return cortex;
    }
}

SafetyDecision SafetyPolicy::assess(const BrowserAction& action, const std::string& currentUrl) const {
    // Perform Real-Time Bloom Filter Check on the current page context
    auto report = getCortex().assessUrl(currentUrl);
    if (report.level == SecurityLevel::Malicious) {
        return {false, false, "CRITICAL: " + report.reason};
    }

    switch (action.type) {
    case BrowserAction::Type::Navigate:
        return assessNavigation(action);
    case BrowserAction::Type::TypeText:
    case BrowserAction::Type::FillForm:
        return assessInput(action, currentUrl);
    case BrowserAction::Type::Click:
    case BrowserAction::Type::PressEnter:
    case BrowserAction::Type::Wait:
        return {};
    }
    return {};
}

SafetyDecision SafetyPolicy::assessNavigation(const BrowserAction& action) const {
    const std::string url = lower(trim(action.url));
    
    // Bloom Filter URL Check
    auto report = getCortex().assessUrl(url);
    if (report.level != SecurityLevel::Safe) {
        return {false, report.level == SecurityLevel::Suspicious, report.reason};
    }

    if (url.rfind("javascript:", 0) == 0) {
        return {false, false, "Blocked javascript: navigation"};
    }
    if (url.rfind("file:", 0) == 0) {
        return {false, true, "File navigation requires explicit user confirmation"};
    }
    if (url.rfind("data:", 0) == 0) {
        return {false, true, "Data URL navigation requires explicit user confirmation"};
    }
    return {};
}

SafetyDecision SafetyPolicy::assessInput(const BrowserAction& action, const std::string&) const {
    static constexpr std::array<const char*, 8> sensitiveWords = {
        "password",
        "passcode",
        "otp",
        "2fa",
        "ssn",
        "social-security",
        "credit-card",
        "cardnumber"
    };

    std::string target = lower(action.selector + " " + action.value);
    for (const auto* word : sensitiveWords) {
        if (target.find(word) != std::string::npos) {
            return {false, true, std::string("Sensitive form input requires user confirmation: ") + word};
        }
    }
    for (const auto& field : action.fields) {
        target = lower(field.selector + " " + field.value);
        for (const auto* word : sensitiveWords) {
            if (target.find(word) != std::string::npos) {
                return {false, true, std::string("Sensitive form field requires user confirmation: ") + word};
            }
        }
    }
    return {};
}

} // namespace quantum

