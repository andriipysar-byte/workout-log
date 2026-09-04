#include "root_view.hpp"

#include <imgui_stdlib.h>

#include <array>
#include <filesystem>
#include <span>
#include <system_error>

#include "theme.hpp"
#include "widgets.hpp"

namespace workoutlog::ui::root_view {

namespace {

void draw_sidebar(Platform& platform, AppModel& model, State& state) {
    static constexpr std::array<Tab, 2> kTabs = {Tab::list, Tab::cycle};
    Tab tab = state.tab;
    if (widgets::segmented<Tab>("tab_picker", std::span<const Tab>(kTabs), tab,
                                 [](Tab t) { return std::string(t == Tab::list ? "List" : "Cycle"); })) {
        state.tab = tab;
    }
    ImGui::Separator();

    ImGui::BeginDisabled(state.folder_dialog_pending);
    if (ImGui::Button("Folder")) {
        platform.request_folder_dialog(model.folder());
        state.folder_dialog_pending = true;
        state.folder_dialog_error.reset();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reload")) model.refresh();

    // In-app fallback: the native dialog needs an XDG portal or zenity on Linux,
    // neither of which is guaranteed to be present (ui/platform.hpp).
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##folder_path", "Or type a folder path and press Enter", &state.folder_path_input);
    if (ImGui::IsItemDeactivatedAfterEdit() && !state.folder_path_input.empty()) {
        std::error_code ec;
        if (std::filesystem::is_directory(state.folder_path_input, ec)) {
            model.set_folder(state.folder_path_input);
            state.folder_dialog_error.reset();
        } else {
            state.folder_dialog_error = "\"" + state.folder_path_input + "\" is not a directory";
        }
        state.folder_path_input.clear();
    }
    if (state.folder_dialog_error.has_value()) {
        ImGui::TextColored(ImVec4(0.85f, 0.30f, 0.30f, 1.0f), "%s", state.folder_dialog_error->c_str());
    }

    ImGui::Separator();

    ImGui::BeginChild("file_list");
    const auto& selection = model.selection();
    for (const auto& file : model.files()) {
        const std::string label = file.stem().string();
        const bool selected = selection.has_value() && selection.value() == file;
        if (ImGui::Selectable(label.c_str(), selected)) model.open(file);
    }
    ImGui::EndChild();
}

void draw_detail(SDL_Renderer& renderer, AppModel& model, State& state, ImFont* mono_font) {
    if (state.tab == Tab::cycle) {
        // Faithful-divergence from Views.swift: clicking a calendar day there
        // loads the session but leaves the sidebar on the Cycle tab, so nothing
        // visibly happens until the user flips the picker themselves. Switching
        // back to List here is a one-line, obviously better fix, not a corner cut.
        if (cycle_view::draw(renderer, model, state.cycle)) state.tab = Tab::list;
        return;
    }
    if (model.has_session()) {
        session_editor::draw(renderer, model, state.editor, mono_font);
    } else {
        ImGui::TextDisabled("Select a session to edit");
    }
}

} // namespace

void draw(Platform& platform, AppModel& model, State& state, ImFont* mono_font) {
    if (auto result = platform.take_folder_result()) {
        model.set_folder(*result);
        state.folder_dialog_pending = false;
        state.folder_dialog_error.reset();
    }
    if (auto err = platform.take_folder_error()) {
        state.folder_dialog_pending = false;
        state.folder_dialog_error = *err;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    constexpr ImGuiWindowFlags kHostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                             ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##root", nullptr, kHostFlags);
    ImGui::PopStyleVar(3);

    const float status_bar_h = ImGui::GetFrameHeightWithSpacing();

    ImGui::BeginChild("sidebar", ImVec2(theme::tokens::sidebar_min_width, -status_bar_h),
                       ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders);
    draw_sidebar(platform, model, state);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("detail", ImVec2(0.0f, -status_bar_h));
    draw_detail(*platform.renderer(), model, state, mono_font);
    ImGui::EndChild();

    ImGui::Separator();
    const std::string status_text = model.status().empty() ? model.folder().string() : model.status();
    ImGui::TextUnformatted(status_text.c_str());

    ImGui::End();
}

} // namespace workoutlog::ui::root_view
