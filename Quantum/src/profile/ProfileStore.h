#pragma once

#include "browser/BrowserModel.h"
#include "core/AppPaths.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace quantum {

struct BrowserSettings {
    std::string homeUrl = "https://www.google.com";
    std::string searchTemplate = "https://www.google.com/search?q={query}";
    bool restoreSession = true;
    bool compactTabs = false;
    bool showAgentTrace = true;
    int maxHistory = 3000;
    std::string geminiApiKey;
};

struct HistoryEntry {
    std::string url;
    std::string title;
    std::int64_t lastVisitedUnix = 0;
    int visitCount = 0;
};

struct BookmarkEntry {
    std::string url;
    std::string title;
    std::string group = "General";
    std::int64_t createdUnix = 0;
};

struct ProfileSnapshot {
    BrowserSettings settings;
    std::vector<HistoryEntry> history;
    std::vector<BookmarkEntry> bookmarks;
    std::vector<BrowserTab> sessionTabs;
    int activeTab = 0;
};

class ProfileStore {
public:
    explicit ProfileStore(AppPaths paths);

    void load();
    void save() const;
    void saveSession(const BrowserModel& model);

    BrowserSettings& settings();
    const BrowserSettings& settings() const;
    const std::vector<HistoryEntry>& history() const;
    const std::vector<BookmarkEntry>& bookmarks() const;
    const std::vector<BrowserTab>& sessionTabs() const;
    int activeTab() const;

    void addHistory(std::string url, std::string title);
    void addOrUpdateBookmark(std::string url, std::string title, std::string group = "General");
    void removeBookmark(std::string url);
    bool isBookmarked(const std::string& url) const;
    std::vector<HistoryEntry> searchHistory(const std::string& query, int limit = 30) const;
    std::vector<BookmarkEntry> searchBookmarks(const std::string& query, int limit = 30) const;

    const AppPaths& paths() const;

private:
    std::filesystem::path profileFile() const;
    static std::int64_t nowUnix();

    AppPaths paths_;
    ProfileSnapshot snapshot_;
};

} // namespace quantum
