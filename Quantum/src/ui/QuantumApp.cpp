#include "ui/QuantumApp.h"

#include "ui/QuantumTheme.h"
#include "ui/QuantumWidgets.h"

#include <GLFW/glfw3.h>

#include "profile/CommandIndex.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

namespace quantum {

namespace {

void drawRoundedRect(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 fill, float rounding, ImU32 border, float borderThickness = 1.0f) {
    draw->AddRectFilled(min, max, fill, rounding);
    draw->AddRect(min, max, border, rounding, 0, borderThickness);
}

void textMuted(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(colors().muted));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

const char* authorName(ChatMessage::Author author) {
    switch (author) {
    case ChatMessage::Author::User: return "You";
    case ChatMessage::Author::System: return "System";
    case ChatMessage::Author::Assistant: return "Quantum";
    }
    return "Quantum";
}

std::filesystem::path openImageFileDialog() {
#ifdef _WIN32
    char fileName[MAX_PATH] = {};
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = "Images\0*.png;*.jpg;*.jpeg;*.webp;*.gif\0PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0WEBP\0*.webp\0GIF\0*.gif\0";
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&dialog) == TRUE) {
        return std::filesystem::path(fileName);
    }
#endif
    return {};
}

} // namespace

QuantumApp::QuantumApp(GLFWwindow* window, IBrowserView& browser, AgentBackend& agent, ProfileStore& profile, CredentialVault& vault)
    : window_(window),
      browser_(browser),
      agent_(agent),
      profile_(profile),
      vault_(vault) {
    if (profile_.settings().restoreSession && !profile_.sessionTabs().empty()) {
        model_.restoreTabs(profile_.sessionTabs(), profile_.activeTab());
    }
    if (model_.tabs().empty()) {
        model_.openTab("https://www.google.com");
    }
    for (const auto& tab : model_.tabs()) {
        browser_.createTab(tab.id, tab.url);
    }
    browser_.activateTab(model_.activeTab().id);
    std::snprintf(addressBuffer_.data(), addressBuffer_.size(), "%s", model_.activeTab().url.c_str());
    
    const std::string apiKey = profile_.settings().geminiApiKey.empty()
        ? std::string()
        : profile_.settings().geminiApiKey;
    std::snprintf(apiKeyBuffer_.data(), apiKeyBuffer_.size(), "%s", apiKey.c_str());
    agent_.setApiKey(apiKey);
}

QuantumApp::~QuantumApp() {
    profile_.saveSession(model_);
}

void QuantumApp::frame() {
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_K)) {
        commandPaletteOpen_ = true;
        std::fill(commandBuffer_.begin(), commandBuffer_.end(), '\0');
    }
    
    if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
        static int lastX, lastY, lastW, lastH;
        static bool isFullscreen = false;
        if (isFullscreen) {
            glfwSetWindowAttrib(window_, GLFW_DECORATED, GLFW_TRUE);
            glfwSetWindowMonitor(window_, nullptr, lastX, lastY, lastW, lastH, 0);
            isFullscreen = false;
        } else {
            glfwGetWindowPos(window_, &lastX, &lastY);
            glfwGetWindowSize(window_, &lastW, &lastH);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowAttrib(window_, GLFW_DECORATED, GLFW_FALSE);
            glfwSetWindowMonitor(window_, nullptr, 0, 0, mode->width, mode->height, mode->refreshRate);
            isFullscreen = true;
        }
    }

    browser_.tick();
    if (browser_.consumeAgentStopRequest()) {
        agent_.stop();
    }
    for (const auto& request : browser_.consumeNewTabRequests()) {
        model_.openTab(request.url.empty() ? profile_.settings().homeUrl : request.url);
        browser_.createTab(model_.activeTab().id, model_.activeTab().url);
        browser_.activateTab(model_.activeTab().id);
    }
    agent_.tick(browser_);
    syncActiveTabFromBrowser();

    // FOCUS GOVERNOR: If we are typing in an ImGui box, the browser must release focus
    bool imguiWantsInput = addressEditing_ || ImGui::IsAnyItemActive();
    
    // We only set focus if it changes to avoid more flicker
    static bool lastImguiWantsInput = false;
    if (imguiWantsInput != lastImguiWantsInput) {
        browser_.setFocus(!imguiWantsInput);
        lastImguiWantsInput = imguiWantsInput;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::Begin("QuantumRoot", nullptr, flags);
    drawChrome(viewport->Size);
    drawCommandPalette(viewport->Pos, viewport->Size);
    ImGui::End();
}

void QuantumApp::drawChrome(const ImVec2& size) {
    const auto& c = colors();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float titleH = 30.0f;
    const float tabsH = 38.0f;
    const float addressH = 48.0f;
    const float assistantTargetW = size.x < 760.0f
        ? std::clamp(size.x * 0.38f, 220.0f, 320.0f)
        : std::clamp(size.x * 0.28f, 330.0f, 430.0f);
    const float assistantW = assistantOpen_ ? assistantTargetW : 0.0f;
    const float gap = 10.0f;
    const float bottomPad = 10.0f;

    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), c.bg);
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + titleH + tabsH + addressH), c.chrome);
    for (int i = 0; i < 7; ++i) {
        const int alpha = 18 - i * 2;
        draw->AddRectFilled(
            ImVec2(origin.x, origin.y + static_cast<float>(i * 10)),
            ImVec2(origin.x + size.x, origin.y + static_cast<float>((i + 1) * 10)),
            IM_COL32(255, 255, 255, alpha));
    }
    draw->AddRectFilled(ImVec2(origin.x, origin.y + titleH + tabsH + addressH - 2.0f), ImVec2(origin.x + size.x, origin.y + titleH + tabsH + addressH), c.accentSoft);
    draw->AddLine(ImVec2(origin.x, origin.y + titleH + tabsH + addressH), ImVec2(origin.x + size.x, origin.y + titleH + tabsH + addressH), c.border);

    handleWindowDrag();

    const ImVec2 lightOrigin(origin.x + 14.0f, origin.y + 15.0f);
    draw->AddCircleFilled(lightOrigin, 5.0f, IM_COL32(255, 91, 87, 255));
    draw->AddCircleFilled(ImVec2(lightOrigin.x + 18.0f, lightOrigin.y), 5.0f, IM_COL32(255, 189, 46, 255));
    draw->AddCircleFilled(ImVec2(lightOrigin.x + 36.0f, lightOrigin.y), 5.0f, IM_COL32(39, 201, 63, 255));
    ImGui::SetCursorScreenPos(ImVec2(lightOrigin.x - 7.0f, lightOrigin.y - 7.0f));
    ImGui::InvisibleButton("window-close", ImVec2(14.0f, 14.0f));
    if (ImGui::IsItemClicked()) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
    ImGui::SetCursorScreenPos(ImVec2(lightOrigin.x + 11.0f, lightOrigin.y - 7.0f));
    ImGui::InvisibleButton("window-minimize", ImVec2(14.0f, 14.0f));
    if (ImGui::IsItemClicked()) {
        glfwIconifyWindow(window_);
    }
    ImGui::SetCursorScreenPos(ImVec2(lightOrigin.x + 29.0f, lightOrigin.y - 7.0f));
    ImGui::InvisibleButton("window-maximize", ImVec2(14.0f, 14.0f));
    if (ImGui::IsItemClicked()) {
        if (glfwGetWindowAttrib(window_, GLFW_MAXIMIZED)) {
            glfwRestoreWindow(window_);
        } else {
            glfwMaximizeWindow(window_);
        }
    }
    draw->AddText(ImVec2(origin.x + 74.0f, origin.y + 8.0f), c.text, "Quantum");
    draw->AddText(ImVec2(origin.x + 152.0f, origin.y + 8.0f), c.muted, "agentic browser");
    drawRoundedRect(draw, ImVec2(origin.x + size.x - 228.0f, origin.y + 6.0f), ImVec2(origin.x + size.x - 18.0f, origin.y + 25.0f), IM_COL32(31, 35, 42, 210), 9.0f, c.border);
    draw->AddText(ImVec2(origin.x + size.x - 216.0f, origin.y + 8.0f), c.muted, agent_.modelName().c_str());

    drawTabs(ImVec2(origin.x, origin.y + titleH), size.x);
    drawAddressBar(ImVec2(origin.x, origin.y + titleH + tabsH), size.x);

    const ImVec2 pageOrigin(origin.x + gap, origin.y + titleH + tabsH + addressH + gap);
    const float pageW = assistantOpen_ ? (size.x - assistantW - gap * 3.0f) : (size.x - gap * 2.0f - 46.0f);
    const ImVec2 pageSize(std::max(180.0f, pageW), size.y - pageOrigin.y + origin.y - bottomPad);
    const ImVec2 assistantOrigin(pageOrigin.x + pageSize.x + gap, pageOrigin.y);
    const ImVec2 assistantSize(assistantW, pageSize.y);

    drawPageSurface(pageOrigin, pageSize);
    if (assistantOpen_) {
        drawAssistant(assistantOrigin, assistantSize);
    } else {
        const ImVec2 railMin(origin.x + size.x - 42.0f, pageOrigin.y + 8.0f);
        const ImVec2 railMax(origin.x + size.x - 10.0f, pageOrigin.y + 156.0f);
        draw->AddRectFilled(ImVec2(railMin.x + 3.0f, railMin.y + 5.0f), ImVec2(railMax.x + 3.0f, railMax.y + 5.0f), IM_COL32(0, 0, 0, 86), 12.0f);
        drawRoundedRect(draw, railMin, railMax, IM_COL32(22, 25, 32, 238), 12.0f, c.borderStrong);
        draw->AddText(ImVec2(railMin.x + 10.0f, railMin.y + 14.0f), c.text, ">");
        draw->AddText(ImVec2(railMin.x + 9.0f, railMin.y + 42.0f), c.muted, "A");
        draw->AddText(ImVec2(railMin.x + 9.0f, railMin.y + 60.0f), c.muted, "G");
        draw->AddText(ImVec2(railMin.x + 9.0f, railMin.y + 78.0f), c.muted, "E");
        draw->AddText(ImVec2(railMin.x + 9.0f, railMin.y + 96.0f), c.muted, "N");
        draw->AddText(ImVec2(railMin.x + 9.0f, railMin.y + 114.0f), c.muted, "T");
        ImGui::SetCursorScreenPos(railMin);
        ImGui::InvisibleButton("open-assistant-rail", ImVec2(railMax.x - railMin.x, railMax.y - railMin.y));
        if (ImGui::IsItemClicked()) {
            assistantOpen_ = true;
        }
    }

    const ImGuiIO& io = ImGui::GetIO();
    const bool imguiOwnsInput =
        addressEditing_ ||
        commandPaletteOpen_ ||
        ImGui::IsAnyItemActive() ||
        io.WantTextInput ||
        io.WantCaptureKeyboard ||
        !model_.isBrowserSurface();
    browser_.setFocus(model_.isBrowserSurface() && !imguiOwnsInput);
}

void QuantumApp::drawTabs(const ImVec2& origin, float width) {
    const auto& c = colors();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float x = origin.x + 74.0f;
    const float y = origin.y + 5.0f;
    const float tabW = 190.0f;
    const float tabH = 30.0f;

    for (int i = 0; i < static_cast<int>(model_.tabs().size()); ++i) {
        const BrowserTab& tab = model_.tabAt(i);
        const bool active = i == model_.activeIndex();
        ImVec2 min(x, y);
        ImVec2 max(x + tabW, y + tabH);
        draw->AddRectFilled(ImVec2(min.x + 4.0f, min.y + 5.0f), ImVec2(max.x + 4.0f, max.y + 7.0f), IM_COL32(0, 0, 0, active ? 70 : 35), 9.0f);
        drawRoundedRect(draw, min, max, active ? c.chrome2 : IM_COL32(20, 22, 26, 220), 8.0f, active ? c.borderStrong : c.border);
        if (active) {
            draw->AddRectFilled(ImVec2(min.x + 12.0f, min.y), ImVec2(max.x - 12.0f, min.y + 2.0f), c.accent, 1.5f);
        }
        std::string label = tab.title.empty() ? "New Tab" : tab.title;
        if (label.size() > 22) {
            label = label.substr(0, 21) + "...";
        }
        draw->AddText(ImVec2(min.x + 13.0f, min.y + 8.0f), active ? c.text : c.muted, label.c_str());
        draw->AddCircleFilled(ImVec2(max.x - 18.0f, min.y + 15.0f), 8.0f, active ? IM_COL32(255, 255, 255, 12) : IM_COL32(255, 255, 255, 6));
        draw->AddText(ImVec2(max.x - 21.5f, min.y + 7.0f), active ? c.muted : IM_COL32(105, 112, 124, 255), "x");

        ImGui::SetCursorScreenPos(min);
        ImGui::InvisibleButton(("tab" + std::to_string(i)).c_str(), ImVec2(tabW - 30.0f, tabH));
        if (ImGui::IsItemClicked()) {
            model_.activate(i);
            browser_.activateTab(tab.id);
        }
        ImGui::SetCursorScreenPos(ImVec2(max.x - 31.0f, min.y + 5.0f));
        ImGui::InvisibleButton(("tab-close" + std::to_string(i)).c_str(), ImVec2(24.0f, 22.0f));
        if (ImGui::IsItemClicked()) {
            const std::uint64_t closedId = tab.id;
            browser_.closeTab(closedId);
            model_.closeTab(i);
            browser_.activateTab(model_.activeTab().id);
        }
        x += tabW + 7.0f;
    }

    ImVec2 plusMin(std::min(x, origin.x + width - 48.0f), y);
    drawRoundedRect(draw, plusMin, ImVec2(plusMin.x + 32.0f, plusMin.y + tabH), IM_COL32(31, 34, 40, 218), 8.0f, c.border);
    draw->AddText(ImVec2(plusMin.x + 11.0f, plusMin.y + 7.0f), c.text, "+");
    ImGui::SetCursorScreenPos(plusMin);
    ImGui::InvisibleButton("new-tab", ImVec2(32.0f, tabH));
    if (ImGui::IsItemClicked()) {
        model_.openTab(profile_.settings().homeUrl);
        browser_.createTab(model_.activeTab().id, model_.activeTab().url);
        browser_.activateTab(model_.activeTab().id);
    }
}

void QuantumApp::drawAddressBar(const ImVec2& origin, float width) {
    const float navX = origin.x + 12.0f;
    const float y = origin.y + 8.0f;
    const ImVec2 buttonSize(32.0f, 32.0f);

    ImGui::SetCursorScreenPos(ImVec2(navX, y));
    drawIconButton("<", browser_.canGoBack(), buttonSize, [this]() { browser_.goBack(); });
    ImGui::SameLine(0.0f, 6.0f);
    drawIconButton(">", browser_.canGoForward(), buttonSize, [this]() { browser_.goForward(); });
    ImGui::SameLine(0.0f, 6.0f);
    drawIconButton("R", true, buttonSize, [this]() { browser_.reload(); });

    ImGui::SameLine(0.0f, 10.0f);
    const float addressW = std::max(260.0f, width - navX - 606.0f);
    ImGui::SetNextItemWidth(addressW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.11f, 0.13f, 1.0f));
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (ImGui::InputTextWithHint("##address", "Search or enter address", addressBuffer_.data(), addressBuffer_.size(), flags)) {
        submitAddress();
    }
    addressEditing_ = ImGui::IsItemActive();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::SameLine(0.0f, 8.0f);
    drawIconButton("S", true, buttonSize, [this]() {
        browser_.stop();
        agent_.stop();
    });
    ImGui::SameLine(0.0f, 8.0f);
    drawIconButton("*", true, buttonSize, [this]() {
        const auto& tab = model_.activeTab();
        if (profile_.isBookmarked(tab.url)) {
            profile_.removeBookmark(tab.url);
        } else {
            profile_.addOrUpdateBookmark(tab.url, tab.title);
        }
        profile_.save();
    });
    ImGui::SameLine(0.0f, 8.0f);
    drawIconButton("H", true, buttonSize, [this]() { model_.setSurfaceMode(BrowserSurfaceMode::History); });
    ImGui::SameLine(0.0f, 8.0f);
    drawIconButton("B", true, buttonSize, [this]() { model_.setSurfaceMode(BrowserSurfaceMode::Bookmarks); });
    ImGui::SameLine(0.0f, 8.0f);
    drawIconButton("T", true, buttonSize, [this]() { model_.setSurfaceMode(BrowserSurfaceMode::AgentTrace); });
    ImGui::SameLine(0.0f, 8.0f);
    drawIconButton("C", true, buttonSize, [this]() { model_.setSurfaceMode(BrowserSurfaceMode::ChatHistory); });
    ImGui::SameLine(0.0f, 8.0f);
    drawIconButton("D", true, buttonSize, [this]() { model_.setSurfaceMode(BrowserSurfaceMode::Downloads); });
    ImGui::SameLine(0.0f, 8.0f);
    drawIconButton("K", true, buttonSize, [this]() { commandPaletteOpen_ = true; });
}

void QuantumApp::toggleFullscreen() {
    static int lastX, lastY, lastW, lastH;
    const bool isDecorated = glfwGetWindowAttrib(window_, GLFW_DECORATED) == GLFW_TRUE;
    
    if (isDecorated) {
        // ENTER SOVEREIGN FULLSCREEN
        glfwGetWindowPos(window_, &lastX, &lastY);
        glfwGetWindowSize(window_, &lastW, &lastH);
        
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        
        glfwSetWindowAttrib(window_, GLFW_DECORATED, GLFW_FALSE);
        // TRICK: 1-pixel buffer to prevent OS exclusive mode (Ensures browser stability)
        glfwSetWindowMonitor(window_, nullptr, 0, 0, mode->width, mode->height + 1, mode->refreshRate);
    } else {
        // EXIT TO STABLE FRAME
        glfwSetWindowAttrib(window_, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(window_, nullptr, lastX, lastY, 1366, 720, 0);
    }
    
    // FORCE GOD-MODE RESURRECTION (Crucial for WebView2 Z-order)
    for (int i = 0; i < 3; ++i) {
        browser_.forceZOrder();
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); 
    }
}

void QuantumApp::drawPageSurface(const ImVec2& origin, const ImVec2& size) {
    const auto& c = colors();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    drawRoundedRect(draw, origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(17, 19, 22, 255), 8.0f, c.border);
    draw->AddRectFilled(ImVec2(origin.x + 1.0f, origin.y + 1.0f), ImVec2(origin.x + size.x - 1.0f, origin.y + 44.0f), IM_COL32(255, 255, 255, 7), 8.0f, ImDrawFlags_RoundCornersTop);

    if (model_.isBrowserSurface()) {
        const ImGuiIO& io = ImGui::GetIO();
        const float sx = io.DisplayFramebufferScale.x;
        const float sy = io.DisplayFramebufferScale.y;
        browser_.setBounds(
            static_cast<int>(origin.x * sx),
            static_cast<int>(origin.y * sy),
            static_cast<int>(size.x * sx),
            static_cast<int>(size.y * sy));
    } else {
        browser_.setBounds(0, 0, 0, 0);
    }

    if (model_.surfaceMode() == BrowserSurfaceMode::History) {
        drawHistorySurface(origin, size);
    } else if (model_.surfaceMode() == BrowserSurfaceMode::Bookmarks) {
        drawBookmarksSurface(origin, size);
    } else if (model_.surfaceMode() == BrowserSurfaceMode::AgentTrace) {
        drawAgentTraceSurface(origin, size);
    } else if (model_.surfaceMode() == BrowserSurfaceMode::ChatHistory) {
        drawChatHistorySurface(origin, size);
    } else if (model_.surfaceMode() == BrowserSurfaceMode::Downloads) {
        drawDownloadsSurface(origin, size);
    } else if (model_.surfaceMode() == BrowserSurfaceMode::Security) {
        drawSecuritySurface(origin, size);
    } else if (model_.surfaceMode() == BrowserSurfaceMode::Settings) {
        drawSettingsSurface(origin, size);
    } else if (!browser_.isNativeOverlay()) {
        drawMockPage(origin, size);
    } else if (!browser_.isReady()) {
        draw->AddText(ImVec2(origin.x + 28.0f, origin.y + 28.0f), c.muted, browser_.statusText().c_str());
    }
}

void QuantumApp::drawAssistant(const ImVec2& origin, const ImVec2& size) {
    const auto& c = colors();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(ImVec2(origin.x + 8.0f, origin.y + 10.0f), ImVec2(origin.x + size.x + 8.0f, origin.y + size.y + 10.0f), IM_COL32(0, 0, 0, 82), 12.0f);
    drawRoundedRect(draw, origin, ImVec2(origin.x + size.x, origin.y + size.y), c.panel, 12.0f, c.borderStrong);
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + 48.0f), IM_COL32(255, 255, 255, 9), 12.0f, ImDrawFlags_RoundCornersTop);
    const float t = static_cast<float>(ImGui::GetTime());
    for (int i = 0; i < 4; ++i) {
        const float y = origin.y + 76.0f + i * 44.0f + std::sin(t * 0.7f + i) * 8.0f;
        const float alpha = 16.0f + 8.0f * std::sin(t + i * 1.7f);
        draw->AddLine(ImVec2(origin.x + 18.0f, y), ImVec2(origin.x + size.x - 18.0f, y + std::cos(t + i) * 12.0f),
            IM_COL32(0, 180, 255, static_cast<int>(std::max(4.0f, alpha))), 1.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 14.0f, origin.y + 12.0f));
    ImGui::BeginChild("assistant_container", ImVec2(size.x - 28.0f, size.y - 24.0f), false, ImGuiWindowFlags_NoScrollbar);

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(c.text));
    ImGui::TextUnformatted("Assistant");
    ImGui::PopStyleColor();
    
    // ARSENAL SELECTOR
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::SetNextItemWidth(140.0f);
    const auto models = agent_.availableModels();
    std::string currentModel = agent_.modelName();
    if (ImGui::BeginCombo("##arsenal_selector", currentModel.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (const auto& model : models) {
            bool selected = (model == currentModel);
            if (ImGui::Selectable(model.c_str(), selected)) {
                agent_.setModel(model);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    
    // VISUAL PATIENCE SENTINEL (Moving Circle)
    const auto wp = agent_.waitProgress();
    if (wp.active) {
        ImGui::SameLine(0.0f, 12.0f);
        ImVec2 cp = ImGui::GetCursorScreenPos();
        cp.y += 8.0f;
        cp.x += 8.0f;
        const float radius = 6.0f;
        const ImU32 ringCol = IM_COL32(255, 255, 255, 40);
        const ImU32 progressCol = IM_COL32(0, 255, 180, 180);
        const float PI = 3.1415926535f;
        draw->AddCircle(cp, radius, ringCol, 32, 2.0f);
        draw->PathArcTo(cp, radius, -PI * 0.5f, -PI * 0.5f + (PI * 2.0f * wp.progress), 32);
        draw->PathStroke(progressCol, 0, 2.0f);
    }
    
    const float closeX = std::max(ImGui::GetCursorPosX() + 10.0f, ImGui::GetWindowContentRegionMax().x - 30.0f);
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), closeX - 34.0f));
    if (ImGui::Button("H##assistantHistory", ImVec2(26.0f, 24.0f))) {
        model_.setSurfaceMode(BrowserSurfaceMode::ChatHistory);
    }
    ImGui::SameLine();
    ImGui::SetCursorPosX(closeX);
    if (ImGui::Button("<##assistantClose", ImVec2(26.0f, 24.0f))) {
        assistantOpen_ = false;
    }

    ImGui::Spacing();
    const auto permissions = browser_.permissions();
    const auto downloads = browser_.downloads();
    if (!permissions.empty() || !downloads.empty()) {
        std::string live = std::to_string(permissions.size()) + " permissions - " + std::to_string(downloads.size()) + " downloads";
        ui::mutedTextWrapped(live);
        ImGui::Spacing();
    }
    ImVec2 taskCard = ImGui::GetCursorScreenPos();
    const float taskCardH = std::clamp(size.y * 0.20f, 74.0f, 96.0f);
    drawRoundedRect(ImGui::GetWindowDrawList(), taskCard, ImVec2(taskCard.x + ImGui::GetContentRegionAvail().x, taskCard.y + taskCardH), c.panel2, 8.0f, c.border);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);
    const bool isRunning = agent_.running();
    const std::string activeTaskId = agent_.activeTaskId();
    const bool hasActiveTask = isRunning && activeTaskId != "IDLE";
    std::string taskLabel = hasActiveTask ? ("TASK " + activeTaskId) : "ASSISTANT";
    ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "%s", taskLabel.c_str());
    ImGui::SameLine();
    ImGui::TextColored(isRunning ? ImVec4(0.3f, 1.0f, 0.7f, 1.0f) : ImVec4(0.45f, 0.50f, 0.58f, 1.0f), "%s", isRunning ? "ACTIVE" : "READY");

    const auto& liveSteps = agent_.steps();
    int doneCount = 0;
    if (hasActiveTask) {
        for (const auto& step : liveSteps) if (step.done) doneCount++;
    }
    const float progress = (!hasActiveTask || liveSteps.empty()) ? 0.0f : static_cast<float>(doneCount) / static_cast<float>(liveSteps.size());
    ImVec2 barMin = ImGui::GetCursorScreenPos();
    barMin.x += 12.0f;
    barMin.y += 5.0f;
    ImVec2 barMax(barMin.x + ImGui::GetContentRegionAvail().x - 24.0f, barMin.y + 5.0f);
    draw->AddRectFilled(barMin, barMax, IM_COL32(22, 25, 31, 255), 3.0f);
    draw->AddRectFilled(barMin, ImVec2(barMin.x + (barMax.x - barMin.x) * progress, barMax.y), IM_COL32(0, 220, 170, 210), 3.0f);
    ImGui::Dummy(ImVec2(0, 16.0f));

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 16.0f);
    if (!hasActiveTask) {
        textMuted("Ready for the next page task.");
    } else if (liveSteps.empty()) {
        textMuted("Observe -> plan exact action -> execute -> verify.");
    } else {
        const size_t visibleSteps = taskCardH < 86.0f ? 1 : 3;
        for (size_t i = 0; i < liveSteps.size() && i < visibleSteps; ++i) {
            std::string label = liveSteps[i].label;
            if (label.size() > 38) label = label.substr(0, 37) + "...";
            ImGui::TextColored(liveSteps[i].done ? ImVec4(0.38f, 0.82f, 0.54f, 1.0f) : ImVec4(0.78f, 0.80f, 0.85f, 1.0f),
                "%s %s", liveSteps[i].done ? "[x]" : "[ ]", label.c_str());
        }
    }
    ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(ImVec2(taskCard.x, taskCard.y + taskCardH + 8.0f));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    ImGui::BeginChild("messages", ImVec2(0, std::max(96.0f, size.y - 226.0f)), false);
    for (const auto& message : agent_.messages()) {
        const bool user = message.author == ChatMessage::Author::User;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(user ? c.text : c.muted));
        ImGui::TextUnformatted(authorName(message.author));
        ImGui::PopStyleColor();

        ImVec2 bubblePos = ImGui::GetCursorScreenPos();
        float bubbleW = ImGui::GetContentRegionAvail().x;
        ImVec2 textSize = ImGui::CalcTextSize(message.text.c_str(), nullptr, false, bubbleW - 22.0f);
        ImU32 fill = user ? IM_COL32(56, 66, 84, 235) : IM_COL32(33, 35, 41, 235);
        drawRoundedRect(ImGui::GetWindowDrawList(), bubblePos, ImVec2(bubblePos.x + bubbleW, bubblePos.y + textSize.y + 22.0f), fill, 8.0f, c.border);
        ImGui::SetCursorScreenPos(ImVec2(bubblePos.x + 11.0f, bubblePos.y + 10.0f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + bubbleW - 22.0f);
        ImGui::TextUnformatted(message.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetCursorScreenPos(ImVec2(bubblePos.x, bubblePos.y + textSize.y + 30.0f));
        ImGui::Dummy(ImVec2(0, 0)); // Anchor boundary to prevent grow-error
    }
    if (agent_.busy()) {
        ImGui::Spacing();
        for (const auto& step : agent_.steps()) {
            ImGui::TextColored(step.done ? ImVec4(0.38f, 0.82f, 0.54f, 1.0f) : ImVec4(0.78f, 0.80f, 0.85f, 1.0f),
                "%s %s", step.done ? "[x]" : "...", step.label.c_str());
        }
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 24.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    const bool isBusy = agent_.busy();

    const auto pendingImages = agent_.pendingImageLabels();
    const float pendingImageH = pendingImages.empty() ? 0.0f : 24.0f;
    ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY() + 4.0f, size.y - 74.0f - pendingImageH));
    if (!pendingImages.empty()) {
        std::string imageLine = "Image context: " + pendingImages.front();
        if (pendingImages.size() > 1) {
            imageLine += " +" + std::to_string(pendingImages.size() - 1);
        }
        ui::mutedTextWrapped(imageLine);
        ImGui::SameLine();
        if (ImGui::SmallButton("clear##pendingImages")) {
            agent_.clearPendingImages();
        }
    }

    if (ImGui::Button("+##agentHook", ImVec2(32.0f, 34.0f))) {
        browser_.setFocus(false);
        ImGui::OpenPopup("AssistantImageHook");
    }
    if (ImGui::BeginPopup("AssistantImageHook")) {
        if (ImGui::Selectable("Attach image...")) {
            attachImageFromDialog();
        }
        if (ImGui::Selectable("Browser snapshot")) {
            attachBrowserSnapshot();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, 8.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.23f, 0.24f, 0.28f, 1.0f));
    ImGui::SetNextItemWidth(std::max(64.0f, ImGui::GetContentRegionAvail().x - 44.0f));
    if (isBusy) ImGui::BeginDisabled();
    if (ImGui::InputTextWithHint("##agentPrompt", isBusy ? "Quantum is executing the task..." : "Give Quantum a page task or target...", promptBuffer_.data(), promptBuffer_.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
        submitAgentPrompt();
    }
    if (ImGui::IsItemActive()) {
        browser_.setFocus(false);
    }
    
    if (isBusy) ImGui::EndDisabled();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SameLine(0.0f, 8.0f);
    
    if (isBusy) ImGui::BeginDisabled();
    if (ImGui::Button(">", ImVec2(34.0f, 34.0f))) {
        submitAgentPrompt();
    }
    if (isBusy) ImGui::EndDisabled();

    ImGui::EndChild();
}

void QuantumApp::drawMockPage(const ImVec2& origin, const ImVec2& size) {
    const auto& c = colors();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 center(origin.x + size.x * 0.5f, origin.y + size.y * 0.48f);

    for (int i = 0; i < 12; ++i) {
        float w = size.x * (0.22f + 0.03f * (i % 4));
        float y = origin.y + 72.0f + i * 28.0f;
        draw->AddRectFilled(ImVec2(origin.x + 46.0f, y), ImVec2(origin.x + 46.0f + w, y + 10.0f), IM_COL32(32, 35, 39, 180), 3.0f);
    }

    if (agent_.busy()) {
        ImVec2 min(center.x - 112.0f, center.y - 23.0f);
        ImVec2 max(center.x + 112.0f, center.y + 23.0f);
        draw->AddRectFilled(ImVec2(min.x + 4.0f, min.y + 8.0f), ImVec2(max.x + 4.0f, max.y + 8.0f), IM_COL32(0, 0, 0, 110), 16.0f);
        drawRoundedRect(draw, min, max, IM_COL32(4, 5, 8, 245), 16.0f, c.borderStrong);
        draw->AddText(ImVec2(min.x + 22.0f, min.y + 14.0f), c.text, "Stop Quantum Assistant");
        ImGui::SetCursorScreenPos(min);
        ImGui::InvisibleButton("stop-agent-overlay", ImVec2(max.x - min.x, max.y - min.y));
        if (ImGui::IsItemClicked()) {
            agent_.stop();
        }
    }

    draw->AddText(ImVec2(origin.x + 24.0f, origin.y + size.y - 34.0f), c.muted, browser_.statusText().c_str());
    ImGui::Dummy(ImVec2(0, 0)); // Satisfy boundary sentinel
}

void QuantumApp::drawHistorySurface(const ImVec2& origin, const ImVec2& size) {
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - 44.0f, origin.y + 14.0f));
    if (ImGui::Button("X##closeHistory", ImVec2(30.0f, 30.0f))) {
        model_.setSurfaceMode(BrowserSurfaceMode::Web);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 22.0f, origin.y + 18.0f));
    ImGui::BeginChild("historySurface", ImVec2(size.x - 44.0f, size.y - 36.0f), false);
    ImGui::TextUnformatted("History");
    ui::sectionLabel("Recent pages");
    ImGui::Spacing();
    for (const auto& entry : profile_.history()) {
        if (ui::commandRow(entry.title.c_str(), entry.url.c_str(), false)) {
            model_.setSurfaceMode(BrowserSurfaceMode::Web);
            browser_.navigate(entry.url);
        }
    }
    ImGui::EndChild();
}

void QuantumApp::drawBookmarksSurface(const ImVec2& origin, const ImVec2& size) {
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - 44.0f, origin.y + 14.0f));
    if (ImGui::Button("X##closeBookmarks", ImVec2(30.0f, 30.0f))) {
        model_.setSurfaceMode(BrowserSurfaceMode::Web);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 22.0f, origin.y + 18.0f));
    ImGui::BeginChild("bookmarksSurface", ImVec2(size.x - 44.0f, size.y - 36.0f), false);
    ImGui::TextUnformatted("Bookmarks");
    ui::sectionLabel("Saved pages");
    ImGui::Spacing();
    for (const auto& entry : profile_.bookmarks()) {
        if (ui::commandRow(entry.title.c_str(), entry.url.c_str(), false)) {
            model_.setSurfaceMode(BrowserSurfaceMode::Web);
            browser_.navigate(entry.url);
        }
    }
    ImGui::EndChild();
}

void QuantumApp::drawAgentTraceSurface(const ImVec2& origin, const ImVec2& size) {
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - 44.0f, origin.y + 14.0f));
    if (ImGui::Button("X##closeTrace", ImVec2(30.0f, 30.0f))) {
        model_.setSurfaceMode(BrowserSurfaceMode::Web);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 22.0f, origin.y + 18.0f));
    ImGui::BeginChild("traceSurface", ImVec2(size.x - 44.0f, size.y - 36.0f), false);
    ImGui::TextUnformatted("Agent Trace");
    ui::sectionLabel("Observe, plan, execute timeline");
    ImGui::Spacing();
    for (const auto& entry : agent_.trace().latest(120)) {
        std::string title = std::string("[") + traceKindLabel(entry.kind) + "] " + entry.title;
        if (ui::commandRow(title.c_str(), entry.detail.c_str(), false)) {
        }
    }
    ImGui::EndChild();
}

void QuantumApp::drawChatHistorySurface(const ImVec2& origin, const ImVec2& size) {
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - 44.0f, origin.y + 14.0f));
    if (ImGui::Button("X##closeChatHistory", ImVec2(30.0f, 30.0f))) {
        model_.setSurfaceMode(BrowserSurfaceMode::Web);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 22.0f, origin.y + 18.0f));
    ImGui::BeginChild("chatHistorySurface", ImVec2(size.x - 44.0f, size.y - 36.0f), false);
    ImGui::TextUnformatted("AI Chat History");
    ui::sectionLabel("Quantum assistant memory for this session");
    ImGui::Spacing();
    const auto& messages = agent_.messages();
    if (messages.empty()) {
        ui::mutedTextWrapped("No assistant messages yet.");
    }
    for (size_t i = 0; i < messages.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ui::commandRow(authorName(messages[i].author), messages[i].text.c_str(), false);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void QuantumApp::drawDownloadsSurface(const ImVec2& origin, const ImVec2& size) {
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - 44.0f, origin.y + 14.0f));
    if (ImGui::Button("X##closeDownloads", ImVec2(30.0f, 30.0f))) {
        model_.setSurfaceMode(BrowserSurfaceMode::Web);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 22.0f, origin.y + 18.0f));
    ImGui::BeginChild("downloadsSurface", ImVec2(size.x - 44.0f, size.y - 36.0f), false);
    ImGui::TextUnformatted("Downloads");
    ui::sectionLabel("Custom Quantum download monitor");
    ImGui::Spacing();
    const auto downloads = browser_.downloads();
    if (downloads.empty()) {
        ui::mutedTextWrapped("Downloads will appear here when pages start saving files.");
    }
    for (const auto& item : downloads) {
        std::string subtitle = item.state + " - " + item.filePath;
        if (item.totalBytes > 0) {
            subtitle += " - " + std::to_string(item.receivedBytes / 1024) + "KB / " + std::to_string(item.totalBytes / 1024) + "KB";
        }
        ui::commandRow(item.uri.empty() ? "Download" : item.uri.c_str(), subtitle.c_str(), false);
        if (item.state == "Downloading") {
            if (ImGui::Button(("Pause##download" + std::to_string(item.id)).c_str(), ImVec2(88.0f, 30.0f))) {
                browser_.pauseDownload(item.id);
            }
            ImGui::SameLine();
            if (ImGui::Button(("Cancel##download" + std::to_string(item.id)).c_str(), ImVec2(88.0f, 30.0f))) {
                browser_.cancelDownload(item.id);
            }
        } else if (item.state == "Interrupted") {
            if (ImGui::Button(("Resume##download" + std::to_string(item.id)).c_str(), ImVec2(88.0f, 30.0f))) {
                browser_.resumeDownload(item.id);
            }
        }
        ImGui::Spacing();
    }
    ImGui::EndChild();
}

void QuantumApp::drawSecuritySurface(const ImVec2& origin, const ImVec2& size) {
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - 44.0f, origin.y + 14.0f));
    if (ImGui::Button("X##closeSecurity", ImVec2(30.0f, 30.0f))) {
        model_.setSurfaceMode(BrowserSurfaceMode::Web);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 22.0f, origin.y + 18.0f));
    ImGui::BeginChild("securitySurface", ImVec2(size.x - 44.0f, size.y - 36.0f), false);
    ImGui::TextUnformatted("Permissions");
    ui::sectionLabel("Site requests handled by Quantum");
    ImGui::Spacing();
    const auto permissions = browser_.permissions();
    if (permissions.empty()) {
        ui::mutedTextWrapped("Camera, microphone, geolocation, notifications, clipboard, file, and download requests appear here.");
    }
    for (const auto& request : permissions) {
        std::string subtitle = request.uri + " - " + request.decision + (request.userInitiated ? " - user initiated" : "");
        ui::commandRow(request.kind.c_str(), subtitle.c_str(), request.pending);
        if (request.pending) {
            if (ImGui::Button(("Allow##perm" + std::to_string(request.id)).c_str(), ImVec2(88.0f, 30.0f))) {
                browser_.resolvePermission(request.id, true);
            }
            ImGui::SameLine();
            if (ImGui::Button(("Deny##perm" + std::to_string(request.id)).c_str(), ImVec2(88.0f, 30.0f))) {
                browser_.resolvePermission(request.id, false);
            }
            ImGui::Spacing();
        }
    }
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Credential Vault");
    ui::sectionLabel("DPAPI encrypted local records");
    const auto credentials = vault_.records();
    if (credentials.empty()) {
        ui::mutedTextWrapped("No saved credentials yet. The encrypted vault is ready for autofill flows.");
    }
    for (const auto& credential : credentials) {
        ui::commandRow(credential.origin.c_str(), credential.username.c_str(), false);
    }
    ImGui::EndChild();
}

void QuantumApp::drawSettingsSurface(const ImVec2& origin, const ImVec2& size) {
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - 44.0f, origin.y + 14.0f));
    if (ImGui::Button("X##closeSettings", ImVec2(30.0f, 30.0f))) {
        model_.setSurfaceMode(BrowserSurfaceMode::Web);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 22.0f, origin.y + 18.0f));
    ImGui::BeginChild("settingsSurface", ImVec2(size.x - 44.0f, size.y - 36.0f), false);
    ImGui::TextUnformatted("Settings");
    ui::sectionLabel("Quantum profile");
    ImGui::Spacing();
    ImGui::Checkbox("Restore session", &profile_.settings().restoreSession);
    ImGui::Checkbox("Compact tabs", &profile_.settings().compactTabs);
    ImGui::Checkbox("Show agent trace", &profile_.settings().showAgentTrace);
    ImGui::InputInt("Max history", &profile_.settings().maxHistory);
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("AI Core");
    ui::sectionLabel("Gemini model and masked API key");
    std::string currentModel = agent_.modelName();
    ImGui::SetNextItemWidth(std::min(320.0f, ImGui::GetContentRegionAvail().x));
    if (ImGui::BeginCombo("Model##settingsModel", currentModel.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (const auto& model : agent_.availableModels()) {
            const bool selected = model == currentModel;
            if (ImGui::Selectable(model.c_str(), selected)) {
                agent_.setModel(model);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Checkbox("Show API key", &showApiKey_);
    ImGui::SetNextItemWidth(std::min(520.0f, ImGui::GetContentRegionAvail().x));
    ImGuiInputTextFlags keyFlags = showApiKey_ ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password;
    ImGui::InputTextWithHint("##geminiApiKey", "Paste Gemini API key", apiKeyBuffer_.data(), apiKeyBuffer_.size(), keyFlags);
    if (ImGui::IsItemActive()) {
        browser_.setFocus(false);
    }
    if (ImGui::Button("Save API key", ImVec2(140.0f, 34.0f))) {
        profile_.settings().geminiApiKey = apiKeyBuffer_.data();
        profile_.save();
        agent_.setApiKey(profile_.settings().geminiApiKey);
        lastUiNotice_ = "Quantum API key updated.";
    }
    ImGui::SameLine();
    ui::mutedTextWrapped("Stored: " + agent_.maskedApiKey());
    ImGui::Separator();
    if (ImGui::Button("Save settings", ImVec2(140.0f, 34.0f))) {
        profile_.save();
    }
    ImGui::EndChild();
}

void QuantumApp::drawCommandPalette(const ImVec2& rootOrigin, const ImVec2& rootSize) {
    if (!commandPaletteOpen_) {
        return;
    }

    const ImVec2 size(620.0f, 430.0f);
    const ImVec2 pos(rootOrigin.x + rootSize.x * 0.5f - size.x * 0.5f, rootOrigin.y + 86.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(rootOrigin, ImVec2(rootOrigin.x + rootSize.x, rootOrigin.y + rootSize.y), IM_COL32(0, 0, 0, 96));
    ui::roundedPanel(draw, pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(28, 30, 36, 248), colors().borderStrong, 12.0f);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + 18.0f, pos.y + 16.0f));
    ImGui::BeginChild("commandPalette", ImVec2(size.x - 36.0f, size.y - 32.0f), false);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputTextWithHint("##command", "Command or URL", commandBuffer_.data(), commandBuffer_.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
        runCommand(commandBuffer_.data());
        commandPaletteOpen_ = false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        commandPaletteOpen_ = false;
    }
    ImGui::Spacing();
    if (ui::commandRow("Open History", "Show recent navigation", false)) {
        runCommand("history");
    }
    if (ui::commandRow("Open Bookmarks", "Show saved pages", false)) {
        runCommand("bookmarks");
    }
    if (ui::commandRow("Open Agent Trace", "Inspect model and action timeline", false)) {
        runCommand("trace");
    }
    if (ui::commandRow("Open AI Chat History", "Review assistant conversation", false)) {
        runCommand("chat history");
    }
    if (ui::commandRow("Open Downloads", "Inspect file transfers", false)) {
        runCommand("downloads");
    }
    if (ui::commandRow("Open Permissions", "Review site access requests", false)) {
        runCommand("permissions");
    }
    if (ui::commandRow("Capture Screenshot", "Save current page as PNG", false)) {
        runCommand("screenshot");
    }
    if (ui::commandRow("Open Settings", "Tune Quantum profile", false)) {
        runCommand("settings");
    }
    if (ui::commandRow("New Tab", profile_.settings().homeUrl.c_str(), false)) {
        runCommand("new tab");
    }
    ImGui::Spacing();
    CommandIndex index(profile_);
    for (const auto& hit : index.search(commandBuffer_.data(), 8)) {
        if (ui::commandRow(hit.title.c_str(), hit.subtitle.c_str(), false)) {
            runCommand(hit.command);
        }
    }
    ImGui::EndChild();
}

void QuantumApp::drawIconButton(const char* label, bool enabled, const ImVec2& size, const std::function<void()>& onClick) {
    if (!enabled) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(label, size) && enabled && onClick) {
        onClick();
    }
    if (!enabled) {
        ImGui::EndDisabled();
    }
}

void QuantumApp::syncActiveTabFromBrowser() {
    if (!model_.isBrowserSurface()) {
        return;
    }
    const std::string url = browser_.currentUrl();
    const std::string title = browser_.title();
    model_.updateActiveFromBrowser(url, title, browser_.statusText() == "Loading");
    if (!addressEditing_) {
        std::snprintf(addressBuffer_.data(), addressBuffer_.size(), "%s", model_.activeTab().url.c_str());
    }
    if (!url.empty() && url != "about:blank" && url != lastRecordedUrl_ && browser_.statusText() == "Ready") {
        profile_.addHistory(url, title);
        lastRecordedUrl_ = url;
    }
}

void QuantumApp::submitAddress() {
    const std::string target = addressBuffer_.data();
    model_.setSurfaceMode(BrowserSurfaceMode::Web);
    browser_.navigate(target);
}

void QuantumApp::submitAgentPrompt() {
    const std::string prompt = promptBuffer_.data();
    if (prompt.empty()) {
        return;
    }
    std::fill(promptBuffer_.begin(), promptBuffer_.end(), '\0');
    agent_.submit(prompt, browser_);
}

void QuantumApp::attachImageFromDialog() {
    browser_.setFocus(false);
    const auto file = openImageFileDialog();
    if (file.empty()) {
        lastUiNotice_ = "Image attach cancelled.";
        return;
    }
    if (agent_.attachImage(file, file.filename().string())) {
        lastUiNotice_ = "Image attached: " + file.filename().string();
    }
}

void QuantumApp::attachBrowserSnapshot() {
    browser_.setFocus(false);
    const auto dir = profile_.paths().dataDir() / "AgentImages";
    const auto file = dir / ("browser_snapshot_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".png");
    browser_.captureScreenshot(file, [this, file](bool ok, std::string message) {
        if (ok && agent_.attachImage(file, "Browser snapshot")) {
            lastUiNotice_ = "Browser snapshot attached.";
        } else {
            lastUiNotice_ = "Browser snapshot failed: " + message;
        }
    });
}

void QuantumApp::runCommand(const std::string& command) {
    commandPaletteOpen_ = false;
    if (command == "history") {
        model_.setSurfaceMode(BrowserSurfaceMode::History);
    } else if (command == "bookmarks") {
        model_.setSurfaceMode(BrowserSurfaceMode::Bookmarks);
    } else if (command == "trace") {
        model_.setSurfaceMode(BrowserSurfaceMode::AgentTrace);
    } else if (command == "chat history" || command == "chat") {
        model_.setSurfaceMode(BrowserSurfaceMode::ChatHistory);
    } else if (command == "downloads") {
        model_.setSurfaceMode(BrowserSurfaceMode::Downloads);
    } else if (command == "permissions") {
        model_.setSurfaceMode(BrowserSurfaceMode::Security);
    } else if (command == "settings") {
        model_.setSurfaceMode(BrowserSurfaceMode::Settings);
    } else if (command == "screenshot") {
        const auto dir = profile_.paths().dataDir() / "Screenshots";
        const auto file = dir / ("quantum_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".png");
        browser_.captureScreenshot(file, [this](bool ok, std::string message) {
            lastUiNotice_ = ok ? "Screenshot saved: " + message : "Screenshot failed: " + message;
        });
    } else if (command == "new tab") {
        model_.openTab(profile_.settings().homeUrl);
        browser_.createTab(model_.activeTab().id, model_.activeTab().url);
        browser_.activateTab(model_.activeTab().id);
    } else if (!command.empty()) {
        model_.setSurfaceMode(BrowserSurfaceMode::Web);
        browser_.navigate(command);
    }
}

void QuantumApp::handleWindowDrag() {
    ImGui::SetCursorScreenPos(ImGui::GetWindowPos());
    ImGui::InvisibleButton("drag-zone", ImVec2(ImGui::GetWindowSize().x, 30.0f));

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (!windowDragging_) {
            windowDragging_ = true;
            dragAnchor_ = ImGui::GetMousePos();
            glfwGetWindowPos(window_, &dragWindowX_, &dragWindowY_);
        }
        ImVec2 mouse = ImGui::GetMousePos();
        glfwSetWindowPos(
            window_,
            dragWindowX_ + static_cast<int>(mouse.x - dragAnchor_.x),
            dragWindowY_ + static_cast<int>(mouse.y - dragAnchor_.y));
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        windowDragging_ = false;
    }
}

} // namespace quantum
