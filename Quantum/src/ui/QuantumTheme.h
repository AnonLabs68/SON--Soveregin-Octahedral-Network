#pragma once

#include <imgui.h>

namespace quantum {

struct QuantumColors {
    ImU32 bg = IM_COL32(13, 15, 18, 255);
    ImU32 chrome = IM_COL32(20, 22, 26, 255);
    ImU32 chrome2 = IM_COL32(34, 37, 44, 255);
    ImU32 panel = IM_COL32(34, 37, 43, 242);
    ImU32 panel2 = IM_COL32(48, 52, 60, 228);
    ImU32 border = IM_COL32(255, 255, 255, 24);
    ImU32 borderStrong = IM_COL32(255, 255, 255, 42);
    ImU32 text = IM_COL32(235, 238, 244, 255);
    ImU32 muted = IM_COL32(142, 148, 158, 255);
    ImU32 accent = IM_COL32(121, 170, 255, 255);
    ImU32 accentSoft = IM_COL32(121, 170, 255, 42);
    ImU32 green = IM_COL32(81, 209, 137, 255);
    ImU32 warning = IM_COL32(239, 184, 82, 255);
};

void applyQuantumStyle();
const QuantumColors& colors();

} // namespace quantum
