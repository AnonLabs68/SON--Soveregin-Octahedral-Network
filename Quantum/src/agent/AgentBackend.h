#pragma once

#include "agent/AgentMemory.h"
#include "agent/AgentTrace.h"
#include "agent/GeminiClient.h"
#include "agent/SafetyPolicy.h"
#include "agent/ToolRegistry.h"
#include "browser/IBrowserView.h"

#include <chrono>
#include <filesystem>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace quantum {

struct ChatMessage {
    enum class Author {
        User,
        Assistant,
        System
    };

    Author author = Author::Assistant;
    std::string text;
};

struct AgentStep {
    std::string label;
    bool done = false;
};

class AgentBackend {
public:
    AgentBackend();

    void submit(const std::string& task, IBrowserView& browser);
    void tick(IBrowserView& browser);
    void stop();

    bool busy() const;
    bool running() const;
    std::string modelName() const;
    void setModel(const std::string& modelName);
    std::vector<std::string> availableModels() const;
    const std::vector<ChatMessage>& messages() const;
    const std::vector<AgentStep>& steps() const;
    const AgentTrace& trace() const;
    void setApiKey(const std::string& key);
    std::string maskedApiKey() const;
    std::string activeTaskId() const;
    std::vector<AgentMemoryItem> recentMemory(int limit = 12) const;
    bool attachImage(const std::filesystem::path& path, const std::string& label);
    void clearPendingImages();
    std::vector<std::string> pendingImageLabels() const;

    struct WaitProgress {
        float progress = 0.0f; // 0.0 to 1.0
        bool active = false;
    };
    WaitProgress waitProgress() const;

private:
    void requestSnapshot(IBrowserView& browser);
    void beginRemotePlan(const std::string& task, const std::string& snapshot, IBrowserView& browser);
    void executeAction(const BrowserAction& action, IBrowserView& browser);
    bool pageIsSettling(const IBrowserView& browser) const;
    void scheduleSnapshotWait(double seconds, const std::string& label);

    GeminiClient client_;
    AgentMemory memory_;
    AgentTrace trace_;
    SafetyPolicy safety_;
    ToolRegistry tools_;
    std::vector<ChatMessage> messages_;
    std::vector<AgentStep> steps_;
    std::future<AgentPlan> planFuture_;
    std::vector<BrowserAction> pendingActions_;
    std::vector<GeminiImageAttachment> pendingImages_;
    std::vector<GeminiImageAttachment> activeImages_;
    std::string activeTask_;
    std::string activeTaskId_;
    int taskSerial_ = 0;
    bool waitingForSnapshot_ = false;
    bool awaitingPageSettle_ = false;
    bool stopped_ = false;
    int cyclesRemaining_ = 0;
    std::chrono::steady_clock::time_point nextActionAt_{};
    std::chrono::steady_clock::time_point nextSnapshotAt_{};
    std::chrono::steady_clock::time_point waitStartedAt_{};
    double waitDurationTotal_ = 0.0;
    std::string lastResponse_;
};

} // namespace quantum
