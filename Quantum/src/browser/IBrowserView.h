#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace quantum {

struct BrowserFormField {
    std::string selector;
    std::string value;
};

struct BrowserAction {
    enum class Type {
        Navigate,
        Click,
        TypeText,
        FillForm,
        PressEnter,
        Wait
    };

    Type type = Type::Wait;
    std::string selector;
    std::string value;
    std::string url;
    std::vector<BrowserFormField> fields;
    double seconds = 0.5;
};

struct BrowserDownload {
    std::uint64_t id = 0;
    std::string uri;
    std::string filePath;
    std::string mimeType;
    std::int64_t totalBytes = 0;
    std::int64_t receivedBytes = 0;
    std::string state = "Starting";
};

struct BrowserPermissionRequest {
    std::uint64_t id = 0;
    std::string uri;
    std::string kind;
    bool userInitiated = false;
    bool pending = true;
    std::string decision = "Pending";
};

struct BrowserNewTabRequest {
    std::string url;
};

class IBrowserView {
public:
    virtual ~IBrowserView() = default;

    virtual void initialize() = 0;
    virtual void tick() = 0;
    virtual void setBounds(int x, int y, int width, int height) = 0;
    virtual void forceZOrder() = 0;
    virtual void setFocus(bool focused) = 0;
    virtual void createTab(std::uint64_t tabId, const std::string& url) = 0;
    virtual void activateTab(std::uint64_t tabId) = 0;
    virtual void closeTab(std::uint64_t tabId) = 0;

    virtual void navigate(const std::string& url) = 0;
    virtual void goBack() = 0;
    virtual void goForward() = 0;
    virtual void reload() = 0;
    virtual void stop() = 0;

    virtual bool canGoBack() const = 0;
    virtual bool canGoForward() const = 0;
    virtual bool isReady() const = 0;
    virtual bool isNativeOverlay() const = 0;

    virtual std::string currentUrl() const = 0;
    virtual std::string title() const = 0;
    virtual std::string statusText() const = 0;

    virtual void setAgentBusy(bool busy) = 0;
    virtual bool consumeAgentStopRequest() = 0;
    virtual std::vector<BrowserNewTabRequest> consumeNewTabRequests() = 0;
    virtual std::vector<BrowserDownload> downloads() const = 0;
    virtual void cancelDownload(std::uint64_t downloadId) = 0;
    virtual void pauseDownload(std::uint64_t downloadId) = 0;
    virtual void resumeDownload(std::uint64_t downloadId) = 0;
    virtual std::vector<BrowserPermissionRequest> permissions() const = 0;
    virtual void resolvePermission(std::uint64_t requestId, bool allow) = 0;
    virtual void captureScreenshot(const std::filesystem::path& path, std::function<void(bool, std::string)> callback) = 0;
    virtual void executeScript(const std::string& script, std::function<void(std::string)> callback) = 0;
};

std::string normalizeUrl(std::string input);

} // namespace quantum
