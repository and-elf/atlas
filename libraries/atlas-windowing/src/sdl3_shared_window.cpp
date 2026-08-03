#include "atlas/windowing/sdl3_shared_window.hpp"

#include <stdexcept>
#include <utility>

namespace atlas::windowing {

Sdl3SharedWindow::Sdl3SharedWindow(const std::string& title,
                                   int width,
                                   int height,
                                   SDL_WindowFlags extra_window_flags) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init(SDL_INIT_VIDEO) failed: ") + SDL_GetError());
    }

    window_ = SDL_CreateWindow(title.c_str(), width, height, extra_window_flags);
    if (window_ == nullptr) {
        const std::string error = SDL_GetError();
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed: " + error);
    }

    owns_sdl_ = true;
}

Sdl3SharedWindow::~Sdl3SharedWindow() {
    destroy();
}

Sdl3SharedWindow::Sdl3SharedWindow(Sdl3SharedWindow&& other) noexcept
    : window_(std::exchange(other.window_, nullptr)), owns_sdl_(std::exchange(other.owns_sdl_, false)) {}

Sdl3SharedWindow& Sdl3SharedWindow::operator=(Sdl3SharedWindow&& other) noexcept {
    if (this != &other) {
        destroy();
        window_ = std::exchange(other.window_, nullptr);
        owns_sdl_ = std::exchange(other.owns_sdl_, false);
    }
    return *this;
}

void Sdl3SharedWindow::destroy() noexcept {
    if (!owns_sdl_) {
        return;
    }

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
    owns_sdl_ = false;
}

} // namespace atlas::windowing
