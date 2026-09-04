#include "app_model.hpp"
#include "fonts.hpp"
#include "platform.hpp"
#include "root_view.hpp"
#include "svg_texture.hpp"
#include "theme.hpp"

#include <imgui.h>

#include <iostream>

using namespace workoutlog::ui;

int main() {
    try {
        Platform platform;

        float scale = platform.display_scale();
        if (scale <= 0.0f) scale = 1.0f; // SDL_GetWindowDisplayScale can return 0 before the first frame

        const auto resolved_fonts = fonts::load(*ImGui::GetIO().Fonts, 16.0f * scale, 15.0f * scale);
        theme::apply(ImGui::GetStyle(), scale);
        if (resolved_fonts.degraded) {
            std::cerr << "wl_ui: no system font found; falling back to ImGui's built-in font "
                          "(Cyrillic exercise names will not render)\n";
        }
        // Usually redundant with lunasvg's own hard-coded DejaVu fallback paths on
        // Linux (ui/svg_texture.hpp), but it's what makes the template's FRONT/BACK
        // labels render on a distro, or on Windows, where those paths don't exist.
        if (resolved_fonts.body_path.has_value()) register_fallback_font(*resolved_fonts.body_path);

        AppModel model;
        root_view::State state;

        while (platform.pump_events()) {
            if (platform.consume_render_device_reset()) {
                state.editor.day_map.invalidate_texture();
                state.cycle.cycle_map.invalidate_texture();
                for (auto& [index, block] : state.editor.blocks) block.exercise_map.invalidate_texture();
            }

            platform.begin_frame();
            ImGui::PushFont(resolved_fonts.body);
            root_view::draw(platform, model, state, resolved_fonts.mono);
            ImGui::PopFont();
            platform.end_frame(24, 24, 27);
        }
    } catch (const std::exception& e) {
        std::cerr << "wl_ui: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
