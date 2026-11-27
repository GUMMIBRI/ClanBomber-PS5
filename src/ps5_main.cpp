/*
 * PS5 homebrew integration for ClanBomber2.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <SDL2/SDL.h>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <exception>
#include <sys/stat.h>
#include <filesystem>
#include <unistd.h>

#include "ClanBomber.h"
#include "GameConfig.h"

extern ClanBomberApplication *app;
extern "C" int sceSystemServiceLoadExec(const char *, const char **);
extern "C" int sceKernelUsleep(unsigned int);

namespace {
constexpr char log_root[] = "/mnt/usb0/homebrew/PPSA77028";
constexpr char diagnostic_build[] = "01.000.032";
constexpr unsigned slot_count = 6;
constexpr long log_limit = 16 * 1024;
char log_path[192]{};
long run_time;
unsigned step;

void slot_path(char *path, size_t size, unsigned slot) {
    std::snprintf(path, size, "%s/clanbomber-run-%u.log", log_root, slot);
}

void begin_startup_log() {
    run_time = static_cast<long>(std::time(nullptr));
    unsigned vacant = 0, oldest = 0;
    unsigned long oldest_sequence = 0, newest_sequence = 0;
    for (unsigned slot = 1; slot <= slot_count; ++slot) {
        char path[sizeof(log_path)];
        slot_path(path, sizeof(path), slot);
        struct stat info{};
        if (stat(path, &info) != 0) {
            if (errno == ENOENT && vacant == 0) vacant = slot;
            continue;
        }
        if (!S_ISREG(info.st_mode)) continue;
        auto *log = std::fopen(path, "r");
        if (!log) continue;
        unsigned long sequence = 0;
        const bool valid = std::fscanf(log, "ClanBomber diagnostic sequence=%lu", &sequence) == 1;
        std::fclose(log);
        if (!valid || sequence == 0) continue;
        if (sequence > newest_sequence) newest_sequence = sequence;
        if (oldest == 0 || sequence < oldest_sequence) {
            oldest = slot;
            oldest_sequence = sequence;
        }
    }
    const unsigned slot = vacant ? vacant : oldest;
    if (!slot) return;

    char path[sizeof(log_path)];
    slot_path(path, sizeof(path), slot);
    if (auto *log = std::fopen(path, "w")) {
        const int result = std::fprintf(log,
            "ClanBomber diagnostic sequence=%lu build=%s run=%ld-%d\n",
            newest_sequence + 1, diagnostic_build, run_time, static_cast<int>(getpid()));
        const int closed = std::fclose(log);
        if (result > 0 && closed == 0)
            std::snprintf(log_path, sizeof(log_path), "%s", path);
    }
}
}

void ps5_startup_log(const char *stage, const char *detail) {
    if (!log_path[0]) return;
    const int saved_errno = errno;
    if (auto *log = std::fopen(log_path, "a")) {
        if (std::fseek(log, 0, SEEK_END) == 0) {
            const long size = std::ftell(log);
            if (size >= 0 && size <= log_limit - 512)
                std::fprintf(log, "build=%s run=%ld-%d step=%03u %.64s: %.320s\n",
                             diagnostic_build, run_time, static_cast<int>(getpid()), ++step, stage, detail);
        }
        std::fclose(log);
    }
    errno = saved_errno;
}

extern "C" void catchReturnFromMain(int status) {
    char value[32];
    std::snprintf(value, sizeof(value), "%d", status);
    ps5_startup_log("main returned", value);
    std::fflush(nullptr);
    const int refused = sceSystemServiceLoadExec("exit", nullptr);
    std::snprintf(value, sizeof(value), "0x%08x", static_cast<unsigned>(refused));
    ps5_startup_log("platform exit returned", value);
    for (;;) sceKernelUsleep(100000);
}

namespace {

SDL_GameController *menu_pad = nullptr;
bool match_input = false;
std::filesystem::path app_root;

void open_menu_pad() {
    for (int i = 0; !menu_pad && i < SDL_NumJoysticks(); ++i) {
        menu_pad = SDL_GameControllerOpen(i);
    }
}

SDL_Scancode menu_key(Uint8 button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return SDL_SCANCODE_UP;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return SDL_SCANCODE_DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return SDL_SCANCODE_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return SDL_SCANCODE_RIGHT;
        case SDL_CONTROLLER_BUTTON_A: return SDL_SCANCODE_RETURN;
        case SDL_CONTROLLER_BUTTON_B: return SDL_SCANCODE_BACKSPACE;
        case SDL_CONTROLLER_BUTTON_X: return SDL_SCANCODE_SPACE;
        case SDL_CONTROLLER_BUTTON_Y: return SDL_SCANCODE_H;
        case SDL_CONTROLLER_BUTTON_START: return match_input ? SDL_SCANCODE_F3 : SDL_SCANCODE_ESCAPE;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

int watch_pad(void *, SDL_Event *event) {
    if (event->type == SDL_CONTROLLERDEVICEADDED && !menu_pad) {
        menu_pad = SDL_GameControllerOpen(event->cdevice.which);
    }
    if (!menu_pad) return 0;

    const auto owner = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(menu_pad));
    if (event->type == SDL_CONTROLLERDEVICEREMOVED && event->cdevice.which == owner) {
        SDL_GameControllerClose(menu_pad);
        menu_pad = nullptr;
        open_menu_pad();
    } else if ((event->type == SDL_CONTROLLERBUTTONDOWN ||
                event->type == SDL_CONTROLLERBUTTONUP) && event->cbutton.which == owner) {
        const auto scancode = menu_key(event->cbutton.button);
        if (scancode == SDL_SCANCODE_UNKNOWN) return 0;
        SDL_Event key{};
        key.type = event->type == SDL_CONTROLLERBUTTONDOWN ? SDL_KEYDOWN : SDL_KEYUP;
        key.key.state = key.type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
        key.key.keysym.scancode = scancode;
        key.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
        SDL_PushEvent(&key);
    }
    return 0;
}

bool select_app_dir(const std::filesystem::path &dir) {
    if (dir.empty()) return false;
    const auto font = dir / CB_DATADIR / "fonts/DejaVuSans-Bold.ttf";
    auto *probe = std::fopen(font.c_str(), "rb");
    if (!probe) {
        char failure[384];
        std::snprintf(failure, sizeof(failure), "fopen errno=%d %.300s", errno, font.c_str());
        ps5_startup_log("font unavailable", failure);
        return false;
    }
    std::fclose(probe);
    app_root = dir;
    ps5_startup_log("asset root", dir.c_str());
    return true;
}

} // namespace

std::string ps5_app_path(const char *relative) {
    return (app_root / relative).string();
}

SDL_JoystickID ps5_menu_pad_id() {
    return menu_pad ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(menu_pad)) : -1;
}

void ps5_set_match_input(bool active) {
    match_input = active;
}

void ps5_start_menu_input() {
    SDL_PumpEvents();
    SDL_ClearError();
    open_menu_pad();
    if (menu_pad) {
        const char *name = SDL_GameControllerName(menu_pad);
        ps5_startup_log("menu controller", name ? name : "opened");
    } else {
        const char *error = SDL_GetError();
        ps5_startup_log("menu controller", *error ? error : "no pad present; waiting for hotplug");
    }
    SDL_AddEventWatch(watch_pad, nullptr);
}

int main(int argc, char **argv) {
    begin_startup_log();
    ps5_startup_log("startup", "begin");
    if (!select_app_dir(argc > 0 && argv && argv[0] ? std::filesystem::path(argv[0]).parent_path() : "") &&
        !select_app_dir("/app0") && !select_app_dir(".") &&
        !select_app_dir("/mnt/usb0/homebrew/PPSA77028")) {
        std::fprintf(stderr, "ClanBomber2: app-local game data missing\n");
        return 1;
    }

#if defined(CLANBOMBER_PS5_VULKAN)
    Config::set_softwareRendering(false);
    Config::set_renderDriver("ps5vulkan");
#elif defined(CLANBOMBER_PS5_GPU)
    Config::set_softwareRendering(false);
    Config::set_renderDriver("opengles2");
#else
    Config::set_softwareRendering(true);
#endif
    app = new ClanBomberApplication();
    int result = 2;
    try {
        result = app->main();
    } catch (const std::exception &error) {
        ps5_startup_log("uncaught game exception", error.what());
    }
    SDL_DelEventWatch(watch_pad, nullptr);
    if (menu_pad && SDL_WasInit(SDL_INIT_GAMECONTROLLER)) SDL_GameControllerClose(menu_pad);
    if (result == 0) delete app;
    return result;
}
