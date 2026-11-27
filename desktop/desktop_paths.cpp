#include <SDL2/SDL.h>
#include <filesystem>
#include <string>

std::string desktop_app_path(const char *relative) {
    char *base = SDL_GetBasePath();
    if (!base) return (std::filesystem::current_path() / relative).string();
    std::filesystem::path result = std::filesystem::path(base) / relative;
    SDL_free(base);
    return result.string();
}

// Shared gameplay diagnostics are optional outside the PS5 port.
void ps5_startup_log(const char *, const char *) {}
