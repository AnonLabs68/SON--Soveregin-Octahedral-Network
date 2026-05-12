#pragma once

#include "profile/ProfileStore.h"

#include <string>
#include <vector>

namespace quantum {

enum class CommandKind {
    BuiltIn,
    History,
    Bookmark,
    Navigation
};

struct CommandHit {
    CommandKind kind = CommandKind::BuiltIn;
    std::string title;
    std::string subtitle;
    std::string command;
    int score = 0;
};

class CommandIndex {
public:
    explicit CommandIndex(const ProfileStore& profile);

    std::vector<CommandHit> search(const std::string& query, int limit = 12) const;

private:
    void addBuiltIns(std::vector<CommandHit>& hits, const std::string& query) const;
    void addHistory(std::vector<CommandHit>& hits, const std::string& query) const;
    void addBookmarks(std::vector<CommandHit>& hits, const std::string& query) const;
    static int scoreText(const std::string& text, const std::string& query);

    const ProfileStore& profile_;
};

} // namespace quantum

