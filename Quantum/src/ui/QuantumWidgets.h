#pragma once

#include <imgui.h>

#include <functional>
#include <string>

namespace quantum::ui {

void roundedPanel(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 fill, ImU32 border, float radius = 8.0f);
bool iconButton(const char* id, const char* label, ImVec2 size, bool enabled = true);
void sectionLabel(const char* text);
void mutedTextWrapped(const std::string& text);
bool commandRow(const char* title, const char* subtitle, bool selected);

} // namespace quantum::ui

