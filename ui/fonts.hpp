#pragma once

#include <imgui.h>

#include <array>
#include <filesystem>
#include <optional>

// Font resolution and glyph coverage for the UI. The exercise catalogue is
// Ukrainian and ImGui's built-in font is ASCII-only, so this has to be handled
// before any screen renders a single exercise name.
namespace workoutlog::ui::fonts {

// GetGlyphRangesCyrillic() (Basic Latin + Latin-1 Supplement + Cyrillic + its two
// Unicode extension blocks) plus every non-ASCII code point this app actually
// displays that range doesn't already cover -- enumerated from data/*.json,
// exercises.json, cycles.json and the UI chrome literals (en/em dash, bullet,
// right arrow, <=). A namespace-scope array, not a local ImFontGlyphRangesBuilder
// result: ImGui only dereferences the ranges pointer at atlas-build time, so a
// stack-local vector would already be gone by then.
inline constexpr std::array<ImWchar, 15> kGlyphRanges = {
    0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement (has U+00D7 x, U+00B7 middot)
    0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
    0x2DE0, 0x2DFF, // Cyrillic Extended-A
    0xA640, 0xA69F, // Cyrillic Extended-B
    0x2010, 0x2027, // General Punctuation: hyphen .. en/em dash .. bullet .. ellipsis
    0x2190, 0x2193, // arrows (the notation preview's "->" is U+2192)
    0x2264, 0x2265, // <= >=
    0,
};
// AddFontFromFileTTF wants a raw `const ImWchar*`, zero-terminated -- use
// kGlyphRanges.data() at the call site rather than letting the array decay
// implicitly (cppcoreguidelines-pro-bounds-array-to-pointer-decay).

struct Resolved {
    ImFont* body = nullptr;
    ImFont* mono = nullptr;
    // No real font file could be found anywhere on the candidate list or via the
    // env override -- ImGui's built-in ASCII-only font is standing in, so Cyrillic
    // exercise names will render as boxes. The caller should surface this (the
    // status bar) rather than let it fail silently.
    bool degraded = false;
    // The resolved proportional font's file, when there is one -- also handed to
    // lunasvg (lunasvg_add_font_face_from_file) so the muscle-map template's
    // FRONT/BACK labels have a face to render with.
    std::optional<std::filesystem::path> body_path;
};

// $WORKOUTLOG_UI_FONT / $WORKOUTLOG_MONO_FONT override; otherwise probes a short
// list of well-known Linux (Debian/Fedora/Arch: DejaVu, Liberation, Noto) and
// Windows (Segoe UI/Consolas, falling back to Arial/Courier) paths with
// std::filesystem::exists before ever calling AddFontFromFileTTF -- that call
// asserts (and, at this repo's default Debug build, aborts the process) on a
// missing file, so a hard-coded path that resolves here would kill the app on a
// distro that lays fonts out differently. Falls back to AddFontDefault() with
// Resolved::degraded = true rather than not starting at all.
Resolved load(ImFontAtlas& atlas, float body_size_px, float mono_size_px);

} // namespace workoutlog::ui::fonts
