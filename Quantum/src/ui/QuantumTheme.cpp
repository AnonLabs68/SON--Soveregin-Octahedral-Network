#include "ui/QuantumTheme.h"

namespace quantum {

const QuantumColors& colors() {
    static QuantumColors c;
    return c;
}

void applyQuantumStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(12, 8);
    style.ItemSpacing = ImVec2(8, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 7.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;

    ImVec4* col = style.Colors;
    col[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.96f, 1.0f);
    col[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.55f, 0.61f, 1.0f);
    col[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.06f, 0.07f, 1.0f);
    col[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.12f, 0.14f, 0.94f);
    col[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.13f, 0.16f, 0.98f);
    col[ImGuiCol_Border] = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
    col[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.20f, 1.0f);
    col[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
    col[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.25f, 0.30f, 1.0f);
    col[ImGuiCol_Button] = ImVec4(0.17f, 0.18f, 0.21f, 1.0f);
    col[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.25f, 0.29f, 1.0f);
    col[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.31f, 0.36f, 1.0f);
    col[ImGuiCol_Header] = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);
    col[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.30f, 0.36f, 1.0f);
    col[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.36f, 0.43f, 1.0f);
    col[ImGuiCol_Separator] = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
    col[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.06f, 0.30f);
    col[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.37f, 0.42f, 0.65f);
    col[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.49f, 0.56f, 0.8f);
}

} // namespace quantum

