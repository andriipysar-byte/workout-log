#include "fonts.hpp"

#include <array>
#include <cstdlib>
#include <span>

namespace workoutlog::ui::fonts {

namespace {

std::optional<std::filesystem::path> from_env(const char* var) {
    const char* v = std::getenv(var);
    if (v == nullptr || *v == '\0') return std::nullopt;
    std::error_code ec;
    if (!std::filesystem::exists(v, ec) || ec) return std::nullopt;
    return std::filesystem::path(v);
}

std::optional<std::filesystem::path> first_existing(std::span<const char* const> candidates) {
    for (const char* c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c, ec) && !ec) return std::filesystem::path(c);
    }
    return std::nullopt;
}

// Debian/Ubuntu, Fedora, Arch, then Windows -- this is a Linux/Windows-only
// target, so unlike core/ (which must also compile for the macOS app) there's no
// need to keep these lists platform-conditional at compile time.
constexpr std::array<const char*, 8> kBodyCandidates = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "C:/Windows/Fonts/segoeui.ttf",
    "C:/Windows/Fonts/arial.ttf",
};

constexpr std::array<const char*, 8> kMonoCandidates = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation-mono/LiberationMono-Regular.ttf",
    "/usr/share/fonts/liberation-mono/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
    "C:/Windows/Fonts/consola.ttf",
    "C:/Windows/Fonts/cour.ttf",
};

} // namespace

Resolved load(ImFontAtlas& atlas, float body_size_px, float mono_size_px) {
    Resolved result;

    std::optional<std::filesystem::path> body_path = from_env("WORKOUTLOG_UI_FONT");
    if (!body_path.has_value()) body_path = first_existing(kBodyCandidates);

    std::optional<std::filesystem::path> mono_path = from_env("WORKOUTLOG_MONO_FONT");
    if (!mono_path.has_value()) mono_path = first_existing(kMonoCandidates);

    if (body_path.has_value()) {
        ImFontConfig body_cfg{};
        result.body =
            atlas.AddFontFromFileTTF(body_path->string().c_str(), body_size_px, &body_cfg, kGlyphRanges.data());
        result.body_path = body_path;
    }

    if (mono_path.has_value()) {
        ImFontConfig mono_cfg{};
        result.mono =
            atlas.AddFontFromFileTTF(mono_path->string().c_str(), mono_size_px, &mono_cfg, kGlyphRanges.data());

        // Merge the body face's glyphs into this same ImFont as a fallback layer,
        // in case the resolved mono candidate is a different family than the body
        // one and is missing something in kGlyphRanges (the DejaVu pair this
        // resolves to on this box already covers it fully, so this only matters
        // when the two candidate lists resolve to unrelated fonts).
        if (result.mono != nullptr && body_path.has_value() && *body_path != *mono_path) {
            ImFontConfig merge_cfg{};
            merge_cfg.MergeMode = true;
            atlas.AddFontFromFileTTF(body_path->string().c_str(), mono_size_px, &merge_cfg, kGlyphRanges.data());
        }
    }

    if (result.body == nullptr) {
        result.body = atlas.AddFontDefault();
        result.degraded = true;
    }
    if (result.mono == nullptr) {
        result.mono = result.body; // still usable, just not visually monospaced
        result.degraded = true;
    }

    return result;
}

} // namespace workoutlog::ui::fonts
