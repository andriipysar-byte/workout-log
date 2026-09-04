#include "platform.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <mutex>
#include <stdexcept>
#include <utility>

namespace workoutlog::ui {

namespace {

struct SdlWindowDeleter {
    void operator()(SDL_Window* w) const {
        if (w) SDL_DestroyWindow(w);
    }
};
struct SdlRendererDeleter {
    void operator()(SDL_Renderer* r) const {
        if (r) SDL_DestroyRenderer(r);
    }
};

} // namespace

struct Platform::Impl {
    std::unique_ptr<SDL_Window, SdlWindowDeleter> window;
    std::unique_ptr<SDL_Renderer, SdlRendererDeleter> renderer;
    bool imgui_context_created = false;
    bool sdl3_backend_initialized = false;
    bool renderer_backend_initialized = false;
    bool running = true;
    bool render_device_reset_pending = false;

    // SDL_ShowOpenFolderDialog's callback "may be called from a different thread
    // than the one the function was invoked on" (SDL_dialog.h) -- it must not touch
    // ImGui or AppModel directly, so it only ever writes here, guarded by a mutex,
    // and main.cpp drains it at the top of the frame (AGENTS.md 1.2.5).
    std::mutex folder_mutex;
    bool folder_dialog_open = false;
    std::optional<std::filesystem::path> folder_result;
    std::optional<std::string> folder_error;

    // A member (not a free function) so it can name Platform::Impl -- a private
    // nested type outside code in this same file still can't reach, only
    // Platform's own members and this struct's own members can. Static, and with
    // SDLCALL's calling convention, so its address converts to a plain
    // SDL_DialogFileCallback for SDL_ShowOpenFolderDialog.
    static void SDLCALL folder_dialog_callback(void* userdata, const char* const* filelist, int filter);
};

void SDLCALL Platform::Impl::folder_dialog_callback(void* userdata, const char* const* filelist, int /*filter*/) {
    auto* impl = static_cast<Impl*>(userdata);
    std::lock_guard<std::mutex> lock(impl->folder_mutex);
    impl->folder_dialog_open = false;
    if (filelist == nullptr) {
        impl->folder_error = SDL_GetError();
        return;
    }
    if (*filelist == nullptr) return; // cancelled: no result, no error
    impl->folder_result = std::filesystem::path(*filelist);
}

Platform::Platform() : impl_(std::make_unique<Impl>()) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    impl_->window.reset(SDL_CreateWindow("WorkoutLog", 820, 560,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
    if (!impl_->window) {
        std::string err = SDL_GetError();
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed: " + err);
    }
    SDL_SetWindowMinimumSize(impl_->window.get(), 820, 560);

    impl_->renderer.reset(SDL_CreateRenderer(impl_->window.get(), nullptr));
    if (!impl_->renderer) {
        std::string err = SDL_GetError();
        impl_->window.reset();
        SDL_Quit();
        throw std::runtime_error("SDL_CreateRenderer failed: " + err);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    impl_->imgui_context_created = true;
    // "imgui.ini" defaults to relative-to-CWD (imgui.cpp); this app has no per-user
    // layout worth persisting yet, and a stray file dropped wherever wl_ui happens
    // to be launched would be a magic path (AGENTS.md 2.5).
    ImGui::GetIO().IniFilename = nullptr;

    if (!ImGui_ImplSDL3_InitForSDLRenderer(impl_->window.get(), impl_->renderer.get())) {
        throw std::runtime_error("ImGui_ImplSDL3_InitForSDLRenderer failed");
    }
    impl_->sdl3_backend_initialized = true;

    if (!ImGui_ImplSDLRenderer3_Init(impl_->renderer.get())) {
        throw std::runtime_error("ImGui_ImplSDLRenderer3_Init failed");
    }
    impl_->renderer_backend_initialized = true;
}

Platform::~Platform() {
    // Reverse of construction: renderer backend, then SDL3 backend, then the ImGui
    // context, then the renderer, then the window -- each explicit so the order
    // doesn't quietly depend on Impl's member declaration order.
    if (impl_->renderer_backend_initialized) ImGui_ImplSDLRenderer3_Shutdown();
    if (impl_->sdl3_backend_initialized) ImGui_ImplSDL3_Shutdown();
    if (impl_->imgui_context_created) ImGui::DestroyContext();
    impl_->renderer.reset();
    impl_->window.reset();
    SDL_Quit();
}

bool Platform::pump_events() {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) impl_->running = false;
        if (event.type == SDL_EVENT_RENDER_DEVICE_RESET || event.type == SDL_EVENT_RENDER_TARGETS_RESET)
            impl_->render_device_reset_pending = true;
    }
    return impl_->running;
}

void Platform::begin_frame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Platform::end_frame(unsigned char clear_r, unsigned char clear_g, unsigned char clear_b) {
    ImGui::Render();
    SDL_SetRenderDrawColor(impl_->renderer.get(), clear_r, clear_g, clear_b, 255);
    SDL_RenderClear(impl_->renderer.get());
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), impl_->renderer.get());
    SDL_RenderPresent(impl_->renderer.get());
}

float Platform::display_scale() const { return SDL_GetWindowDisplayScale(impl_->window.get()); }

SDL_Renderer* Platform::renderer() const { return impl_->renderer.get(); }

bool Platform::consume_render_device_reset() {
    bool was_pending = impl_->render_device_reset_pending;
    impl_->render_device_reset_pending = false;
    return was_pending;
}

void Platform::request_folder_dialog(const std::filesystem::path& start_dir) {
    std::lock_guard<std::mutex> lock(impl_->folder_mutex);
    if (impl_->folder_dialog_open) return;
    impl_->folder_dialog_open = true;
    impl_->folder_error.reset();
    SDL_ShowOpenFolderDialog(&Impl::folder_dialog_callback, impl_.get(), impl_->window.get(),
                              start_dir.string().c_str(), false);
}

std::optional<std::filesystem::path> Platform::take_folder_result() {
    std::lock_guard<std::mutex> lock(impl_->folder_mutex);
    return std::exchange(impl_->folder_result, std::nullopt);
}

std::optional<std::string> Platform::take_folder_error() {
    std::lock_guard<std::mutex> lock(impl_->folder_mutex);
    return std::exchange(impl_->folder_error, std::nullopt);
}

} // namespace workoutlog::ui
