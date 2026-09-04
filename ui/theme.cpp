#include "theme.hpp"

#include <array>

namespace workoutlog::ui::theme {

void apply(ImGuiStyle& style, float display_scale) {
    ImGui::StyleColorsDark(&style);

    // The host window is an invisible, full-viewport frame (root_view); rounding
    // and borders belong to the cards and controls drawn inside it, not to it.
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildRounding = 0.0f; // cards push their own ChildRounding (theme::tokens::card_rounding)
    style.PopupRounding = 6.0f;
    style.FrameRounding = 6.0f; // approximates .textFieldStyle(.roundedBorder)
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 16.0f;

    const ImU32 accent = rgba(kAccentHex);
    const ImVec4 accent_f = ImGui::ColorConvertU32ToFloat4(accent);
    const ImVec4 accent_hover = ImVec4(accent_f.x, accent_f.y, accent_f.z, 0.8f);
    const ImVec4 accent_soft = ImVec4(accent_f.x, accent_f.y, accent_f.z, 0.18f); // selection fill, matches
                                                                                   // Color.accentColor.opacity(0.18)

    style.Colors[ImGuiCol_CheckMark] = accent_f;
    style.Colors[ImGuiCol_SliderGrab] = accent_f;
    style.Colors[ImGuiCol_SliderGrabActive] = accent_hover;
    style.Colors[ImGuiCol_Button] = ImVec4(accent_f.x, accent_f.y, accent_f.z, 0.30f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(accent_f.x, accent_f.y, accent_f.z, 0.55f);
    style.Colors[ImGuiCol_ButtonActive] = accent_f;
    style.Colors[ImGuiCol_Header] = accent_soft;
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(accent_f.x, accent_f.y, accent_f.z, 0.35f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(accent_f.x, accent_f.y, accent_f.z, 0.45f);
    style.Colors[ImGuiCol_SeparatorHovered] = accent_hover;
    style.Colors[ImGuiCol_SeparatorActive] = accent_f;
    style.Colors[ImGuiCol_TabSelected] = ImVec4(accent_f.x, accent_f.y, accent_f.z, 0.55f);
    style.Colors[ImGuiCol_TabSelectedOverline] = accent_f;

    // controlBackgroundColor-ish: a touch lighter than WindowBg, used for cards,
    // the muscle-map frame and the cycle table's header row.
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.13f, 0.145f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.175f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.205f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.21f, 0.21f, 0.225f, 1.0f);

    style.ScaleAllSizes(display_scale);
}

ImU32 rgba(std::string_view hex, float alpha) {
    std::string_view h = hex;
    if (!h.empty() && h.front() == '#') h.remove_prefix(1);

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    std::array<int, 3> channel{0, 0, 0};
    bool ok = h.size() == 6;
    for (size_t i = 0; ok && i < channel.size(); i++) {
        int hi = nibble(h[i * 2]);
        int lo = nibble(h[i * 2 + 1]);
        ok = hi >= 0 && lo >= 0;
        if (ok) channel.at(i) = hi * 16 + lo;
    }
    if (!ok) return ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.0f, 1.0f, alpha));

    return ImGui::ColorConvertFloat4ToU32(ImVec4(static_cast<float>(channel.at(0)) / 255.0f,
                                                  static_cast<float>(channel.at(1)) / 255.0f,
                                                  static_cast<float>(channel.at(2)) / 255.0f, alpha));
}

CornerColors group_gradient(std::span<const MuscleGroup> groups, double base_opacity) {
    if (groups.empty()) {
        const ImU32 clear = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        return {clear, clear, clear, clear};
    }

    ImU32 top_left{};
    ImU32 bottom_right{};
    if (groups.size() == 1) {
        top_left = rgba(workoutlog::hex(groups[0]), static_cast<float>(base_opacity * 1.4));
        bottom_right = rgba(workoutlog::hex(groups[0]), static_cast<float>(base_opacity * 0.5));
    } else {
        top_left = rgba(workoutlog::hex(groups.front()), static_cast<float>(base_opacity));
        bottom_right = rgba(workoutlog::hex(groups.back()), static_cast<float>(base_opacity));
    }

    const ImVec4 tl_f = ImGui::ColorConvertU32ToFloat4(top_left);
    const ImVec4 br_f = ImGui::ColorConvertU32ToFloat4(bottom_right);
    const ImU32 mid = ImGui::ColorConvertFloat4ToU32(
        ImVec4((tl_f.x + br_f.x) * 0.5f, (tl_f.y + br_f.y) * 0.5f, (tl_f.z + br_f.z) * 0.5f, (tl_f.w + br_f.w) * 0.5f));

    return {top_left, mid, bottom_right, mid};
}

} // namespace workoutlog::ui::theme
