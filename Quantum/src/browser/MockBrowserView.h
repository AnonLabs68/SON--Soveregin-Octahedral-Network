#pragma once

#include "browser/IBrowserView.h"

#include <chrono>

namespace quantum {

class MockBrowserView final : public IBrowserView {
public:
    void initialize() override;
    void tick() override;
    void setBounds(int x, int y, int width, int height) override;
    void forceZOrder() override;
    void setFocus(bool focused) override;
    void createTab(std::uint64_t tabId, const std::string& url) override;
    void activateTab(std::uint64_t tabId) override;
    void closeTab(std::uint64_t tabId) override;

    void navigate(const std::string& url) override;
    void goBack() override;
    void goForward() override;
    void reload() override;
    void stop() override;

    bool canGoBack() const override;
    bool canGoForward() const override;
    bool isReady() const override;
    bool isNativeOverlay() const override;

    std::string currentUrl() const override;
    std::string title() const override;
    std::string statusText() const override;

    void setAgentBusy(bool busy) override;
    bool consumeAgentStopRequest() override;
    std::vector<BrowserNewTabRequest> consumeNewTabRequests() override;
    std::vector<BrowserDownload> downloads() const override;
    void cancelDownload(std::uint64_t downloadId) override;
    void pauseDownload(std::uint64_t downloadId) override;
    void resumeDownload(std::uint64_t downloadId) override;
    std::vector<BrowserPermissionRequest> permissions() const override;
    void resolvePermission(std::uint64_t requestId, bool allow) override;
    void captureScreenshot(const std::filesystem::path& path, std::function<void(bool, std::string)> callback) override;
    void executeScript(const std::string& script, std::function<void(std::string)> callback) override;

private:
    std::string url_ = "https://www.google.com";
    std::string title_ = "Quantum Start";
    std::string status_ = "Mock browser backend active";
    bool agentBusy_ = false;
    int x_ = 0;
    int y_ = 0;
    int width_ = 0;
    int height_ = 0;
    std::chrono::steady_clock::time_point lastNavigate_{};
};

} // namespace quantum
