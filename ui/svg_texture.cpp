#include "svg_texture.hpp"

#include <SDL3/SDL.h>
#include <lunasvg.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <utility>

namespace workoutlog::ui {

namespace {

// ImTextureID has been ImU64 (not void*) since ImGui v1.91.4; the SDL_Renderer
// backend stores an SDL_Texture* in it. std::bit_cast keeps this conversion out
// of reinterpret_cast, which AGENTS.md 2.6 bans outright ("no reinterpret_cast in
// this tree") -- this is the one named helper doing it.
ImTextureID as_texture_id(SDL_Texture* texture) {
    static_assert(sizeof(SDL_Texture*) <= sizeof(ImTextureID));
    return static_cast<ImTextureID>(std::bit_cast<std::uintptr_t>(texture));
}

struct SdlTextureDeleter {
    void operator()(SDL_Texture* t) const {
        if (t != nullptr) SDL_DestroyTexture(t);
    }
};

// Fits the template's intrinsic aspect ratio inside a box_w x box_h box (matching
// the SwiftUI original's `svg{width:100%;height:100%}` inside a fixed-height
// frame, Views.swift's DayMuscleMap/CycleMuscleMap) -- the raster is sized to the
// content, not stretched to fill the box.
std::pair<int, int> aspect_fit(double content_w, double content_h, int box_w, int box_h) {
    if (content_w <= 0.0 || content_h <= 0.0 || box_w <= 0 || box_h <= 0) return {0, 0};
    const double scale = std::min(static_cast<double>(box_w) / content_w, static_cast<double>(box_h) / content_h);
    const int w = std::max(1, static_cast<int>(content_w * scale));
    const int h = std::max(1, static_cast<int>(content_h * scale));
    return {w, h};
}

} // namespace

bool register_fallback_font(const std::filesystem::path& font_path) {
    return lunasvg_add_font_face_from_file("", false, false, font_path.string().c_str());
}

struct SvgTexture::Impl {
    std::string last_svg;
    std::unique_ptr<lunasvg::Document> document;
    int last_box_w = 0;
    int last_box_h = 0;
    int texture_w = 0;
    int texture_h = 0;
    std::unique_ptr<SDL_Texture, SdlTextureDeleter> texture;

    void clear_texture() {
        texture.reset();
        texture_w = 0;
        texture_h = 0;
    }
};

SvgTexture::SvgTexture() : impl_(std::make_unique<Impl>()) {}
SvgTexture::~SvgTexture() = default;
SvgTexture::SvgTexture(SvgTexture&&) noexcept = default;
SvgTexture& SvgTexture::operator=(SvgTexture&&) noexcept = default;

void SvgTexture::update(SDL_Renderer& renderer, const std::string& svg, int box_width_px, int box_height_px) {
    if (box_width_px <= 0 || box_height_px <= 0) return;

    // No unbounded allocation sized from a window resize (AGENTS.md 1.2.2).
    constexpr int kMaxDimension = 2048;
    box_width_px = std::min(box_width_px, kMaxDimension);
    box_height_px = std::min(box_height_px, kMaxDimension);

    const bool content_changed = svg != impl_->last_svg;
    const bool size_changed = box_width_px != impl_->last_box_w || box_height_px != impl_->last_box_h;
    impl_->last_box_w = box_width_px;
    impl_->last_box_h = box_height_px;
    if (!content_changed && !size_changed) return;

    if (content_changed) {
        impl_->document = lunasvg::Document::loadFromData(svg);
        impl_->last_svg = svg;
    }
    if (!impl_->document) {
        impl_->clear_texture();
        return;
    }

    const auto [w, h] = aspect_fit(impl_->document->width(), impl_->document->height(), box_width_px, box_height_px);
    if (w <= 0 || h <= 0) {
        impl_->clear_texture();
        return;
    }

    lunasvg::Bitmap bitmap = impl_->document->renderToBitmap(w, h, 0x00000000);
    if (bitmap.isNull()) {
        impl_->clear_texture();
        return;
    }
    bitmap.convertToRGBA(); // ARGB32 premultiplied -> plain RGBA, matching SDL_PIXELFORMAT_RGBA32 below

    impl_->texture.reset(SDL_CreateTexture(&renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h));
    if (!impl_->texture) {
        impl_->texture_w = 0;
        impl_->texture_h = 0;
        return;
    }
    SDL_SetTextureBlendMode(impl_->texture.get(), SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(impl_->texture.get(), nullptr, bitmap.data(), bitmap.stride());
    impl_->texture_w = w;
    impl_->texture_h = h;
}

ImTextureID SvgTexture::id() const {
    return impl_->texture ? as_texture_id(impl_->texture.get()) : ImTextureID{0};
}

ImVec2 SvgTexture::size() const {
    return {static_cast<float>(impl_->texture_w), static_cast<float>(impl_->texture_h)};
}

void SvgTexture::invalidate_texture() {
    impl_->clear_texture();
    // Forces the size-changed branch on the next update() even if the SVG string
    // is unchanged, so the (already-parsed) document is re-rasterized without
    // re-parsing it.
    impl_->last_box_w = -1;
    impl_->last_box_h = -1;
}

} // namespace workoutlog::ui
