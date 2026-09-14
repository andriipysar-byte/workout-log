// Headless regression harness for the ImGui screens (issue #21): renders a fixed
// set of scenarios offscreen (SDL_VIDEODRIVER=offscreen, forced below regardless of
// the caller's environment) and diffs the result against a checked-in PPM golden
// per scenario. wl_verify (core/) has nothing to say about ui/ -- this is the first
// tool that does.
//
// Determinism: the harness never calls fonts::load() (which probes system font
// files -- different machines/distros would rasterize differently). It uses
// ImGui's compiled-in default font instead, so every glyph comes from data baked
// into the pinned ImGui version, not the filesystem. That means Cyrillic exercise
// names render as ImGui's fallback glyph here, not real Cyrillic text -- a known,
// accepted tradeoff for pixel-reproducible snapshots; it is not what this harness
// is for. Rendering goes through SDL's own software path for the "offscreen"
// driver, not a GPU, so there is no driver/vendor variance either. If a snapshot
// ever proves flaky across machines despite this, that is real information (an
// actual nondeterminism bug), not a reason to loosen the comparison first.
//
// Usage: wl_ui_snapshot [--update-golden]
//   --update-golden   (Re)write every scenario's golden PPM instead of comparing
//                      against it. Review the resulting image diff before
//                      committing ui/tests/golden/*.ppm.

#include "app_model.hpp"
#include "platform.hpp"
#include "root_view.hpp"
#include "theme.hpp"
#include "workoutlog/paths.hpp"

#include <imgui.h>

// Direct SDL use is otherwise confined to platform.cpp (the pimpl boundary,
// AGENTS.md 1.1.1) -- this harness is the one deliberate exception, since reading
// back rendered pixels for comparison is inherently an SDL-level concern no
// screen's own code needs.
#include <SDL3/SDL.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace workoutlog::ui;

namespace {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<char> rgb; // width * height * 3, row-major, no padding
};

Image capture(SDL_Renderer& renderer) {
    SDL_Surface* raw = SDL_RenderReadPixels(&renderer, nullptr);
    if (raw == nullptr) throw std::runtime_error(std::string("SDL_RenderReadPixels failed: ") + SDL_GetError());
    SDL_Surface* converted = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGB24);
    SDL_DestroySurface(raw);
    if (converted == nullptr) throw std::runtime_error(std::string("SDL_ConvertSurface failed: ") + SDL_GetError());

    Image img;
    img.width = converted->w;
    img.height = converted->h;
    img.rgb.resize(static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height) * 3);
    const auto* pixels = static_cast<const char*>(converted->pixels);
    for (int y = 0; y < img.height; y++) {
        const char* row = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(converted->pitch);
        std::copy(row, row + static_cast<std::size_t>(img.width) * 3,
                  img.rgb.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) *
                                                                  static_cast<std::size_t>(img.width) * 3));
    }
    SDL_DestroySurface(converted);
    return img;
}

void write_ppm(const std::filesystem::path& path, const Image& img) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot open " + path.string() + " for writing");
    out << "P6\n" << img.width << " " << img.height << "\n255\n";
    out.write(img.rgb.data(), static_cast<std::streamsize>(img.rgb.size()));
}

// Untrusted-boundary input like any other file this codebase reads (AGENTS.md
// 1.2.1) even though today only this tool ever writes one.
Image read_ppm(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::string magic;
    int width = 0;
    int height = 0;
    int maxval = 0;
    in >> magic >> width >> height >> maxval;
    if (!in || magic != "P6" || width <= 0 || height <= 0 || maxval != 255)
        throw std::runtime_error("malformed PPM header in " + path.string());
    in.get(); // the single whitespace byte the P6 header ends with

    Image img;
    img.width = width;
    img.height = height;
    img.rgb.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
    in.read(img.rgb.data(), static_cast<std::streamsize>(img.rgb.size()));
    if (!in) throw std::runtime_error("truncated PPM data in " + path.string());
    return img;
}

struct Scenario {
    std::string name;
    std::function<void(AppModel&, root_view::State&)> setup;
};

// Scenarios share one AppModel across the run (one repo-root resolution, one
// folder listing) rather than each getting its own -- so a later scenario can see
// state an earlier one left behind (list_with_session opens a file; cycle_tab
// doesn't depend on that, but does run after it). That is deliberate, not
// incidental: the fixed run order is as much a part of the golden as the screens
// themselves, so don't reorder this list without regenerating them.
std::vector<Scenario> scenarios() {
    return {
        {"list_empty", [](AppModel&, root_view::State& state) { state.tab = root_view::Tab::list; }},
        {"list_with_session",
         [](AppModel& model, root_view::State& state) {
             state.tab = root_view::Tab::list;
             if (!model.files().empty()) model.open(model.files().front());
         }},
        {"cycle_tab", [](AppModel&, root_view::State& state) { state.tab = root_view::Tab::cycle; }},
    };
}

} // namespace

int main(int argc, char** argv) {
    bool update_golden = false;
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--update-golden") update_golden = true;

    std::filesystem::path repo_root;
    try {
        repo_root = workoutlog::paths::resolve_repo_root();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    const auto golden_dir = repo_root / "ui" / "tests" / "golden";
    std::error_code ec;
    std::filesystem::create_directories(golden_dir, ec);

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

    int failures = 0;
    try {
        Platform platform;
        float scale = platform.display_scale();
        if (scale <= 0.0f) scale = 1.0f; // SDL_GetWindowDisplayScale can return 0 before the first frame

        ImFont* font = ImGui::GetIO().Fonts->AddFontDefault();
        theme::apply(ImGui::GetStyle(), scale);

        AppModel model;

        for (const auto& scenario : scenarios()) {
            root_view::State state;
            scenario.setup(model, state);

            platform.begin_frame();
            ImGui::PushFont(font);
            root_view::draw(platform, model, state, font);
            ImGui::PopFont();
            platform.end_frame(24, 24, 27);

            const Image captured = capture(*platform.renderer());
            const auto golden_path = golden_dir / (scenario.name + ".ppm");

            if (update_golden || !std::filesystem::exists(golden_path)) {
                write_ppm(golden_path, captured);
                std::cout << "  wrote golden: " << golden_path.string() << "\n";
                continue;
            }

            const Image golden = read_ppm(golden_path);
            if (golden.width != captured.width || golden.height != captured.height) {
                std::cout << "  FAIL " << scenario.name << ": size mismatch (golden " << golden.width << "x"
                          << golden.height << ", got " << captured.width << "x" << captured.height << ")\n";
                failures++;
            } else if (golden.rgb != captured.rgb) {
                std::size_t diff_bytes = 0;
                for (std::size_t i = 0; i < golden.rgb.size(); i++)
                    if (golden.rgb[i] != captured.rgb[i]) diff_bytes++;
                std::cout << "  FAIL " << scenario.name << ": " << diff_bytes << "/" << golden.rgb.size()
                          << " byte(s) differ from " << golden_path.string()
                          << " (rerun with --update-golden if this is an intended change)\n";
                failures++;
            } else {
                std::cout << "  ok   " << scenario.name << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "wl_ui_snapshot: " << e.what() << "\n";
        return 1;
    }

    if (update_golden) {
        std::cout << "\nGolden images updated -- review the diff and commit ui/tests/golden/*.ppm.\n";
        return 0;
    }
    std::cout << (failures == 0 ? "\n\xE2\x9C\x85 ALL SNAPSHOTS MATCH"
                                : "\n\xE2\x9D\x8C " + std::to_string(failures) + " SNAPSHOT(S) DIFFER")
              << "\n";
    return failures == 0 ? 0 : 1;
}
