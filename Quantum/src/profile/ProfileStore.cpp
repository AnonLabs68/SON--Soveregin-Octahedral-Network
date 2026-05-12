#include "profile/ProfileStore.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <utility>

namespace quantum {

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool containsFolded(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    return lowerCopy(haystack).find(lowerCopy(needle)) != std::string::npos;
}

void to_json(nlohmann::json& json, const BrowserSettings& settings) {
    json = {
        {"homeUrl", settings.homeUrl},
        {"searchTemplate", settings.searchTemplate},
        {"restoreSession", settings.restoreSession},
        {"compactTabs", settings.compactTabs},
        {"showAgentTrace", settings.showAgentTrace},
        {"maxHistory", settings.maxHistory},
        {"geminiApiKey", settings.geminiApiKey}
    };
}

void from_json(const nlohmann::json& json, BrowserSettings& settings) {
    settings.homeUrl = json.value("homeUrl", settings.homeUrl);
    settings.searchTemplate = json.value("searchTemplate", settings.searchTemplate);
    settings.restoreSession = json.value("restoreSession", settings.restoreSession);
    settings.compactTabs = json.value("compactTabs", settings.compactTabs);
    settings.showAgentTrace = json.value("showAgentTrace", settings.showAgentTrace);
    settings.maxHistory = json.value("maxHistory", settings.maxHistory);
    settings.geminiApiKey = json.value("geminiApiKey", settings.geminiApiKey);
}

void to_json(nlohmann::json& json, const HistoryEntry& entry) {
    json = {
        {"url", entry.url},
        {"title", entry.title},
        {"lastVisitedUnix", entry.lastVisitedUnix},
        {"visitCount", entry.visitCount}
    };
}

void from_json(const nlohmann::json& json, HistoryEntry& entry) {
    entry.url = json.value("url", "");
    entry.title = json.value("title", entry.url);
    entry.lastVisitedUnix = json.value("lastVisitedUnix", 0LL);
    entry.visitCount = json.value("visitCount", 0);
}

void to_json(nlohmann::json& json, const BookmarkEntry& entry) {
    json = {
        {"url", entry.url},
        {"title", entry.title},
        {"group", entry.group},
        {"createdUnix", entry.createdUnix}
    };
}

void from_json(const nlohmann::json& json, BookmarkEntry& entry) {
    entry.url = json.value("url", "");
    entry.title = json.value("title", entry.url);
    entry.group = json.value("group", "General");
    entry.createdUnix = json.value("createdUnix", 0LL);
}

void to_json(nlohmann::json& json, const BrowserTab& tab) {
    json = {
        {"id", tab.id},
        {"title", tab.title},
        {"url", tab.url},
        {"pinned", tab.pinned},
        {"loading", tab.loading}
    };
}

void from_json(const nlohmann::json& json, BrowserTab& tab) {
    tab.id = json.value("id", 0ULL);
    tab.title = json.value("title", "New Tab");
    tab.url = json.value("url", "https://www.google.com");
    tab.pinned = json.value("pinned", false);
    tab.loading = json.value("loading", false);
}

ProfileStore::ProfileStore(AppPaths paths)
    : paths_(std::move(paths)) {
}

void ProfileStore::load() {
    paths_.ensure();
    std::ifstream in(profileFile());
    if (!in) {
        return;
    }

    try {
        nlohmann::json json;
        in >> json;
        snapshot_.settings = json.value("settings", BrowserSettings{});
        snapshot_.history = json.value("history", std::vector<HistoryEntry>{});
        snapshot_.bookmarks = json.value("bookmarks", std::vector<BookmarkEntry>{});
        snapshot_.sessionTabs = json.value("sessionTabs", std::vector<BrowserTab>{});
        snapshot_.activeTab = json.value("activeTab", 0);
    } catch (...) {
        snapshot_ = ProfileSnapshot{};
    }
}

void ProfileStore::save() const {
    paths_.ensure();
    nlohmann::json json = {
        {"settings", snapshot_.settings},
        {"history", snapshot_.history},
        {"bookmarks", snapshot_.bookmarks},
        {"sessionTabs", snapshot_.sessionTabs},
        {"activeTab", snapshot_.activeTab}
    };

    const auto temp = profileFile().string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << json.dump(2);
    }
    if (std::filesystem::exists(profileFile())) {
        std::filesystem::remove(profileFile());
    }
    std::filesystem::rename(temp, profileFile());
}

void ProfileStore::saveSession(const BrowserModel& model) {
    snapshot_.sessionTabs = model.tabs();
    snapshot_.activeTab = model.activeIndex();
    save();
}

BrowserSettings& ProfileStore::settings() {
    return snapshot_.settings;
}

const BrowserSettings& ProfileStore::settings() const {
    return snapshot_.settings;
}

const std::vector<HistoryEntry>& ProfileStore::history() const {
    return snapshot_.history;
}

const std::vector<BookmarkEntry>& ProfileStore::bookmarks() const {
    return snapshot_.bookmarks;
}

const std::vector<BrowserTab>& ProfileStore::sessionTabs() const {
    return snapshot_.sessionTabs;
}

int ProfileStore::activeTab() const {
    return snapshot_.activeTab;
}

void ProfileStore::addHistory(std::string url, std::string title) {
    if (url.empty() || url == "about:blank") {
        return;
    }
    auto it = std::find_if(snapshot_.history.begin(), snapshot_.history.end(), [&](const HistoryEntry& entry) {
        return entry.url == url;
    });
    if (it == snapshot_.history.end()) {
        snapshot_.history.insert(snapshot_.history.begin(), {
            std::move(url),
            title.empty() ? url : std::move(title),
            nowUnix(),
            1
        });
    } else {
        it->title = title.empty() ? it->url : std::move(title);
        it->lastVisitedUnix = nowUnix();
        it->visitCount += 1;
        std::rotate(snapshot_.history.begin(), it, it + 1);
    }

    if (snapshot_.history.size() > static_cast<size_t>(snapshot_.settings.maxHistory)) {
        snapshot_.history.resize(static_cast<size_t>(snapshot_.settings.maxHistory));
    }
}

void ProfileStore::addOrUpdateBookmark(std::string url, std::string title, std::string group) {
    if (url.empty()) {
        return;
    }
    auto it = std::find_if(snapshot_.bookmarks.begin(), snapshot_.bookmarks.end(), [&](const BookmarkEntry& entry) {
        return entry.url == url;
    });
    if (it == snapshot_.bookmarks.end()) {
        snapshot_.bookmarks.push_back({
            std::move(url),
            title.empty() ? url : std::move(title),
            group.empty() ? "General" : std::move(group),
            nowUnix()
        });
    } else {
        it->title = title.empty() ? it->url : std::move(title);
        it->group = group.empty() ? it->group : std::move(group);
    }
}

void ProfileStore::removeBookmark(std::string url) {
    snapshot_.bookmarks.erase(
        std::remove_if(snapshot_.bookmarks.begin(), snapshot_.bookmarks.end(), [&](const BookmarkEntry& entry) {
            return entry.url == url;
        }),
        snapshot_.bookmarks.end());
}

bool ProfileStore::isBookmarked(const std::string& url) const {
    return std::any_of(snapshot_.bookmarks.begin(), snapshot_.bookmarks.end(), [&](const BookmarkEntry& entry) {
        return entry.url == url;
    });
}

std::vector<HistoryEntry> ProfileStore::searchHistory(const std::string& query, int limit) const {
    std::vector<HistoryEntry> matches;
    for (const auto& entry : snapshot_.history) {
        if (containsFolded(entry.title, query) || containsFolded(entry.url, query)) {
            matches.push_back(entry);
            if (static_cast<int>(matches.size()) >= limit) {
                break;
            }
        }
    }
    return matches;
}

std::vector<BookmarkEntry> ProfileStore::searchBookmarks(const std::string& query, int limit) const {
    std::vector<BookmarkEntry> matches;
    for (const auto& entry : snapshot_.bookmarks) {
        if (containsFolded(entry.title, query) || containsFolded(entry.url, query) || containsFolded(entry.group, query)) {
            matches.push_back(entry);
            if (static_cast<int>(matches.size()) >= limit) {
                break;
            }
        }
    }
    return matches;
}

const AppPaths& ProfileStore::paths() const {
    return paths_;
}

std::filesystem::path ProfileStore::profileFile() const {
    return paths_.dataDir() / "profile.json";
}

std::int64_t ProfileStore::nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace quantum
