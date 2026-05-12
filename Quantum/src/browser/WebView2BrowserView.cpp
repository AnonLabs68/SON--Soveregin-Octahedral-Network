#if QUANTUM_ENABLE_WEBVIEW2

#include "browser/WebView2BrowserView.h"
#include "core/Log.h"
#include "security/SecurityCortex.h"

#include <WebView2EnvironmentOptions.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>

#include <algorithm>
#include <sstream>
#include <utility>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace quantum {

namespace {

std::wstring toWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), length);
    return wide;
}

std::string toUtf8(const wchar_t* value) {
    if (!value) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }
    std::string utf8(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, utf8.data(), length, nullptr, nullptr);
    return utf8;
}

std::filesystem::path knownFolder(REFKNOWNFOLDERID id, std::filesystem::path fallback) {
    PWSTR raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw))) {
        std::filesystem::path result(raw);
        CoTaskMemFree(raw);
        return result;
    }
    return fallback;
}

std::filesystem::path localAppData() {
    return knownFolder(FOLDERID_LocalAppData, std::filesystem::temp_directory_path());
}

std::filesystem::path downloadsFolder() {
    return knownFolder(FOLDERID_Downloads, localAppData() / "Quantum" / "Downloads");
}

std::string permissionKindName(COREWEBVIEW2_PERMISSION_KIND kind) {
    switch (kind) {
    case COREWEBVIEW2_PERMISSION_KIND_MICROPHONE: return "Microphone";
    case COREWEBVIEW2_PERMISSION_KIND_CAMERA: return "Camera";
    case COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION: return "Geolocation";
    case COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS: return "Notifications";
    case COREWEBVIEW2_PERMISSION_KIND_OTHER_SENSORS: return "Sensors";
    case COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ: return "Clipboard";
    case COREWEBVIEW2_PERMISSION_KIND_MULTIPLE_AUTOMATIC_DOWNLOADS: return "Multiple downloads";
    case COREWEBVIEW2_PERMISSION_KIND_FILE_READ_WRITE: return "File read/write";
    case COREWEBVIEW2_PERMISSION_KIND_AUTOPLAY: return "Autoplay";
    case COREWEBVIEW2_PERMISSION_KIND_LOCAL_FONTS: return "Local fonts";
    case COREWEBVIEW2_PERMISSION_KIND_MIDI_SYSTEM_EXCLUSIVE_MESSAGES: return "MIDI sysex";
    case COREWEBVIEW2_PERMISSION_KIND_WINDOW_MANAGEMENT: return "Window management";
    case COREWEBVIEW2_PERMISSION_KIND_PERSISTENT_STORAGE: return "Persistent storage";
    default: return "Unknown";
    }
}

std::string downloadStateName(COREWEBVIEW2_DOWNLOAD_STATE state) {
    switch (state) {
    case COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS: return "Downloading";
    case COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED: return "Interrupted";
    case COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED: return "Completed";
    default: return "Unknown";
    }
}

} // namespace

WebView2BrowserView::WebView2BrowserView(void* parentHwnd)
    : parentHwnd_(parentHwnd) {
}

WebView2BrowserView::~WebView2BrowserView() {
    std::scoped_lock lock(tabMutex_);
    for (auto& [id, tab] : tabs_) {
        if (tab && tab->controller) {
            tab->controller->Close();
        }
    }
}

void WebView2BrowserView::initialize() {
    // FORCE LOCAL PROFILE for diagnostic
    profileDir_ = std::filesystem::current_path() / "QuantumProfile";
    std::filesystem::create_directories(profileDir_);
    
    std::wstring userData = profileDir_.wstring();
    std::filesystem::create_directories(downloadsFolder());
    
    startupLog("Initializing Quantum Core with Profile: " + profileDir_.string());

    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    if (options) {
        options->put_EnableTrackingPrevention(FALSE);
        options->put_AreBrowserExtensionsEnabled(FALSE);
        options->put_AdditionalBrowserArguments(
            L"--disable-background-networking "
            L"--disable-component-update "
            L"--disable-default-apps "
            L"--disable-domain-reliability "
            L"--disable-features=msSmartScreenProtection,msEdgeSmartScreenProtection,SmartScreenUrlReputationService,SafeBrowsingEnhancedProtection,TrackingPrevention,OptimizationGuideModelDownloading,OptimizationHintsFetching "
            L"--disable-sync "
            L"--no-first-run");
        startupLog("WebView2 native browser protection stack disabled; Quantum SecurityCortex remains active.");
    }

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, 
        userData.c_str(),
        options.Get(), 
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || !environment) {
                    startupLog("FATAL: Environment creation failed with HRESULT " + std::to_string(result));
                    return result;
                }
                environment_ = environment;
                status_ = "WebView2 ready";
                
                std::vector<PendingTab> pending;
                {
                    std::scoped_lock lock(tabMutex_);
                    pending = std::move(pendingTabs_);
                    pendingTabs_.clear();
                }
                
                for (const auto& tab : pending) {
                    createNativeTab(tab.id, tab.url);
                }
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        status_ = "CreateCoreWebView2EnvironmentWithOptions failed";
    }
}

void WebView2BrowserView::tick() {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (tab && tab->webview && tab->overlayInjected != agentBusy_) {
        injectStopOverlay(*tab);
    }
}

void WebView2BrowserView::setBounds(int x, int y, int width, int height) {
    x_ = x;
    y_ = y;
    width_ = width;
    height_ = height;
    updateBounds();
}

void WebView2BrowserView::forceZOrder() {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (tab && tab->controller && tab->ready) {
        // INSTITUTIONAL PARENTING
        tab->controller->put_ParentWindow(static_cast<HWND>(parentHwnd_));
        
        // RE-ANCHOR BOUNDS (Forces WebView2 to recalculate its DCOMP/Composition layer)
        RECT bounds{
            static_cast<LONG>(x_),
            static_cast<LONG>(y_),
            static_cast<LONG>(x_ + width_),
            static_cast<LONG>(y_ + height_)
        };
        tab->controller->put_Bounds(bounds);
        tab->controller->NotifyParentWindowPositionChanged();
        
        // VISIBILITY PULSE (Forces a paint refresh)
        tab->controller->put_IsVisible(FALSE);
        tab->controller->put_IsVisible(TRUE);
    }
}

void WebView2BrowserView::setFocus(bool focused) {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (tab && tab->controller) {
        if (focused) {
            tab->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        } else {
            // Give focus back to the parent ImGui window to stop the flicker
            SetFocus(static_cast<HWND>(parentHwnd_));
        }
    }
}

void WebView2BrowserView::createTab(std::uint64_t tabId, const std::string& url) {
    std::scoped_lock lock(tabMutex_);
    if (tabId == 0 || tabs_.contains(tabId)) {
        return;
    }
    if (!environment_) {
        pendingTabs_.push_back({tabId, url});
        return;
    }
    createNativeTab(tabId, url);
}

void WebView2BrowserView::activateTab(std::uint64_t tabId) {
    std::scoped_lock lock(tabMutex_);
    if (tabId == 0) {
        return;
    }
    activeTabId_ = tabId;
    updateBounds();
    if (NativeTab* tab = activeTab()) {
        injectStopOverlay(*tab);
    }
}

void WebView2BrowserView::closeTab(std::uint64_t tabId) {
    std::scoped_lock lock(tabMutex_);
    auto it = tabs_.find(tabId);
    if (it == tabs_.end()) {
        return;
    }
    if (it->second && it->second->controller) {
        it->second->controller->Close();
    }
    tabs_.erase(it);
    if (activeTabId_ == tabId) {
        activeTabId_ = tabs_.empty() ? 0 : tabs_.begin()->first;
    }
    updateBounds();
}

void WebView2BrowserView::navigate(const std::string& url) {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (!tab || !tab->webview) {
        return;
    }
    const auto normalized = normalizeUrl(url);
    const auto wide = toWide(normalized);
    tab->webview->Navigate(wide.c_str());
    tab->url = normalized;
    tab->status = "Navigating";
}

void WebView2BrowserView::goBack() {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (tab && tab->webview && tab->canGoBack) {
        tab->webview->GoBack();
    }
}

void WebView2BrowserView::goForward() {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (tab && tab->webview && tab->canGoForward) {
        tab->webview->GoForward();
    }
}

void WebView2BrowserView::reload() {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (tab && tab->webview) {
        tab->webview->Reload();
    }
}

void WebView2BrowserView::stop() {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (tab && tab->webview) {
        tab->webview->Stop();
    }
}

bool WebView2BrowserView::canGoBack() const {
    std::scoped_lock lock(tabMutex_);
    const NativeTab* tab = activeTab();
    return tab && tab->canGoBack;
}

bool WebView2BrowserView::canGoForward() const {
    std::scoped_lock lock(tabMutex_);
    const NativeTab* tab = activeTab();
    return tab && tab->canGoForward;
}

bool WebView2BrowserView::isReady() const {
    std::scoped_lock lock(tabMutex_);
    const NativeTab* tab = activeTab();
    return tab && tab->ready && tab->webview;
}

bool WebView2BrowserView::isNativeOverlay() const {
    return true;
}

std::string WebView2BrowserView::currentUrl() const {
    std::scoped_lock lock(tabMutex_);
    const NativeTab* tab = activeTab();
    return tab ? tab->url : "about:blank";
}

std::string WebView2BrowserView::title() const {
    std::scoped_lock lock(tabMutex_);
    const NativeTab* tab = activeTab();
    return tab ? tab->title : "Quantum";
}

std::string WebView2BrowserView::statusText() const {
    std::scoped_lock lock(tabMutex_);
    const NativeTab* tab = activeTab();
    return tab ? tab->status : status_;
}

void WebView2BrowserView::setAgentBusy(bool busy) {
    agentBusy_ = busy;
}

bool WebView2BrowserView::consumeAgentStopRequest() {
    std::scoped_lock lock(stateMutex_);
    const bool requested = agentStopRequested_;
    agentStopRequested_ = false;
    return requested;
}

std::vector<BrowserNewTabRequest> WebView2BrowserView::consumeNewTabRequests() {
    std::scoped_lock lock(stateMutex_);
    auto requests = std::move(newTabRequests_);
    newTabRequests_.clear();
    return requests;
}

std::vector<BrowserDownload> WebView2BrowserView::downloads() const {
    std::scoped_lock lock(stateMutex_);
    return downloads_;
}

void WebView2BrowserView::cancelDownload(std::uint64_t downloadId) {
    std::scoped_lock lock(stateMutex_);
    auto it = downloadOperations_.find(downloadId);
    if (it != downloadOperations_.end() && it->second) {
        it->second->Cancel();
    }
}

void WebView2BrowserView::pauseDownload(std::uint64_t downloadId) {
    std::scoped_lock lock(stateMutex_);
    auto it = downloadOperations_.find(downloadId);
    if (it != downloadOperations_.end() && it->second) {
        it->second->Pause();
    }
}

void WebView2BrowserView::resumeDownload(std::uint64_t downloadId) {
    std::scoped_lock lock(stateMutex_);
    auto it = downloadOperations_.find(downloadId);
    if (it != downloadOperations_.end() && it->second) {
        it->second->Resume();
    }
}

std::vector<BrowserPermissionRequest> WebView2BrowserView::permissions() const {
    std::scoped_lock lock(stateMutex_);
    return permissions_;
}

void WebView2BrowserView::resolvePermission(std::uint64_t requestId, bool allow) {
    ComPtr<ICoreWebView2PermissionRequestedEventArgs> args;
    ComPtr<ICoreWebView2Deferral> deferral;
    {
        std::scoped_lock lock(stateMutex_);
        auto it = permissionTickets_.find(requestId);
        if (it == permissionTickets_.end()) {
            return;
        }
        args = it->second.args;
        deferral = it->second.deferral;
        permissionTickets_.erase(it);
        for (auto& permission : permissions_) {
            if (permission.id == requestId) {
                permission.pending = false;
                permission.decision = allow ? "Allowed" : "Denied";
                break;
            }
        }
    }

    if (args) {
        args->put_State(allow ? COREWEBVIEW2_PERMISSION_STATE_ALLOW : COREWEBVIEW2_PERMISSION_STATE_DENY);
        ComPtr<ICoreWebView2PermissionRequestedEventArgs2> args2;
        if (SUCCEEDED(args.As(&args2)) && args2) {
            args2->put_Handled(TRUE);
        }
    }
    if (deferral) {
        deferral->Complete();
    }
}

void WebView2BrowserView::captureScreenshot(const std::filesystem::path& path, std::function<void(bool, std::string)> callback) {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (!tab || !tab->webview) {
        if (callback) {
            callback(false, "No active WebView");
        }
        return;
    }

    std::filesystem::create_directories(path.parent_path());
    ComPtr<IStream> stream;
    HRESULT streamResult = SHCreateStreamOnFileEx(
        path.wstring().c_str(),
        STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE,
        FILE_ATTRIBUTE_NORMAL,
        TRUE,
        nullptr,
        &stream);
    if (FAILED(streamResult) || !stream) {
        if (callback) {
            callback(false, "Could not create screenshot stream");
        }
        return;
    }

    tab->webview->CapturePreview(
        COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,
        stream.Get(),
        Callback<ICoreWebView2CapturePreviewCompletedHandler>(
            [callback = std::move(callback), path](HRESULT result) -> HRESULT {
                if (callback) {
                    callback(SUCCEEDED(result), SUCCEEDED(result) ? path.string() : "CapturePreview failed");
                }
                return S_OK;
            }).Get());
}

void WebView2BrowserView::executeScript(const std::string& script, std::function<void(std::string)> callback) {
    std::scoped_lock lock(tabMutex_);
    NativeTab* tab = activeTab();
    if (!tab || !tab->webview) {
        if (callback) {
            callback({});
        }
        return;
    }

    const auto wide = toWide(script);
    tab->webview->ExecuteScript(
        wide.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [callback = std::move(callback)](HRESULT result, LPCWSTR jsonResult) -> HRESULT {
                if (callback) {
                    callback(SUCCEEDED(result) ? toUtf8(jsonResult) : std::string{});
                }
                return S_OK;
            }).Get());
}

WebView2BrowserView::NativeTab* WebView2BrowserView::activeTab() {
    return findTab(activeTabId_);
}

const WebView2BrowserView::NativeTab* WebView2BrowserView::activeTab() const {
    auto it = tabs_.find(activeTabId_);
    return it == tabs_.end() ? nullptr : it->second.get();
}

WebView2BrowserView::NativeTab* WebView2BrowserView::findTab(std::uint64_t tabId) {
    auto it = tabs_.find(tabId);
    return it == tabs_.end() ? nullptr : it->second.get();
}

void WebView2BrowserView::createNativeTab(std::uint64_t tabId, const std::string& url) {
    {
        std::scoped_lock lock(tabMutex_);
        if (!environment_ || tabs_.contains(tabId)) {
            return;
        }
    }
    
    environment_->CreateCoreWebView2Controller(
        static_cast<HWND>(parentHwnd_),
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this, tabId, url](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                if (FAILED(controllerResult) || !controller) {
                    status_ = "WebView2 controller failed";
                    return S_OK;
                }

                auto tab = std::make_unique<NativeTab>();
                tab->id = tabId;
                tab->controller = controller;
                tab->controller->get_CoreWebView2(&tab->webview);
                tab->controller->put_IsVisible(TRUE);
                
                ComPtr<ICoreWebView2Controller2> controller2;
                if (SUCCEEDED(tab->controller.As(&controller2))) {
                    COREWEBVIEW2_COLOR white{ 255, 255, 255, 255 };
                    controller2->put_DefaultBackgroundColor(white);
                }
                
                tab->status = "Ready";
                tab->ready = true;

                NativeTab* raw = tab.get();
                {
                    std::scoped_lock lock(tabMutex_);
                    tabs_[tabId] = std::move(tab);
                    if (activeTabId_ == 0) {
                        activeTabId_ = tabId;
                    }
                }
                
                configureTab(*raw);
                updateBounds();
                if (!url.empty()) {
                    const auto normalized = normalizeUrl(url);
                    raw->url = normalized;
                    raw->status = "Navigating";
                    raw->webview->Navigate(toWide(normalized).c_str());
                }
                return S_OK;
            }).Get());
}

void WebView2BrowserView::configureTab(NativeTab& tab) {
    if (!tab.webview) {
        return;
    }

    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(tab.webview->get_Settings(&settings)) && settings) {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_AreHostObjectsAllowed(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        
        // MODERN INSTITUTIONAL USER AGENT
        ComPtr<ICoreWebView2Settings7> settings7;
        if (SUCCEEDED(settings.As(&settings7)) && settings7) {
            settings7->put_UserAgent(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36 Quantum/1.7");
            settings7->put_IsPasswordAutosaveEnabled(FALSE);
            settings7->put_IsGeneralAutofillEnabled(FALSE);
        }

        ComPtr<ICoreWebView2Settings8> settings8;
        if (SUCCEEDED(settings.As(&settings8)) && settings8) {
            settings8->put_IsReputationCheckingRequired(FALSE);
        }
    }

    EventRegistrationToken token{};
    const std::uint64_t id = tab.id;
    tab.webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this, id](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                LPWSTR uri = nullptr;
                args->get_Uri(&uri);
                if (uri) {
                    std::string url = toUtf8(uri);
                    CoTaskMemFree(uri);

                    // QUANTUM SECURITY CHECK
                    static quantum::SecurityCortex cortex;
                    auto report = cortex.assessUrl(url);
                    
                    if (report.level == quantum::SecurityLevel::Malicious) {
                        args->put_Cancel(TRUE); // STOP THE NAVIGATION
                        
                        // INJECT WARNING PAGE
                        std::string warningHtml = "<html><body style='background-color:#1a0000; color:#ff4d4d; font-family:sans-serif; display:flex; flex-direction:column; align-items:center; justify-content:center; height:100vh; margin:0;'>"
                                                  "<h1>QUANTUM SECURITY BLOCK</h1>"
                                                  "<p style='font-size:1.2em;'>This website is flagged as a <b>" + report.reason + "</b>.</p>"
                                                  "<div style='background:#330000; padding:20px; border-radius:10px; border:1px solid #ff4d4d;'>"
                                                  "URL: " + url + "</div>"
                                                  "<button onclick='window.history.back()' style='margin-top:20px; padding:10px 20px; background:#ff4d4d; color:white; border:none; border-radius:5px; cursor:pointer;'>Go Back to Safety</button>"
                                                  "</body></html>";
                        sender->NavigateToString(toWide(warningHtml).c_str());
                        return S_OK;
                    }
                }

                {
                    std::scoped_lock lock(tabMutex_);
                    if (NativeTab* tab = findTab(id)) {
                        tab->status = "Loading";
                    }
                }
                return S_OK;
            }).Get(),
        &token);

    tab.webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this, id](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                std::scoped_lock lock(tabMutex_);
                if (NativeTab* tab = findTab(id)) {
                    refreshNavigationState(*tab);
                    tab->status = "Ready";
                    injectStopOverlay(*tab);
                }
                return S_OK;
            }).Get(),
        &token);

    tab.webview->add_SourceChanged(
        Callback<ICoreWebView2SourceChangedEventHandler>(
            [this, id](ICoreWebView2*, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
                std::scoped_lock lock(tabMutex_);
                NativeTab* tab = findTab(id);
                if (!tab || !tab->webview) {
                    return S_OK;
                }
                LPWSTR source = nullptr;
                if (SUCCEEDED(tab->webview->get_Source(&source))) {
                    tab->url = toUtf8(source);
                    CoTaskMemFree(source);
                }
                refreshNavigationState(*tab);
                return S_OK;
            }).Get(),
        &token);

    tab.webview->add_DocumentTitleChanged(
        Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
            [this, id](ICoreWebView2*, IUnknown*) -> HRESULT {
                std::scoped_lock lock(tabMutex_);
                NativeTab* tab = findTab(id);
                if (!tab || !tab->webview) {
                    return S_OK;
                }
                LPWSTR title = nullptr;
                if (SUCCEEDED(tab->webview->get_DocumentTitle(&title))) {
                    tab->title = toUtf8(title);
                    CoTaskMemFree(title);
                }
                return S_OK;
            }).Get(),
        &token);

    tab.webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR message = nullptr;
                if (args && SUCCEEDED(args->TryGetWebMessageAsString(&message))) {
                    const std::string text = toUtf8(message);
                    CoTaskMemFree(message);
                    if (text == "quantum.stopAgent") {
                        std::scoped_lock lock(stateMutex_);
                        agentStopRequested_ = true;
                    }
                }
                return S_OK;
            }).Get(),
        &token);

    tab.webview->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                if (!args) {
                    return S_OK;
                }
                args->put_Handled(TRUE);
                LPWSTR uri = nullptr;
                if (SUCCEEDED(args->get_Uri(&uri))) {
                    std::scoped_lock lock(stateMutex_);
                    newTabRequests_.push_back({toUtf8(uri)});
                    CoTaskMemFree(uri);
                }
                return S_OK;
            }).Get(),
        &token);

    tab.webview->add_PermissionRequested(
        Callback<ICoreWebView2PermissionRequestedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                recordPermission(args);
                return S_OK;
            }).Get(),
        &token);

    ComPtr<ICoreWebView2_4> webview4;
    if (SUCCEEDED(tab.webview.As(&webview4)) && webview4) {
        webview4->add_DownloadStarting(
            Callback<ICoreWebView2DownloadStartingEventHandler>(
                [this](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                    if (!args) {
                        return S_OK;
                    }
                    args->put_Handled(TRUE);
                    ComPtr<ICoreWebView2DownloadOperation> operation;
                    if (SUCCEEDED(args->get_DownloadOperation(&operation)) && operation) {
                        recordDownload(operation.Get());
                    }
                    return S_OK;
                }).Get(),
            &token);
    }
}

void WebView2BrowserView::updateBounds() {
    std::scoped_lock lock(tabMutex_);
    RECT bounds{
        static_cast<LONG>(x_),
        static_cast<LONG>(y_),
        static_cast<LONG>(x_ + width_),
        static_cast<LONG>(y_ + height_)
    };
    
    for (auto& [id, tab] : tabs_) {
        if (!tab || !tab->controller) {
            continue;
        }
        
        const BOOL visible = (id == activeTabId_ && activeTabId_ != 0) ? TRUE : FALSE;
        tab->controller->put_IsVisible(visible);
        tab->controller->put_Bounds(bounds);
        tab->controller->NotifyParentWindowPositionChanged();
    }
}

void WebView2BrowserView::refreshNavigationState(NativeTab& tab) {
    if (!tab.webview) {
        return;
    }
    BOOL canBack = FALSE;
    BOOL canForward = FALSE;
    tab.webview->get_CanGoBack(&canBack);
    tab.webview->get_CanGoForward(&canForward);
    tab.canGoBack = canBack == TRUE;
    tab.canGoForward = canForward == TRUE;
}

void WebView2BrowserView::injectStopOverlay(NativeTab& tab) {
    if (!tab.webview) {
        return;
    }

    tab.overlayInjected = agentBusy_;
    std::string script;
    if (agentBusy_) {
        script = R"JS(
(() => {
  let btn = document.getElementById('__quantum_stop_agent');
  if (!btn) {
    btn = document.createElement('button');
    btn.id = '__quantum_stop_agent';
    btn.textContent = 'Stop Quantum Assistant';
    btn.onclick = () => chrome.webview.postMessage('quantum.stopAgent');
    Object.assign(btn.style, {
      position: 'fixed',
      left: '50%',
      top: '48%',
      transform: 'translate(-50%, -50%)',
      zIndex: '2147483647',
      border: '1px solid rgba(255,255,255,.14)',
      borderRadius: '16px',
      padding: '14px 22px',
      background: 'rgba(5,7,10,.92)',
      color: 'white',
      font: '600 14px Segoe UI, system-ui, sans-serif',
      boxShadow: '0 18px 52px rgba(0,0,0,.48)',
      cursor: 'pointer',
      backdropFilter: 'blur(18px)'
    });
    document.documentElement.appendChild(btn);
  }
})();
)JS";
    } else {
        script = R"JS(
(() => {
  const btn = document.getElementById('__quantum_stop_agent');
  if (btn) btn.remove();
})();
)JS";
    }

    tab.webview->ExecuteScript(toWide(script).c_str(), nullptr);
}

void WebView2BrowserView::recordDownload(ICoreWebView2DownloadOperation* operation) {
    if (!operation) {
        return;
    }
    const std::uint64_t id = nextDownloadId_++;
    downloads_.push_back({id});
    downloadOperations_[id] = operation;
    updateDownload(id, operation);

    EventRegistrationToken token{};
    operation->add_BytesReceivedChanged(
        Callback<ICoreWebView2BytesReceivedChangedEventHandler>(
            [this, id](ICoreWebView2DownloadOperation* sender, IUnknown*) -> HRESULT {
                updateDownload(id, sender);
                return S_OK;
            }).Get(),
        &token);
    operation->add_StateChanged(
        Callback<ICoreWebView2StateChangedEventHandler>(
            [this, id](ICoreWebView2DownloadOperation* sender, IUnknown*) -> HRESULT {
                updateDownload(id, sender);
                return S_OK;
            }).Get(),
        &token);
}

void WebView2BrowserView::updateDownload(std::uint64_t id, ICoreWebView2DownloadOperation* operation) {
    if (!operation) {
        return;
    }
    auto it = std::find_if(downloads_.begin(), downloads_.end(), [&](const BrowserDownload& item) {
        return item.id == id;
    });
    if (it == downloads_.end()) {
        return;
    }

    LPWSTR raw = nullptr;
    if (SUCCEEDED(operation->get_Uri(&raw))) {
        it->uri = toUtf8(raw);
        CoTaskMemFree(raw);
    }
    if (SUCCEEDED(operation->get_ResultFilePath(&raw))) {
        it->filePath = toUtf8(raw);
        CoTaskMemFree(raw);
    }
    if (SUCCEEDED(operation->get_MimeType(&raw))) {
        it->mimeType = toUtf8(raw);
        CoTaskMemFree(raw);
    }
    INT64 total = 0;
    if (SUCCEEDED(operation->get_TotalBytesToReceive(&total))) {
        it->totalBytes = total;
    }
    INT64 received = 0;
    if (SUCCEEDED(operation->get_BytesReceived(&received))) {
        it->receivedBytes = received;
    }
    COREWEBVIEW2_DOWNLOAD_STATE state{};
    if (SUCCEEDED(operation->get_State(&state))) {
        it->state = downloadStateName(state);
    }
}

void WebView2BrowserView::recordPermission(ICoreWebView2PermissionRequestedEventArgs* args) {
    if (!args) {
        return;
    }
    ComPtr<ICoreWebView2Deferral> deferral;
    args->GetDeferral(&deferral);

    BrowserPermissionRequest request;
    request.id = nextPermissionId_++;
    LPWSTR uri = nullptr;
    if (SUCCEEDED(args->get_Uri(&uri))) {
        request.uri = toUtf8(uri);
        CoTaskMemFree(uri);
    }
    COREWEBVIEW2_PERMISSION_KIND kind{};
    if (SUCCEEDED(args->get_PermissionKind(&kind))) {
        request.kind = permissionKindName(kind);
    }
    BOOL initiated = FALSE;
    if (SUCCEEDED(args->get_IsUserInitiated(&initiated))) {
        request.userInitiated = initiated == TRUE;
    }

    {
        std::scoped_lock lock(stateMutex_);
        permissions_.push_back(request);
        permissionTickets_[request.id] = {args, deferral};
    }
}

} // namespace quantum

#endif
