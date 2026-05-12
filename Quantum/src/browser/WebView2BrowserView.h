#pragma once

#if QUANTUM_ENABLE_WEBVIEW2

#include "browser/IBrowserView.h"

#include <windows.h>
#include <unknwn.h>
#include <WebView2.h>
#include <wrl.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace quantum {

class WebView2BrowserView final : public IBrowserView {
public:
    explicit WebView2BrowserView(void* parentHwnd);
    ~WebView2BrowserView() override;

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
    struct NativeTab {
        std::uint64_t id = 0;
        Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
        Microsoft::WRL::ComPtr<ICoreWebView2> webview;
        bool ready = false;
        bool canGoBack = false;
        bool canGoForward = false;
        bool overlayInjected = false;
        std::string url = "about:blank";
        std::string title = "Quantum";
        std::string status = "Starting WebView2";
    };

    struct PendingTab {
        std::uint64_t id = 0;
        std::string url;
    };

    struct PermissionTicket {
        Microsoft::WRL::ComPtr<ICoreWebView2PermissionRequestedEventArgs> args;
        Microsoft::WRL::ComPtr<ICoreWebView2Deferral> deferral;
    };

    NativeTab* activeTab();
    const NativeTab* activeTab() const;
    NativeTab* findTab(std::uint64_t tabId);
    void createNativeTab(std::uint64_t tabId, const std::string& url);
    void configureTab(NativeTab& tab);
    void updateBounds();
    void refreshNavigationState(NativeTab& tab);
    void injectStopOverlay(NativeTab& tab);
    void recordDownload(ICoreWebView2DownloadOperation* operation);
    void updateDownload(std::uint64_t id, ICoreWebView2DownloadOperation* operation);
    void recordPermission(ICoreWebView2PermissionRequestedEventArgs* args);

    void* parentHwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    mutable std::recursive_mutex tabMutex_;
    std::unordered_map<std::uint64_t, std::unique_ptr<NativeTab>> tabs_;
    std::vector<PendingTab> pendingTabs_;
    std::uint64_t activeTabId_ = 0;

    int x_ = 0;
    int y_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool agentBusy_ = false;
    bool agentStopRequested_ = false;
    std::string status_ = "Starting WebView2";
    std::filesystem::path profileDir_;
    std::vector<BrowserDownload> downloads_;
    std::unordered_map<std::uint64_t, Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation>> downloadOperations_;
    std::vector<BrowserPermissionRequest> permissions_;
    std::vector<BrowserNewTabRequest> newTabRequests_;
    std::unordered_map<std::uint64_t, PermissionTicket> permissionTickets_;
    std::uint64_t nextDownloadId_ = 1;
    std::uint64_t nextPermissionId_ = 1;
    mutable std::mutex stateMutex_;
};

} // namespace quantum

#endif
