#include "profile/CommandIndex.h"

#include "core/StringUtils.h"

#include <algorithm>

namespace quantum {

CommandIndex::CommandIndex(const ProfileStore& profile)
    : profile_(profile) {
}

std::vector<CommandHit> CommandIndex::search(const std::string& query, int limit) const {
    std::vector<CommandHit> hits;
    addBuiltIns(hits, query);
    addBookmarks(hits, query);
    addHistory(hits, query);

    const std::string cleaned = trim(query);
    if (!cleaned.empty()) {
        hits.push_back({
            CommandKind::Navigation,
            "Go to " + cleaned,
            "Navigate or search",
            cleaned,
            70
        });
    }

    std::stable_sort(hits.begin(), hits.end(), [](const CommandHit& a, const CommandHit& b) {
        return a.score > b.score;
    });
    if (static_cast<int>(hits.size()) > limit) {
        hits.resize(static_cast<size_t>(limit));
    }
    return hits;
}

void CommandIndex::addBuiltIns(std::vector<CommandHit>& hits, const std::string& query) const {
    const std::vector<CommandHit> builtIns = {
        {CommandKind::BuiltIn, "New Tab", profile_.settings().homeUrl, "new tab", 60},
        {CommandKind::BuiltIn, "Open History", "Show recent navigation", "history", 58},
        {CommandKind::BuiltIn, "Open Bookmarks", "Show saved pages", "bookmarks", 58},
        {CommandKind::BuiltIn, "Open Agent Trace", "Inspect model/action timeline", "trace", 56},
        {CommandKind::BuiltIn, "Open Downloads", "Inspect file transfers", "downloads", 56},
        {CommandKind::BuiltIn, "Open Permissions", "Review site access requests", "permissions", 56},
        {CommandKind::BuiltIn, "Capture Screenshot", "Save the active page as PNG", "screenshot", 55},
        {CommandKind::BuiltIn, "Open Settings", "Tune Quantum profile", "settings", 55}
    };

    for (auto hit : builtIns) {
        const int score = scoreText(hit.title + " " + hit.subtitle, query);
        if (query.empty() || score > 0) {
            hit.score += score;
            hits.push_back(std::move(hit));
        }
    }
}

void CommandIndex::addHistory(std::vector<CommandHit>& hits, const std::string& query) const {
    for (const auto& entry : profile_.searchHistory(query, 20)) {
        hits.push_back({
            CommandKind::History,
            entry.title,
            entry.url,
            entry.url,
            40 + scoreText(entry.title + " " + entry.url, query) + entry.visitCount
        });
    }
}

void CommandIndex::addBookmarks(std::vector<CommandHit>& hits, const std::string& query) const {
    for (const auto& entry : profile_.searchBookmarks(query, 20)) {
        hits.push_back({
            CommandKind::Bookmark,
            entry.title,
            entry.group + " - " + entry.url,
            entry.url,
            80 + scoreText(entry.title + " " + entry.url + " " + entry.group, query)
        });
    }
}

int CommandIndex::scoreText(const std::string& text, const std::string& query) {
    const std::string q = lower(trim(query));
    if (q.empty()) {
        return 0;
    }
    const std::string t = lower(text);
    if (t == q) {
        return 100;
    }
    if (t.rfind(q, 0) == 0) {
        return 80;
    }
    if (t.find(q) != std::string::npos) {
        return 45;
    }
    int score = 0;
    for (const auto& word : splitWords(q)) {
        if (t.find(word) != std::string::npos) {
            score += 12;
        }
    }
    return score;
}

} // namespace quantum
