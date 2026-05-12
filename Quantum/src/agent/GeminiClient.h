#pragma once

#include "browser/IBrowserView.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace quantum {

struct AgentPlan {
    std::string response;
    std::string thought;
    std::vector<BrowserAction> actions;
    bool usedRemoteModel = false;
    std::string error;
    std::string memory;
};

struct GeminiImageAttachment {
    std::filesystem::path path;
    std::string label;
};

class GeminiClient {
public:
    GeminiClient();

    AgentPlan planFromSnapshot(
        const std::string& userTask,
        const std::string& snapshotJson,
        const std::string& currentUrl,
        const std::string& currentTitle) const;

    AgentPlan planFromPrompt(
        const std::string& prompt,
        const std::string& fallbackTask,
        const std::string& currentUrl,
        const std::vector<GeminiImageAttachment>& images = {}) const;

    std::string model() const;
    void setModel(const std::string& modelName);
    std::vector<std::string> availableModels() const;
    bool hasApiKey() const;
    void setApiKey(const std::string& key);
    std::string maskedApiKey() const;

private:
    AgentPlan localFallbackPlan(const std::string& userTask, const std::string& currentUrl) const;
    std::optional<std::string> postGenerateContent(
        const std::string& prompt,
        std::string* outError = nullptr,
        const std::vector<GeminiImageAttachment>& images = {}) const;
    AgentPlan parsePlanText(const std::string& text) const;

    std::string apiKey_;
    std::string model_;
};

} // namespace quantum
