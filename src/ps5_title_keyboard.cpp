/* Controller-only native title integration. SPDX-License-Identifier: GPL-2.0-or-later */
#include <SDL2/SDL.h>

struct SDL_VideoDevice;

// The payload SDL backend eagerly calls native keyboard and IME services
// that are unavailable to this title. Menu input still comes from gamepads.
extern "C" {
int __wrap_PS5_Keyboard_Init() { return 0; }
int __wrap_PS5_Keyboard_Open() { return 0; }
int __wrap_PS5_Keyboard_PumpEvents() { return 0; }
SDL_bool __wrap_PS5_HasScreenKeyboardSupport(SDL_VideoDevice *) { return SDL_FALSE; }
SDL_bool __wrap_PS5_IsScreenKeyboardShown(SDL_VideoDevice *, SDL_Window *) { return SDL_FALSE; }
void __wrap_PS5_ShowScreenKeyboard(SDL_VideoDevice *, SDL_Window *) {}
void __wrap_PS5_HideScreenKeyboard(SDL_VideoDevice *, SDL_Window *) {}
}
