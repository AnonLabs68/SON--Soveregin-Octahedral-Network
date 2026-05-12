#include "ui/QuantumWidgets.h"

#include "ui/QuantumTheme.h"

namespace quantum::ui {

void roundedPanel(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 fill, ImU32 border, float radius) {
    draw->AddRectFilled(min, max, fill, radius);
    draw->AddRect(min, max, border, radius);
}

bool iconButton(const char* id, const char* label, ImVec2 size, bool enabled) {
    if (!enabled) {
        ImGui::BeginDisabled();
    }
    ImGui::PushID(id);
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopID();
    if (!enabled) {
        ImGui::EndDisabled();
    }
    return enabled && clicked;
}

void sectionLabel(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(colors().muted));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void mutedTextWrapped(const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(colors().muted));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

bool commandRow(const char* title, const char* subtitle, bool selected) {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = 52.0f;
    const auto& c = colors();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    roundedPanel(draw, start, ImVec2(start.x + width, start.y + height),
        selected ? IM_COL32(52, 58, 70, 245) : IM_COL32(32, 35, 41, 210),
        selected ? c.borderStrong : c.border,
        8.0f);
    ImGui::SetCursorScreenPos(ImVec2(start.x + 12.0f, start.y + 8.0f));
    ImGui::TextUnformatted(title);
    ImGui::SetCursorScreenPos(ImVec2(start.x + 12.0f, start.y + 28.0f));
    sectionLabel(subtitle);
    ImGui::SetCursorScreenPos(start);
    ImGui::PushID(title ? title : "");
    ImGui::PushID(subtitle ? subtitle : "");
    ImGui::InvisibleButton("##commandRowButton", ImVec2(width, height));
    const bool clicked = ImGui::IsItemClicked();
    ImGui::PopID();
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    return clicked;
}

} // namespace quantum::ui
