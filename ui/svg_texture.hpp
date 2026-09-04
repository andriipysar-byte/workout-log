#pragma once

#include <imgui.h>

#include <filesystem>
#include <memory>
#include <string>

// No SDL3 or lunasvg headers here -- Impl is the pimpl boundary (AGENTS.md 1.1.1)
// that keeps both out of every other ui/ header.
struct SDL_Renderer;

namespace workoutlog::ui {

// Registers a font file with lunasvg's fallback (empty-family) slot, so the
// muscle-map template's <text>FRONT</text>/<text>BACK</text> labels have a face
// to render with. lunasvg v3.5.0 already auto-registers a couple of hard-coded
// DejaVu paths on Linux, so this is usually redundant there -- but it's what
// makes the labels render on a distro (or on Windows) where the resolved UI font
// (ui::fonts::load) isn't at one of those exact paths. Safe to call more than
// once; returns false if the file couldn't be read as a font.
bool register_fallback_font(const std::filesystem::path& font_path);

// Rasterizes one colorized muscle-map SVG string into an SDL_Texture and caches
// it: re-parsing the ~90KB template and re-rendering 167 paths is far too
// expensive to redo every frame. Two independent things can make the cache stale
// -- the SVG *content* (a session edit, a mode switch, a different session) and
// the *target size* (a window resize) -- and only the second one needs the
// (cheap) rasterize step redone if the parsed document is still valid; the first
// needs both redone. update() is called with the actual current SVG string every
// frame; whether anything expensive happens is decided by direct string
// comparison against what was last rasterized, so there's no separate revision
// counter for a caller to keep in sync (and no way for it to drift out of sync).
class SvgTexture {
public:
    SvgTexture();
    ~SvgTexture();
    SvgTexture(const SvgTexture&) = delete;
    SvgTexture& operator=(const SvgTexture&) = delete;
    SvgTexture(SvgTexture&&) noexcept;
    SvgTexture& operator=(SvgTexture&&) noexcept;

    // `svg` is the already-colorized template (muscle_map_svg::colorize's output).
    // `box_width_px`/`box_height_px` is the available framebuffer pixel box; the
    // template's own aspect ratio is preserved within it, matching the SwiftUI
    // original's `svg{width:100%;height:100%}` inside a fixed-height frame.
    // Degrades to no texture (id() returns 0) on a parse failure, a render
    // failure, or a nonsensical box size -- never throws; this runs from the
    // render loop.
    void update(SDL_Renderer& renderer, const std::string& svg, int box_width_px, int box_height_px);

    // 0 until the first successful update() (or after invalidate_texture()).
    // Safe to hand straight to ImGui::Image even when it's 0 -- ImGui draws an
    // empty rect for a null texture ID.
    ImTextureID id() const;
    // The rasterized pixel size (post aspect-fit), for ImGui::Image's image_size.
    ImVec2 size() const;

    // Drops the texture (the cached parsed document and SVG string survive) so
    // the next update() call rebuilds it from what's already parsed -- for
    // SDL_EVENT_RENDER_DEVICE_RESET / _TARGETS_RESET, which invalidate every
    // SDL_Texture in the process (real on the D3D11 backend on Windows).
    void invalidate_texture();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace workoutlog::ui
