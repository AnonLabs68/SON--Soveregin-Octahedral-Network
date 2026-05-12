#include "browser/MockBrowserView.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace quantum {

std::string normalizeUrl(std::string input) {
    input.erase(input.begin(), std::find_if(input.begin(), input.end(), [](unsigned char ch) {
        return ch > ' ';
    }));
    input.erase(std::find_if(input.rbegin(), input.rend(), [](unsigned char ch) {
        return ch > ' ';
    }).base(), input.end());

    if (input.empty()) {
        return "about:blank";
    }
    if (input.find("://") != std::string::npos || input.rfind("about:", 0) == 0) {
        return input;
    }
    if (input.find('.') != std::string::npos && input.find(' ') == std::string::npos) {
        return "https://" + input;
    }
    return "https://www.google.com/search?q=" + input;
}

void MockBrowserView::initialize() {
    status_ = "Mock browser ready. Build on Windows with WebView2 for real pages.";
}

void MockBrowserView::tick() {
}

void MockBrowserView::setBounds(int, int, int, int) {
}

void MockBrowserView::forceZOrder() {
}

void MockBrowserView::setFocus(bool) {
}

void MockBrowserView::createTab(std::uint64_t, const std::string& url) {
    navigate(url);
}

void MockBrowserView::activateTab(std::uint64_t) {
}

void MockBrowserView::closeTab(std::uint64_t) {
}

void MockBrowserView::navigate(const std::string& url) {
    url_ = normalizeUrl(url);
    title_ = url_;
    status_ = "Mock loaded " + url_;
    lastNavigate_ = std::chrono::steady_clock::now();
}

void MockBrowserView::goBack() {
    status_ = "Mock history has no back entry";
}

void MockBrowserView::goForward() {
    status_ = "Mock history has no forward entry";
}

void MockBrowserView::reload() {
    status_ = "Mock reloaded " + url_;
}

void MockBrowserView::stop() {
    status_ = "Stopped";
}

bool MockBrowserView::canGoBack() const {
    return false;
}

bool MockBrowserView::canGoForward() const {
    return false;
}

bool MockBrowserView::isReady() const {
    return true;
}

bool MockBrowserView::isNativeOverlay() const {
    return false;
}

std::string MockBrowserView::currentUrl() const {
    return url_;
}

std::string MockBrowserView::title() const {
    return title_;
}

std::string MockBrowserView::statusText() const {
    return status_;
}

void MockBrowserView::setAgentBusy(bool busy) {
    agentBusy_ = busy;
}

bool MockBrowserView::consumeAgentStopRequest() {
    return false;
}

std::vector<BrowserNewTabRequest> MockBrowserView::consumeNewTabRequests() {
    return {};
}

std::vector<BrowserDownload> MockBrowserView::downloads() const {
    return {};
}

void MockBrowserView::cancelDownload(std::uint64_t) {
}

void MockBrowserView::pauseDownload(std::uint64_t) {
}

void MockBrowserView::resumeDownload(std::uint64_t) {
}

std::vector<BrowserPermissionRequest> MockBrowserView::permissions() const {
    return {};
}

void MockBrowserView::resolvePermission(std::uint64_t, bool) {
}

void MockBrowserView::captureScreenshot(const std::filesystem::path& path, std::function<void(bool, std::string)> callback) {
    if (callback) {
        callback(false, "Mock backend cannot capture " + path.string());
    }
}

void MockBrowserView::executeScript(const std::string&, std::function<void(std::string)> callback) {
    nlohmann::json snapshot = {
        {"url", url_},
        {"title", title_},
        {"text", "Quantum mock page. Buttons: Search, Sign in, Learn more."},
        {"nodes", nlohmann::json::array({
            {{"role", "button"}, {"name", "Search"}, {"selector", "button:nth-of-type(1)"}, {"text", "Search"}},
            {{"role", "link"}, {"name", "Learn more"}, {"selector", "a:nth-of-type(1)"}, {"text", "Learn more"}}
        })}
    };

    if (callback) {
        callback(nlohmann::json(snapshot.dump()).dump());
    }
}

} // namespace quantum
