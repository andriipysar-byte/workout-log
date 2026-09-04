#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

// No SDL3, ImGui or lunasvg headers here -- Platform is the pimpl boundary
// (AGENTS.md 1.1.1) that keeps every other ui/ header free of them.
struct SDL_Renderer;

namespace workoutlog::ui {

// Owns SDL's lifetime (video subsystem, window, renderer), the Dear ImGui context
// and both backends (SDL3 + SDLRenderer3), plus the app's one asynchronous
// operation: the native folder-picker dialog. One instance for the process
// lifetime; construct it before touching ImGui:: or SDL_ from anywhere else.
class Platform {
public:
    Platform();
    ~Platform();
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    Platform(Platform&&) = delete;
    Platform& operator=(Platform&&) = delete;

    // Pumps SDL events into ImGui. Returns false once the user has asked to quit
    // (window close, SDL_EVENT_QUIT) -- the caller's frame loop should stop.
    bool pump_events();

    void begin_frame();
    // Renders the frame ImGui built and presents it. clear_r/g/b is the window
    // background behind every ImGui draw, 0-255 per channel (matches
    // SDL_SetRenderDrawColor's Uint8 range so this header needn't include SDL's).
    void end_frame(unsigned char clear_r, unsigned char clear_g, unsigned char clear_b);

    // io.DisplayFramebufferScale.x -- the font/style scale factor for HiDPI.
    float display_scale() const;
    SDL_Renderer* renderer() const;

    // True at most once per occurrence: SDL_EVENT_RENDER_DEVICE_RESET or
    // _TARGETS_RESET happened since the last call, which invalidates every
    // SDL_Texture the app holds (real on the D3D11 backend on Windows). Callers
    // that cache textures (ui::SvgTexture's MapCache) must check this every frame.
    bool consume_render_device_reset();

    // Shows the native folder picker (SDL_ShowOpenFolderDialog). Asynchronous and
    // fire-and-forget: the result (or a cancellation, or an error) shows up later
    // through take_folder_result()/take_folder_error(). A call while one is
    // already open is ignored. Not every platform can show this dialog (no XDG
    // portal or zenity on Linux) -- callers must still offer an in-app fallback.
    void request_folder_dialog(const std::filesystem::path& start_dir);
    std::optional<std::filesystem::path> take_folder_result();
    std::optional<std::string> take_folder_error();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace workoutlog::ui
