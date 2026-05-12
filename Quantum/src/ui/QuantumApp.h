#pragma once

#include "agent/AgentBackend.h"
#include "browser/BrowserModel.h"
#include "browser/IBrowserView.h"
#include "profile/CredentialVault.h"
#include "profile/ProfileStore.h"

#include <imgui.h>

#include <array>
#include <filesystem>
#include <functional>
#include <string>

struct GLFWwindow;

namespace quantum {

class QuantumApp {
public:
    QuantumApp(GLFWwindow* window, IBrowserView& browser, AgentBackend& agent, ProfileStore& profile, CredentialVault& vault);
    ~QuantumApp();

    void frame();
    void drawChrome(const ImVec2& size);
    void syncActiveTabFromBrowser();

private:
    void drawTabs(const ImVec2& origin, float width);
    void drawAddressBar(const ImVec2& origin, float width);
    void drawPageSurface(const ImVec2& origin, const ImVec2& size);
    void drawAssistant(const ImVec2& origin, const ImVec2& size);
    void drawMockPage(const ImVec2& origin, const ImVec2& size);
    void drawHistorySurface(const ImVec2& origin, const ImVec2& size);
    void drawBookmarksSurface(const ImVec2& origin, const ImVec2& size);
    void drawAgentTraceSurface(const ImVec2& origin, const ImVec2& size);
    void drawChatHistorySurface(const ImVec2& origin, const ImVec2& size);
    void drawDownloadsSurface(const ImVec2& origin, const ImVec2& size);
    void drawSecuritySurface(const ImVec2& origin, const ImVec2& size);
    void drawSettingsSurface(const ImVec2& origin, const ImVec2& size);
    void drawCommandPalette(const ImVec2& rootOrigin, const ImVec2& rootSize);
    void drawIconButton(const char* label, bool enabled, const ImVec2& size, const std::function<void()>& onClick);
    void submitAddress();
    void submitAgentPrompt();
    void runCommand(const std::string& command);
    void attachImageFromDialog();
    void attachBrowserSnapshot();
    void handleWindowDrag();
    void toggleFullscreen();

    GLFWwindow* window_ = nullptr;
    IBrowserView& browser_;
    AgentBackend& agent_;
    ProfileStore& profile_;
    CredentialVault& vault_;
    BrowserModel model_;
    std::array<char, 1024> addressBuffer_{};
    std::array<char, 2048> promptBuffer_{};
    std::array<char, 512> commandBuffer_{};
    std::array<char, 256> apiKeyBuffer_{};
    bool sidebarOpen_ = true;
    bool isFullscreen_ = false;
    int windowedX_ = 0, windowedY_ = 0, windowedW_ = 0, windowedH_ = 0;
    bool addressEditing_ = false;
    bool commandPaletteOpen_ = false;
    bool assistantOpen_ = true;
    bool showApiKey_ = false;
    bool windowDragging_ = false;
    ImVec2 dragAnchor_{};
    int dragWindowX_ = 0;
    int dragWindowY_ = 0;
    std::string lastRecordedUrl_;
    std::string lastUiNotice_;
};

} // namespace quantum
