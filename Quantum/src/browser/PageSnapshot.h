#pragma once

#include <string>
#include <vector>

namespace quantum {

struct SnapshotNode {
    int id = 0;
    std::string role;
    std::string name;
    std::string text;
    std::string selector;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct PageSnapshot {
    std::string url;
    std::string title;
    std::string text;
    bool loading = false;
    std::vector<SnapshotNode> nodes;

    int linkCount() const;
    int buttonCount() const;
    int inputCount() const;
    std::string summaryLine() const;
};

PageSnapshot parsePageSnapshot(const std::string& snapshotJson);

} // namespace quantum

