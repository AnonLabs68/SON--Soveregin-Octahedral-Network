#include "browser/BrowserModel.h"

#include <algorithm>
#include <utility>

namespace quantum {

BrowserModel::BrowserModel() {
    openTab();
}

BrowserTab& BrowserModel::activeTab() {
    return tabs_[activeIndex_];
}

const BrowserTab& BrowserModel::activeTab() const {
    return tabs_[activeIndex_];
}

BrowserTab& BrowserModel::tabAt(int index) {
    return tabs_[static_cast<size_t>(index)];
}

const BrowserTab& BrowserModel::tabAt(int index) const {
    return tabs_[static_cast<size_t>(index)];
}

const std::vector<BrowserTab>& BrowserModel::tabs() const {
    return tabs_;
}

int BrowserModel::activeIndex() const {
    return activeIndex_;
}

void BrowserModel::activate(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    activeIndex_ = index;
    surfaceMode_ = BrowserSurfaceMode::Web;
}

void BrowserModel::openTab(std::string url, std::string title) {
    BrowserTab tab;
    tab.id = nextId_++;
    tab.url = std::move(url);
    tab.title = std::move(title);
    tabs_.push_back(std::move(tab));
    activeIndex_ = static_cast<int>(tabs_.size()) - 1;
    surfaceMode_ = BrowserSurfaceMode::Web;
}

void BrowserModel::closeTab(int index) {
    if (tabs_.size() <= 1 || index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    tabs_.erase(tabs_.begin() + index);
    if (index < activeIndex_) {
        --activeIndex_;
    } else if (index == activeIndex_) {
        activeIndex_ = std::min(activeIndex_, static_cast<int>(tabs_.size()) - 1);
    }
    activeIndex_ = std::clamp(activeIndex_, 0, static_cast<int>(tabs_.size()) - 1);
}

void BrowserModel::updateActiveFromBrowser(std::string url, std::string title, bool loading) {
    BrowserTab& tab = activeTab();
    tab.url = std::move(url);
    tab.title = title.empty() ? tab.url : std::move(title);
    tab.loading = loading;
}

void BrowserModel::restoreTabs(std::vector<BrowserTab> tabs, int activeIndex) {
    tabs_.clear();
    nextId_ = 1;
    for (auto& tab : tabs) {
        if (tab.id == 0) {
            tab.id = nextId_++;
        } else {
            nextId_ = std::max(nextId_, tab.id + 1);
        }
        if (tab.url.empty()) {
            tab.url = "https://www.google.com";
        }
        if (tab.title.empty()) {
            tab.title = tab.url;
        }
        tabs_.push_back(tab);
    }
    if (tabs_.empty()) {
        openTab();
        return;
    }
    activeIndex_ = std::clamp(activeIndex, 0, static_cast<int>(tabs_.size()) - 1);
}

BrowserSurfaceMode BrowserModel::surfaceMode() const {
    return surfaceMode_;
}

void BrowserModel::setSurfaceMode(BrowserSurfaceMode mode) {
    surfaceMode_ = mode;
}

bool BrowserModel::isBrowserSurface() const {
    return surfaceMode_ == BrowserSurfaceMode::Web;
}

} // namespace quantum
