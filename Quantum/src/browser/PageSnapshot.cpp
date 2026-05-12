#include "browser/PageSnapshot.h"

#include <nlohmann/json.hpp>

#include <sstream>
#include <utility>

namespace quantum {

int PageSnapshot::linkCount() const {
    int count = 0;
    for (const auto& node : nodes) {
        if (node.role == "link") {
            ++count;
        }
    }
    return count;
}

int PageSnapshot::buttonCount() const {
    int count = 0;
    for (const auto& node : nodes) {
        if (node.role == "button") {
            ++count;
        }
    }
    return count;
}

int PageSnapshot::inputCount() const {
    int count = 0;
    for (const auto& node : nodes) {
        if (node.role == "input" || node.role == "textbox" || node.role == "searchbox") {
            ++count;
        }
    }
    return count;
}

std::string PageSnapshot::summaryLine() const {
    std::ostringstream out;
    out << nodes.size() << " nodes, "
        << linkCount() << " links, "
        << buttonCount() << " buttons, "
        << inputCount() << " inputs";
    return out.str();
}

PageSnapshot parsePageSnapshot(const std::string& snapshotJson) {
    PageSnapshot snapshot;
    if (snapshotJson.empty()) {
        return snapshot;
    }

    try {
        auto json = nlohmann::json::parse(snapshotJson);
        snapshot.url = json.value("url", "");
        snapshot.title = json.value("title", "");
        snapshot.text = json.value("text", "");
        snapshot.loading = json.value("loading", false);
        if (json.contains("nodes") && json["nodes"].is_array()) {
            for (const auto& item : json["nodes"]) {
                SnapshotNode node;
                node.id = item.value("id", 0);
                node.role = item.value("role", "");
                node.name = item.value("name", "");
                node.text = item.value("text", "");
                node.selector = item.value("selector", "");
                if (item.contains("bounds")) {
                    const auto& bounds = item["bounds"];
                    node.x = bounds.value("x", 0);
                    node.y = bounds.value("y", 0);
                    node.width = bounds.value("w", 0);
                    node.height = bounds.value("h", 0);
                }
                snapshot.nodes.push_back(std::move(node));
            }
        }
    } catch (...) {
    }
    return snapshot;
}

} // namespace quantum
