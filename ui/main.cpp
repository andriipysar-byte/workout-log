#include "app_model.hpp"
#include "fonts.hpp"
#include "platform.hpp"
#include "session_editor.hpp"
#include "svg_texture.hpp"
#include "theme.hpp"

#include <imgui.h>

#include <iostream>

using namespace workoutlog::ui;

int main() {
    try {
        Platform platform;
        float scale = platform.display_scale();
        if (scale <= 0.0f) scale = 1.0f;
        auto resolved_fonts = fonts::load(*ImGui::GetIO().Fonts, 15.0f * scale, 14.0f * scale);
        theme::apply(ImGui::GetStyle(), scale);
        if (resolved_fonts.body_path) register_fallback_font(*resolved_fonts.body_path);

        AppModel model;
        // Pick the real hand-transcribed session (has strength/cardio/metcon/cooldown
        // blocks), not one of the generated weightless stubs.
        for (const auto& f : model.files()) {
            if (f.filename().string().rfind("2026-06-23", 0) == 0) model.open(f);
        }
        std::cerr << "opened: " << model.status() << "\n";

        session_editor::State state;

        int frame = 0;
        while (platform.pump_events() && frame < 100000) {
            platform.begin_frame();
            ImGui::PushFont(resolved_fonts.body);

            const ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(vp->WorkPos);
            ImGui::SetNextWindowSize(vp->WorkSize);
            ImGui::Begin("Session check", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
            if (model.has_session()) {
                session_editor::draw(*platform.renderer(), model, state, resolved_fonts.mono);
            } else {
                ImGui::TextUnformatted("no session loaded");
            }
            ImGui::End();

            ImGui::PopFont();
            platform.end_frame(30, 30, 34);
            frame++;
        }
    } catch (const std::exception& e) {
        std::cerr << "wl_ui session test: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
