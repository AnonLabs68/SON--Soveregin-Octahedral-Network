#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace quantum {

enum class BrowserSurfaceMode {
    Web,
    History,
    Bookmarks,
    AgentTrace,
    ChatHistory,
    Downloads,
    Security,
    Settings
};

struct BrowserTab {
    std::uint64_t id = 0;
    std::string title = "New Tab";
    std::string url = "https://www.google.com";
    bool pinned = false;
    bool loading = false;
};

class BrowserModel {
public:
    BrowserModel();

    BrowserTab& activeTab();
    const BrowserTab& activeTab() const;
    BrowserTab& tabAt(int index);
    const BrowserTab& tabAt(int index) const;
    const std::vector<BrowserTab>& tabs() const;

    int activeIndex() const;
    void activate(int index);
    void openTab(std::string url = "https://www.google.com", std::string title = "New Tab");
    void closeTab(int index);
    void updateActiveFromBrowser(std::string url, std::string title, bool loading);
    void restoreTabs(std::vector<BrowserTab> tabs, int activeIndex);

    BrowserSurfaceMode surfaceMode() const;
    void setSurfaceMode(BrowserSurfaceMode mode);
    bool isBrowserSurface() const;

private:
    std::vector<BrowserTab> tabs_;
    int activeIndex_ = 0;
    std::uint64_t nextId_ = 1;
    BrowserSurfaceMode surfaceMode_ = BrowserSurfaceMode::Web;
};

} // namespace quantum
