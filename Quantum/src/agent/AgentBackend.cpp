#include "agent/AgentBackend.h"

#include "agent/AxTreeSnapshot.h"
#include "agent/BrowserTools.h"
#include "agent/PromptBuilder.h"
#include "browser/PageSnapshot.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>

namespace quantum {

namespace {

const char* actionName(BrowserAction::Type type) {
    switch (type) {
    case BrowserAction::Type::Navigate: return "navigate";
    case BrowserAction::Type::Click: return "click";
    case BrowserAction::Type::TypeText: return "type";
    case BrowserAction::Type::FillForm: return "fill_form";
    case BrowserAction::Type::PressEnter: return "press_enter";
    case BrowserAction::Type::Wait: return "wait";
    }
    return "action";
}

bool isSupportedImagePath(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".gif";
}

} // namespace

AgentBackend::AgentBackend()
    : tools_(createBrowserToolRegistry()) {
    messages_.push_back({
        ChatMessage::Author::Assistant,
        "Quantum Assistant online. I can read the page, plan actions, and drive the browser."
    });
    steps_.push_back({"Idle", true});
    trace_.record(TraceKind::System, "Agent online", client_.model());
}

void AgentBackend::submit(const std::string& task, IBrowserView& browser) {
    if (task.empty() || waitingForSnapshot_ || planFuture_.valid() || !pendingActions_.empty()) {
        return;
    }

    stopped_ = false;
    activeTask_ = task;
    char taskId[32];
    std::snprintf(taskId, sizeof(taskId), "Q-%04d", ++taskSerial_);
    activeTaskId_ = taskId;
    cyclesRemaining_ = 8;
    awaitingPageSettle_ = false;
    waitingForSnapshot_ = false;
    pendingActions_.clear();
    activeImages_ = std::move(pendingImages_);
    pendingImages_.clear();
    lastResponse_.clear();
    messages_.push_back({ChatMessage::Author::User, "[" + activeTaskId_ + "] " + task});
    trace_.record(TraceKind::System, "Task submitted", activeTaskId_ + " " + task);
    steps_ = {
        {activeTaskId_ + " capture browser state", false},
        {activeTaskId_ + " plan exact action batch", false},
        {activeTaskId_ + " execute and verify", false}
    };

    if (pageIsSettling(browser)) {
        scheduleSnapshotWait(2.5, activeTaskId_ + " wait for page settle");
    } else {
        requestSnapshot(browser);
    }
}

void AgentBackend::tick(IBrowserView& browser) {
    browser.setAgentBusy(busy());

    auto now = std::chrono::steady_clock::now();
    if (awaitingPageSettle_ && !stopped_) {
        if (now < nextSnapshotAt_) {
            return;
        }
        
        if (waitingForSnapshot_ || planFuture_.valid()) {
            return;
        }
        
        if (pageIsSettling(browser)) {
            scheduleSnapshotWait(1.5, activeTaskId_.empty() ? "Wait for page settle" : activeTaskId_ + " wait for page settle");
            return;
        }

        awaitingPageSettle_ = false;
        requestSnapshot(browser);
    }

    if (planFuture_.valid() &&
        planFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        AgentPlan plan = planFuture_.get();
        waitingForSnapshot_ = false;
        if (stopped_) {
            return;
        }
        if (steps_.size() > 1) {
            steps_[1].done = true;
        }

        if (!plan.error.empty()) {
            messages_.push_back({ChatMessage::Author::System, plan.error});
            trace_.record(TraceKind::Error, "Planner fallback", plan.error);
        }
        
        if (!plan.thought.empty()) {
            trace_.record(TraceKind::Model, "Thought", plan.thought);
        }

        // MISSION ID GOVERNOR: Deduplicate repetitive AI pulses
        bool hasActions = !plan.actions.empty();
        
        // We only show the response if:
        // 1. It's different from the last one.
        // 2. It's NOT empty.
        // 3. Either it's the final answer (no actions) OR it's a significant enough update.
        if (!plan.response.empty() && plan.response != lastResponse_) {
            if (!hasActions || plan.response.length() > 20) { 
                messages_.push_back({ChatMessage::Author::Assistant, plan.response});
                lastResponse_ = plan.response;
            }
        }

        if (!plan.memory.empty()) {
            memory_.remember("model", plan.memory);
            trace_.record(TraceKind::Memory, "Remembered", plan.memory);
        }
        pendingActions_ = std::move(plan.actions);
        trace_.record(TraceKind::Model, "Plan received", std::to_string(pendingActions_.size()) + " actions");
        nextActionAt_ = std::chrono::steady_clock::now();
        
        if (pendingActions_.empty()) {
            // CONVERSATIONAL SENTINEL: Task is complete if no actions
            activeTask_.clear();
            cyclesRemaining_ = 0;
            activeTaskId_.clear();
            if (steps_.size() > 2) {
                steps_[2].done = true;
            }
            steps_ = {{"Ready", true}};
        }
    }

    if (stopped_) {
        pendingActions_.clear();
        waitingForSnapshot_ = false;
        if (planFuture_.valid() &&
            planFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            planFuture_.get();
        }
        return;
    }

    if (!pendingActions_.empty() && std::chrono::steady_clock::now() >= nextActionAt_) {
        BrowserAction action = pendingActions_.front();
        pendingActions_.erase(pendingActions_.begin());
        
        // AUTOMATIC ACTION MEMORY: Ensure the AI knows what it just did
        std::string actionDesc = actionName(action.type);
        if (!action.url.empty()) actionDesc += " " + action.url;
        if (!action.selector.empty()) actionDesc += " on " + action.selector;
        if (!action.value.empty()) actionDesc += " with '" + action.value + "'";
        memory_.remember("action", actionDesc);
        
        executeAction(action, browser);
        nextActionAt_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(450);
        if (pendingActions_.empty() && steps_.size() > 2) {
            steps_[2].done = true;
            if (!activeTask_.empty() && cyclesRemaining_ > 0) {
                --cyclesRemaining_;
                scheduleSnapshotWait(2.0, activeTaskId_ + " wait for browser settle");
                steps_ = {
                    {activeTaskId_ + " observe updated page", false},
                    {activeTaskId_ + " re-plan from fresh snapshot", false},
                    {activeTaskId_ + " execute next action batch", false}
                };
            } else {
                activeTask_.clear();
                activeTaskId_.clear();
                steps_ = {{"Ready", true}};
            }
        }
    }
}

void AgentBackend::stop() {
    stopped_ = true;
    pendingActions_.clear();
    waitingForSnapshot_ = false;
    awaitingPageSettle_ = false;
    activeTask_.clear();
    activeTaskId_.clear();
    cyclesRemaining_ = 0;
    steps_ = {{"Ready", true}};
    messages_.push_back({ChatMessage::Author::System, "Agent stopped."});
    trace_.record(TraceKind::System, "Agent stopped");
}

bool AgentBackend::busy() const {
    return !stopped_ && (waitingForSnapshot_ || awaitingPageSettle_ || planFuture_.valid() || !pendingActions_.empty());
}

bool AgentBackend::running() const {
    return busy() || !activeTask_.empty();
}

std::string AgentBackend::modelName() const {
    return client_.model();
}

void AgentBackend::setModel(const std::string& modelName) {
    client_.setModel(modelName);
    messages_.push_back({ChatMessage::Author::System, "Quantum arsenal rotated to: " + modelName});
}

std::vector<std::string> AgentBackend::availableModels() const {
    return client_.availableModels();
}

const std::vector<ChatMessage>& AgentBackend::messages() const {
    return messages_;
}

const std::vector<AgentStep>& AgentBackend::steps() const {
    return steps_;
}

const AgentTrace& AgentBackend::trace() const {
    return trace_;
}
AgentBackend::WaitProgress AgentBackend::waitProgress() const {
    WaitProgress wp;
    auto now = std::chrono::steady_clock::now();
    if (awaitingPageSettle_ && now < nextSnapshotAt_) {
        wp.active = true;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - waitStartedAt_).count() / 1000.0;
        wp.progress = static_cast<float>(elapsed / waitDurationTotal_);
    } else if (!pendingActions_.empty() && std::chrono::steady_clock::now() < nextActionAt_ && !pendingActions_.front().selector.empty()) {
        // We can also show progress for action delays if needed, but let's stick to page waits for now
    }
    return wp;
}
void AgentBackend::setApiKey(const std::string& key) {
    client_.setApiKey(key);
    messages_.push_back({ChatMessage::Author::System, "Quantum Intelligence core activated with your API key."});
}

std::string AgentBackend::maskedApiKey() const {
    return client_.maskedApiKey();
}

std::string AgentBackend::activeTaskId() const {
    return activeTaskId_.empty() ? "IDLE" : activeTaskId_;
}

std::vector<AgentMemoryItem> AgentBackend::recentMemory(int limit) const {
    return memory_.recent(limit);
}

bool AgentBackend::attachImage(const std::filesystem::path& path, const std::string& label) {
    if (path.empty() || !std::filesystem::exists(path) || std::filesystem::is_directory(path) || !isSupportedImagePath(path)) {
        messages_.push_back({ChatMessage::Author::System, "Image attach failed. Use PNG, JPG, WEBP, or GIF only."});
        return false;
    }
    GeminiImageAttachment attachment;
    attachment.path = path;
    attachment.label = label.empty() ? path.filename().string() : label;
    pendingImages_.push_back(std::move(attachment));
    messages_.push_back({ChatMessage::Author::System, "Image ready for next agent prompt: " + pendingImages_.back().label});
    trace_.record(TraceKind::System, "Image attached", pendingImages_.back().path.string());
    return true;
}

void AgentBackend::clearPendingImages() {
    pendingImages_.clear();
    messages_.push_back({ChatMessage::Author::System, "Pending image context cleared."});
}

std::vector<std::string> AgentBackend::pendingImageLabels() const {
    std::vector<std::string> labels;
    labels.reserve(pendingImages_.size());
    for (const auto& image : pendingImages_) {
        labels.push_back(image.label);
    }
    return labels;
}

bool AgentBackend::pageIsSettling(const IBrowserView& browser) const {
    const std::string status = browser.statusText();
    return !browser.isReady() || status == "Loading" || status == "Starting";
}

void AgentBackend::scheduleSnapshotWait(double seconds, const std::string& label) {
    awaitingPageSettle_ = true;
    waitingForSnapshot_ = false;
    const auto now = std::chrono::steady_clock::now();
    waitStartedAt_ = now;
    waitDurationTotal_ = std::max(0.1, seconds);
    nextSnapshotAt_ = now + std::chrono::milliseconds(static_cast<int>(waitDurationTotal_ * 1000.0));
    if (!steps_.empty()) {
        steps_[0].label = label;
        steps_[0].done = false;
    }
}

void AgentBackend::requestSnapshot(IBrowserView& browser) {
    if (pageIsSettling(browser)) {
        scheduleSnapshotWait(2.0, activeTaskId_.empty() ? "Wait for browser settle" : activeTaskId_ + " wait for browser settle");
        return;
    }
    waitingForSnapshot_ = true;
    const std::string task = activeTask_;
    trace_.record(TraceKind::Snapshot, "Snapshot requested", browser.currentUrl());
    browser.executeScript(buildAxTreeSnapshotScript(), [this, task, &browser](std::string result) {
        beginRemotePlan(task, unwrapWebViewJsonString(result), browser);
    });
}

void AgentBackend::beginRemotePlan(const std::string& task, const std::string& snapshot, IBrowserView& browser) {
    if (stopped_) {
        waitingForSnapshot_ = false;
        return;
    }

    if (!steps_.empty()) {
        steps_[0].done = true;
    }

    const PageSnapshot page = parsePageSnapshot(snapshot);
    trace_.record(TraceKind::Snapshot, "Snapshot captured", page.summaryLine());

    const std::string url = browser.currentUrl();
    const std::string title = browser.title();
    std::string prompt = PromptBuilder::buildAgentPrompt({
        task,
        snapshot,
        url,
        title,
        memory_.summarizeForPrompt(),
        tools_.describeForPrompt()
    });
    std::vector<GeminiImageAttachment> images = activeImages_;
    activeImages_.clear();
    if (!images.empty()) {
        prompt += "\nImage attachments are included as visual context. Use them when they clarify the user's request.\n";
        for (const auto& image : images) {
            prompt += "- " + image.label + "\n";
        }
    }
    memory_.remember("task", task);
    trace_.record(TraceKind::Model, "Planner request", title.empty() ? url : title);
    planFuture_ = std::async(std::launch::async, [this, prompt, task, url, images = std::move(images)]() {
        return client_.planFromPrompt(prompt, task, url, images);
    });
}

void AgentBackend::executeAction(const BrowserAction& action, IBrowserView& browser) {
    SafetyDecision decision = safety_.assess(action, browser.currentUrl());
    if (!decision.allowed) {
        messages_.push_back({ChatMessage::Author::System, decision.reason});
        trace_.record(TraceKind::Error, "Action blocked", decision.reason);
        return;
    }

    trace_.record(TraceKind::Action, actionName(action.type), action.url.empty() ? action.selector : action.url);
    if (action.type == BrowserAction::Type::Wait) {
        auto now = std::chrono::steady_clock::now();
        waitStartedAt_ = now;
        waitDurationTotal_ = action.seconds;
        nextActionAt_ = now + std::chrono::milliseconds(static_cast<int>(action.seconds * 1000.0));
        awaitingPageSettle_ = true; // Use the settle logic to show the timer
        nextSnapshotAt_ = nextActionAt_;
        return;
    }
    tools_.execute(action, browser);
}

} // namespace quantum
