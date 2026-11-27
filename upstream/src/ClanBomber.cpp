/*
 * This file is part of ClanBomber;
 * you can get it at "http://www.nongnu.org/clanbomber".
 *
 * Copyright (C) 1999-2004, 2007 Andreas Hundt, Denis Oliver Kropp
 * Copyright (C) 2009, 2010 Rene Lopez <rsl@members.fsf.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "AssetPath.h"
#include "SkinSelection.h"
#include "ClanBomber.h"


#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <ctime>

#include <filesystem>
#include <SDL2/SDL.h>
#ifdef CLANBOMBER_DESKTOP
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#endif
#if defined(CLANBOMBER_PS5_VULKAN)
#include <SDL2/SDL_vulkan.h>
#endif

#include "config.h"
#include "Controller.h"
#include "Observer.h"
#include "GameConfig.h"
#include "Menu.h"
#include "Timer.h"
#include "PlayerSetup.h"
#include "Map.h"
#include "Credits.h"
#include "MapEditor.h"
#include "MapTile.h"
#include "Bomber.h"
#include "MapSelector.h"
#ifdef CLANBOMBER_PS5
#include "Controller_Joystick.h"
#endif
#include "Bomb.h"
#include "Utils.h"
#include "UtilsGetHome.h"
#include "AudioSimple.h"
#include "AudioSimpleSDL.h"

#include <unistd.h>

ClanBomberApplication *app;

SDL_Renderer *renderer = NULL;
SDL_Window *gameWindow = NULL;
const Uint8 *keyboard = NULL;

std::filesystem::path ClanBomberApplication::map_path;
std::filesystem::path ClanBomberApplication::local_map_path;

static unsigned short next_object_id = 0;

/* initialize state to random bits */
static unsigned long state[16];
/* init should also reset this to 0 */
static unsigned int indx = 0;

/* return 32 bit random number */
unsigned long fast_random(void) {
	unsigned long a, b, c, d;
	a = state[indx];
	c = state[(indx + 13) & 15];
	b = a ^ c ^ (a << 16) ^ (c << 15);
	c = state[(indx + 9) & 15];
	c ^= (c >> 11);
	a = state[indx] = b ^ c;
	d = a ^ ((a << 5) & 0xDA442D24UL);
	indx = (indx + 15) & 15;
	a = state[indx];
	state[indx] = a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28);
	return state[indx];
}

bool game_object_compare(GameObject * go1, GameObject * go2) {
	return go1->get_z() < go2->get_z();
}

ClanBomberApplication::ClanBomberApplication() {
#ifdef CLANBOMBER_DESKTOP
	AS = nullptr;
#endif
	observer = NULL;
	map = NULL;
	pause_game = false;

	// initialize fast_random
#ifdef CLANBOMBER_DESKTOP
	std::srand(static_cast<unsigned>(std::time(nullptr)));
#endif
	for (int i = 0; i < 16; i++)
		state[i] = rand();
}

ClanBomberApplication::~ClanBomberApplication() {
	std::cout << _("(+) clanbomber: deallocating resources...") << std::endl;

#ifdef __SWITCH__
	// On Switch, calling Mix_CloseAudio() / TTF_Quit() / SDL_Quit() at this
	// point fatally crashes the OS — most likely an audren/HID teardown
	// ordering issue in libnx's SDL2 port. The kernel reclaims everything
	// cleanly when the process exits, so we deliberately skip the explicit
	// cleanup here.
	return;
#endif

#ifdef CLANBOMBER_DESKTOP
	if (AS) AS->close();
#else
	AS->close();
#endif
	TTF_Quit();
	SDL_Quit();

	if (map) {
		delete map;
		map = NULL;
	}

}

#ifdef CLANBOMBER_PS5
void ps5_startup_log(const char *stage, const char *detail);
void ps5_start_menu_input();
void ps5_set_match_input(bool active);
#endif

int ClanBomberApplication::init_SDL() {
	Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK;
#ifdef CLANBOMBER_PS5
    flags |= SDL_INIT_GAMECONTROLLER;
    ps5_startup_log("SDL_Init", "begin video/audio/controller");
#endif
    if (SDL_Init(flags)) {
		std::cerr << _("Cannot Initialise SDL!") << std::endl;
#ifdef CLANBOMBER_PS5
        ps5_startup_log("SDL video/audio init", SDL_GetError());
#endif
		SDL_Quit();
		return -1;
	}

#ifdef CLANBOMBER_PS5
    ps5_startup_log("SDL_Init", "ok");
    ps5_start_menu_input();
#endif

    int window_width = 800;
    int window_height = 600;
#ifdef CLANBOMBER_PS5
#if defined(CLANBOMBER_PS5_VULKAN)
    // Update the SDL2 PS5 GPU output mode before reading the desktop extent.
    if (SDL_Vulkan_LoadLibrary(nullptr) != 0) {
        ps5_startup_log("static Vulkan load", SDL_GetError());
        return -1;
    }
    struct VulkanLoaderReference {
        ~VulkanLoaderReference() { SDL_Vulkan_UnloadLibrary(); }
    } loader_reference;
#endif
    SDL_DisplayMode display_mode{};
    if (SDL_GetDesktopDisplayMode(0, &display_mode) != 0) {
        ps5_startup_log("desktop display mode", SDL_GetError());
        return -1;
    }
    window_width = display_mode.w;
    window_height = display_mode.h;
#if defined(CLANBOMBER_PS5_VULKAN)
    const Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_VULKAN;
#elif defined(CLANBOMBER_PS5_GPU)
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES) != 0 ||
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2) != 0 ||
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0) != 0) {
        ps5_startup_log("GPU context attributes", SDL_GetError());
        return -1;
    }
    const Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL;
#else
    const Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS;
#endif
#elif defined(__ANDROID__)
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    const Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
    const Uint32 fullscreen = Config::get_fullscreen() ? SDL_WINDOW_FULLSCREEN : 0;
    const Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | fullscreen;
#endif
	Uint32 renderMode = (Config::get_softwareRendering()) ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED;

	if (Config::get_fullscreen()) SDL_SetRelativeMouseMode(SDL_TRUE);

	keyboard = SDL_GetKeyboardState(NULL);
	
	#ifdef SDL_HINT_RENDER_BATCHING
	SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1"); // enable batch rendering, it's faster
	#endif
	
	#ifdef SDL_HINT_RENDER_SCALE_QUALITY
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
	#endif
	
	if (!Config::get_renderDriver().empty()){
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, Config::get_renderDriver().c_str());
	}

#ifdef CLANBOMBER_PS5
    ps5_startup_log("SDL_CreateWindow", "begin");
#endif
	gameWindow = SDL_CreateWindow(PACKAGE_STRING,
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          window_width, window_height, window_flags);

#ifdef CLANBOMBER_PS5
    if (gameWindow) ps5_startup_log("SDL_CreateWindow", "ok");
    if (!gameWindow) {
        ps5_startup_log("SDL window creation", SDL_GetError());
        return -1;
    }
#endif
#ifdef CLANBOMBER_PS5
    ps5_startup_log("SDL_CreateRenderer", "begin");
#endif
#ifdef CLANBOMBER_PS5
    int renderer_index = -1;
    for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
        SDL_RendererInfo driver{};
        if (SDL_GetRenderDriverInfo(i, &driver) == 0 &&
            (driver.flags & renderMode) == renderMode
#if defined(CLANBOMBER_PS5_VULKAN)
            && SDL_strcmp(driver.name, "ps5vulkan") == 0
#endif
        ) {
            renderer_index = i;
            ps5_startup_log("requested renderer", driver.name);
            break;
        }
    }
#if defined(CLANBOMBER_PS5_VULKAN)
    if (renderer_index < 0) {
        ps5_startup_log("SDL renderer selection", "required ps5vulkan driver is unavailable");
        return -1;
    }
#endif
    renderer = SDL_CreateRenderer(gameWindow, renderer_index, renderMode);
#else
    renderer = SDL_CreateRenderer(gameWindow, -1, renderMode);
#endif
#ifdef CLANBOMBER_DESKTOP
    if (!gameWindow || !renderer) {
        std::cerr << "Unable to create the game window or renderer: " << SDL_GetError() << std::endl;
        return -1;
    }
#endif
#ifdef CLANBOMBER_PS5
#if !defined(CLANBOMBER_PS5_GPU) && !defined(CLANBOMBER_PS5_VULKAN)
    if (!renderer) {
        ps5_startup_log("desktop renderer failed; diagnostic fallback to 800x600", SDL_GetError());
        if (SDL_SetWindowFullscreen(gameWindow, 0) == 0) {
            SDL_SetWindowSize(gameWindow, 800, 600);
            renderer = SDL_CreateRenderer(gameWindow, -1, renderMode);
        }
    }
#endif
    if (!renderer) {
        ps5_startup_log("SDL renderer creation", SDL_GetError());
        return -1;
    }
#endif

#ifdef CLANBOMBER_PS5
    int output_width = 0;
    int output_height = 0;
    if (SDL_GetRendererOutputSize(renderer, &output_width, &output_height) != 0 ||
        output_width <= 0 || output_height <= 0 ||
        SDL_RenderSetScale(renderer, output_width / 800.0f, output_height / 600.0f) != 0) {
        ps5_startup_log("display setup failed", SDL_GetError());
        return -1;
    }
    SDL_RendererInfo renderer_info{};
    int actual_width = 0;
    int actual_height = 0;
    SDL_GetWindowSize(gameWindow, &actual_width, &actual_height);
    SDL_DisplayMode present_mode{};
    const bool have_present_mode = SDL_GetCurrentDisplayMode(0, &present_mode) == 0;
    char detail[192];
    if (SDL_GetRendererInfo(renderer, &renderer_info) == 0) {
        std::snprintf(detail, sizeof(detail), "%s flags=0x%x",
                      renderer_info.name, renderer_info.flags);
        ps5_startup_log("renderer backend", detail);
    }
#ifdef CLANBOMBER_PS5_GPU
    using GLGetString = const unsigned char *(*)(unsigned int);
    auto get_gl_string = reinterpret_cast<GLGetString>(SDL_GL_GetProcAddress("glGetString"));
    if (get_gl_string) {
        const auto *device_name = get_gl_string(0x1F01); /* GL_RENDERER */
        ps5_startup_log("GPU device", device_name ? reinterpret_cast<const char *>(device_name) : "unknown");
    } else {
        ps5_startup_log("GPU device", "glGetString unavailable");
    }
#endif
    std::snprintf(detail, sizeof(detail), "window=%dx%d output=%dx%d present=%dx%d",
                  actual_width, actual_height, output_width, output_height,
                  have_present_mode ? present_mode.w : 0,
                  have_present_mode ? present_mode.h : 0);
    ps5_startup_log("display", detail);
#else
    SDL_RenderSetLogicalSize(renderer, 800, 600);
#endif

   	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

#ifdef CLANBOMBER_PS5
    ps5_startup_log("audio mixer", "begin");
#endif
	AS = new cbe::AudioSimpleSDL();
	if (AS == NULL) {
		std::cerr << _("Cannot initialise the requested audio plugin!")
				<< std::endl;
	}
	if (!AS->init()) {
		std::cerr << _("Cannot Initialise SDL audio!") << std::endl;
	}

#ifdef CLANBOMBER_PS5
    ps5_startup_log("TTF_Init", "begin");
#endif
	if (TTF_Init()) {
		std::cerr << _("Cannot Initialise SDL ttf!") << std::endl;
#ifdef CLANBOMBER_PS5
        ps5_startup_log("SDL ttf init", TTF_GetError());
#endif
		TTF_Quit();
		AS->close();
		SDL_Quit();
		return -1;
	}

	return 0;
}

bool ClanBomberApplication::paused_game() {
	return pause_game;
}

void ClanBomberApplication::set_pause_game(bool paused) {
	pause_game = paused;
}

void ClanBomberApplication::reload_map() {
	if (map) {
		delete map;
	}
	map = new Map(this);
}

unsigned short ClanBomberApplication::get_next_object_id() {
	if (next_object_id++ > 65534) {
		next_object_id = 23;
	}
	return next_object_id;
}

int ClanBomberApplication::main() {
	std::srand(std::time(NULL));

	if (init_SDL()) {
		return -1;
	}

	show_fps = false;
	play_music = false;
	map_path = CB_ASSET("/maps");

	std::filesystem::path configpath(GetConfigHome() / "clanbomber");
	if (!RecursiveDirCreation(configpath)) {
		std::string output = _("Config directory ") + configpath.string() + (" cannot be created");
		std::cout << output << std::endl;
	}

	Config::set_path(configpath);
	Config::set_filename("clanbomber.cfg");

	std::filesystem::path mappath(GetDataHome() / "clanbomber" / "maps");
	if (!RecursiveDirCreation(mappath)) {
		std::string output = _("Data directory ") + configpath.string() + (" cannot be created");
		std::cout << output << std::endl;
	}
	local_map_path = mappath;

#ifdef CLANBOMBER_PS5
    ps5_startup_log("resources", "begin");
#endif
	Resources::init();
#ifdef CLANBOMBER_PS5
    ps5_startup_log("resources", "ok");
#endif
	Config::load();
#ifdef CLANBOMBER_PS5
    ps5_startup_log("intro credit", "preloading original logo");
    Resources::Intro_fl_logo();
    ps5_startup_log("match resources", "begin");
    Resources::Game_putbomb();
    Resources::Game_explode();
    Resources::Game_bombs();
    // Player Setup can select any stock skin, including slots unused at boot.
    for (int skin = 0; skin < NR_BOMBERSKINS; ++skin)
        Resources::Bombers(skin);
    Resources::Game_maptiles();
    Resources::Game_explosion();
    ps5_startup_log("match resources", "preload finished");
#endif
#ifdef CLANBOMBER_PS5
    ps5_startup_log("intro", "begin");
#endif
	run_intro();
#ifdef CLANBOMBER_PS5
    ps5_startup_log("intro", "ok; preparing main menu");
#endif

	map = NULL;
	observer = NULL;

	menu = new Menu(_("Main Menu"), this);

	menu->add_item(_("New Game"), MENU_GAME);
	menu->add_item(_("Start Local"), LOCALGAME_START, MENU_GAME);
	menu->add_item(_("Local Player Setup"), MENU_PLAYER_SETUP, MENU_GAME);
#if defined(CLANBOMBER_PS5) || defined(CLANBOMBER_DESKTOP)
    static std::string cpu_normal = _("Normal");
    static std::string cpu_easy = _("Easy");
    static std::string cpu_hard = _("Hard");
    menu->add_stringlist(_("CPU Difficulty"), CONFIG_CPU_DIFFICULTY, MENU_GAME,
                         {&cpu_easy, &cpu_normal, &cpu_hard}, cpu_difficulty::to_menu(Config::cpu_difficulty));
#endif
	menu->add_item(_("Map Selection"), CONFIG_MAP_SEL, MENU_GAME);
	menu->add_value(_("Random Bomber Positions"), CONFIG_RANDOM_POSITIONS,
			MENU_GAME, 0, 1, Config::get_random_positions());
	menu->add_value(_("Random Map Order"), CONFIG_RANDOM_MAP_ORDER, MENU_GAME,
			0, 1, Config::get_random_map_order());
	menu->add_value(_("Points to win"), CONFIG_POINTS, MENU_GAME, 1, 10,
			Config::get_points_to_win());
	menu->add_value(_("Round Time"), CONFIG_ROUND_TIME, MENU_GAME, 5, 300,
			Config::get_round_time());

	menu->add_item(_("Options"), MENU_OPTIONS);
	menu->add_item(_("Start/Max Extras"), MENU_EXTRA_VALUES, MENU_OPTIONS);
	menu->add_value(_("Start Bombs"), CONFIG_START_BOMBS, MENU_EXTRA_VALUES, 1,
			15, Config::get_start_bombs());
	menu->add_value(_("Start Power"), CONFIG_START_POWER, MENU_EXTRA_VALUES, 1,
			15, Config::get_start_power());
	menu->add_value(_("Start Skateboards"), CONFIG_START_SKATES,
			MENU_EXTRA_VALUES, 0, 10, Config::get_start_skateboards());
	menu->add_value(_("Start Kick"), CONFIG_START_KICK, MENU_EXTRA_VALUES, 0, 1,
			Config::get_start_kick());
	menu->add_value(_("Start Glove"), CONFIG_START_GLOVE, MENU_EXTRA_VALUES, 0,
			1, Config::get_start_glove());
	menu->add_value(_("Max. Bombs"), CONFIG_MAX_BOMBS, MENU_EXTRA_VALUES, 1, 15,
			Config::get_max_bombs());
	menu->add_value(_("Max. Power"), CONFIG_MAX_POWER, MENU_EXTRA_VALUES, 1, 15,
			Config::get_max_power());
	menu->add_value(_("Max. Skateboards"), CONFIG_MAX_SKATES, MENU_EXTRA_VALUES,
			0, 10, Config::get_max_skateboards());
	menu->add_item(_("Enable/Disable Extras"), MENU_EXTRA_ONOFF, MENU_OPTIONS);
	menu->add_value(_("Bombs"), CONFIG_BOMBS, MENU_EXTRA_ONOFF, 0, 1,
			Config::get_bombs());
	menu->add_value(_("Power"), CONFIG_POWER, MENU_EXTRA_ONOFF, 0, 1,
			Config::get_power());
	menu->add_value(_("Skateboard"), CONFIG_SKATES, MENU_EXTRA_ONOFF, 0, 1,
			Config::get_skateboards());
	menu->add_value(_("Kick"), CONFIG_KICK, MENU_EXTRA_ONOFF, 0, 1,
			Config::get_kick());
	menu->add_value(_("Glove"), CONFIG_GLOVE, MENU_EXTRA_ONOFF, 0, 1,
			Config::get_glove());
	menu->add_item(_("Enable/Disable Diseases"), MENU_DISEASE_ONOFF,
			MENU_OPTIONS);
	menu->add_value(_("Joint"), CONFIG_JOINT, MENU_DISEASE_ONOFF, 0, 1,
			Config::get_joint());
	menu->add_value(_("Viagra"), CONFIG_VIAGRA, MENU_DISEASE_ONOFF, 0, 1,
			Config::get_viagra());
	menu->add_value(_("Koks"), CONFIG_KOKS, MENU_DISEASE_ONOFF, 0, 1,
			Config::get_koks());
	menu->add_item(_("Bomb Timing and Speed"), MENU_TIMING, MENU_OPTIONS);
	menu->add_value(_("Bomb Countdown (1/10 s)"), CONFIG_BOMB_COUNTDOWN,
			MENU_TIMING, 0, 50, Config::get_bomb_countdown());
	menu->add_value(_("Bomb Chain Reaction Delay (1/100 s)"), CONFIG_BOMB_DELAY,
			MENU_TIMING, 0, 50, Config::get_bomb_delay());
	menu->add_value(_("Moving Bombs Speed (pixels per second)"),
			CONFIG_BOMB_SPEED, MENU_TIMING, 10, 500, Config::get_bomb_speed());
	menu->add_item(_("Graphic Options"), MENU_GRAPHICS, MENU_OPTIONS);
	menu->add_value(_("Kidz Mode"), CONFIG_KIDS_MODE, MENU_GRAPHICS, 0, 1,
			Config::get_kids_mode());
	menu->add_value(_("Corpse Parts"), CONFIG_CORPSE_PARTS, MENU_GRAPHICS, 0,
			100, Config::get_corpse_parts());
	menu->add_value(_("Shaky Explosions"), CONFIG_SHAKE, MENU_GRAPHICS, 0, 1,
			Config::get_shaky_explosions());
	menu->add_value(_("Random Bomber Positions"),
			CONFIG_RANDOM_POSITIONS_DUPLICATE, MENU_OPTIONS, 0, 1,
			Config::get_random_positions());
	menu->add_value(_("Random Map Order"), CONFIG_RANDOM_MAP_ORDER_DUPLICATE,
			MENU_OPTIONS, 0, 1, Config::get_random_map_order());
	menu->add_value(_("Points to win"), CONFIG_POINTS_DUPLICATE, MENU_OPTIONS,
			1, 10, Config::get_points_to_win());
	menu->add_value(_("Round Time"), CONFIG_ROUND_TIME_DUPLICATE, MENU_OPTIONS,
			5, 300, Config::get_round_time());

	menu->backup_options_values();
	menu->save_common_options(0, false, true);

	menu->add_item(_("Map Editor"), MENU_MAP_EDITOR);
	menu->add_item(_("Show Credits"), MENU_CREDITS);
	menu->add_item(_("Help Screen"), MENU_HELP);
#ifndef __SWITCH__
	// On Switch the player exits via the Home button — no in-app Quit needed,
	// and the upstream Quit path runs SDL_Quit() which crashes audren teardown.
	menu->add_item(_("Quit Game"), MENU_EXIT);
#endif

	menu->scroll_in();

	while (1) {
		int result;

		result = menu->execute(false);

		MenuItem *item = menu->get_item_by_id(result);
		switch (result) {
		case MENU_EXIT:
			menu->scroll_out();
			delete menu;
			return 0;
		case MENU_PLAYER_SETUP:
			menu->scroll_out();
			{
				PlayerSetup ps(this);
				ps.exec();
			}
			menu->scroll_in();
			break;
		case CONFIG_START_BOMBS:
			Config::set_start_bombs(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_START_POWER:
			Config::set_start_power(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_START_SKATES:
			Config::set_start_skateboards(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_START_KICK:
			Config::set_start_kick(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_START_GLOVE:
			Config::set_start_glove(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_BOMBS:
			Config::set_bombs(static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_POWER:
			Config::set_power(static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_SKATES:
			Config::set_skateboards(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_KICK:
			Config::set_kick(static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_GLOVE:
			Config::set_glove(static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_JOINT:
			Config::set_joint(static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_VIAGRA:
			Config::set_viagra(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_KOKS:
			Config::set_koks(static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_MAX_BOMBS:
			Config::set_max_bombs(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_MAX_POWER:
			Config::set_max_power(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_MAX_SKATES:
			Config::set_max_skateboards(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case MENU_CREDITS:
			menu->scroll_out();
			{
				Credits credits(this);
				credits.exec();
			}
			menu->scroll_in();
			break;
		case MENU_MAP_EDITOR:
#ifdef CLANBOMBER_PS5
            ps5_startup_log("map editor", "begin");
#endif
			menu->scroll_out();
			{
				MapEditor me(this);
				me.exec();
			}
			menu->scroll_in();
			break;
		case MENU_HELP:
			menu->scroll_out();
			show_tutorial();
			menu->scroll_in();
			break;
		case CONFIG_MAP_SEL:
			menu->scroll_out();
			{
				MapSelector ms(this);
				ms.exec();
			}
			menu->scroll_in();
			break;
		case CONFIG_POINTS:
			Config::set_points_to_win(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_POINTS_DUPLICATE:
			Config::set_points_to_win(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_ROUND_TIME:
			if (static_cast<MenuItem_Value *>(item)->get_value() % 5 == 1) {
				static_cast<MenuItem_Value *>(item)->set_value(
						static_cast<MenuItem_Value *>(item)->get_value() + 4);
			} else if (static_cast<MenuItem_Value *>(item)->get_value() % 5
					== 4) {
				static_cast<MenuItem_Value *>(item)->set_value(
						static_cast<MenuItem_Value *>(item)->get_value() - 4);
			}
			Config::set_round_time(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_ROUND_TIME_DUPLICATE:
			if (static_cast<MenuItem_Value *>(item)->get_value() % 5 == 1) {
				static_cast<MenuItem_Value *>(item)->set_value(
						static_cast<MenuItem_Value *>(item)->get_value() + 4);
			} else if (static_cast<MenuItem_Value *>(item)->get_value() % 5
					== 4) {
				static_cast<MenuItem_Value *>(item)->set_value(
						static_cast<MenuItem_Value *>(item)->get_value() - 4);
			}
			Config::set_round_time(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_KIDS_MODE:
			Config::set_kids_mode(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_CORPSE_PARTS:
			Config::set_corpse_parts(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_SHAKE:
			Config::set_shaky_explosions(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_BOMB_COUNTDOWN:
			Config::set_bomb_countdown(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_BOMB_DELAY:
			Config::set_bomb_delay(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_BOMB_SPEED:
			if (static_cast<MenuItem_Value *>(item)->get_value() % 10 == 1) {
				static_cast<MenuItem_Value *>(item)->set_value(
						static_cast<MenuItem_Value *>(item)->get_value() + 9);
			} else if (static_cast<MenuItem_Value *>(item)->get_value() % 10
					== 9) {
				static_cast<MenuItem_Value *>(item)->set_value(
						static_cast<MenuItem_Value *>(item)->get_value() - 9);
			}
			Config::set_bomb_speed(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_RANDOM_POSITIONS:
			Config::set_random_positions(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_RANDOM_POSITIONS_DUPLICATE:
			Config::set_random_positions(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_RANDOM_MAP_ORDER:
			Config::set_random_map_order(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
		case CONFIG_RANDOM_MAP_ORDER_DUPLICATE:
			Config::set_random_map_order(
					static_cast<MenuItem_Value *>(item)->get_value());
			Config::save();
			break;
#if defined(CLANBOMBER_PS5) || defined(CLANBOMBER_DESKTOP)
        case CONFIG_CPU_DIFFICULTY:
            Config::cpu_difficulty = cpu_difficulty::from_menu(static_cast<MenuItem_StringList *>(item)->get_value());
            Config::save();
            break;
#endif
		case LOCALGAME_START: {
#ifdef CLANBOMBER_PS5
            ps5_startup_log("local match", "selected");
#endif
			menu->scroll_out();
			if (Config::get_number_of_opponents() > 1) {
                int restart_map = -1;
                while (init_game(restart_map)) {
                    const MatchExit result = run_game();
                    if (result == MatchExit::Restart) {
                        const auto &levels = map->map_list;
                        const auto current = std::find(levels.begin(), levels.end(), map->get_current_map());
                        restart_map = current == levels.end() ? -1 : static_cast<int>(current - levels.begin());
                    }
                    deinit_game();
                    if (result == MatchExit::Title) menu->return_to_title();
                    if (result != MatchExit::Restart || restart_map < 0) break;
                }
			} else {
				menu->scroll_in();
			}
			break;
        }
		default:
			std::cout << result << std::endl;
			break;
		}
	}
}

ClanBomberApplication::MatchExit ClanBomberApplication::run_game() {
    MatchExit result = MatchExit::Finished;
#if defined(CLANBOMBER_PS5) || defined(CLANBOMBER_DESKTOP)
    bool confirm_pause_choice = false;
    enum PauseChoice { Resume, RestartMatch, ReturnToTitle, ChoiceCount };
    int pause_choice = Resume;
    const char *choices[] = {_("Resume"), _("Restart Match"), _("Return to Title")};
    bool pause_needs_draw = true;
#endif
#ifdef CLANBOMBER_PS5
    ps5_set_match_input(true);
    pause_game = false;
#endif

    cbe::Music *match_music = nullptr;
    bool music_requested = false;

	Timer::reset();

	bool escape = false;
	while (!escape) {

		Timer::time_elapsed(true);

#ifndef CLANBOMBER_PS5
        SDL_RenderClear(renderer);
#endif

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
#ifdef CLANBOMBER_PS5
            Controller_Joystick::handle_event(event, pause_game);
            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_EXPOSED ||
                 event.window.event == SDL_WINDOWEVENT_RESTORED))
                pause_needs_draw = true;
#endif
			if (event.type == SDL_KEYDOWN) {
#if defined(CLANBOMBER_PS5) || defined(CLANBOMBER_DESKTOP)
                if (event.key.repeat) continue;
                if (pause_game) {
                    switch (event.key.keysym.scancode) {
                    case SDL_SCANCODE_UP:
                    case SDL_SCANCODE_DOWN:
                        if (!confirm_pause_choice) {
                            const int step = event.key.keysym.scancode == SDL_SCANCODE_UP ? -1 : 1;
                            pause_choice = (pause_choice + step + ChoiceCount) % ChoiceCount;
                            pause_needs_draw = true;
                        }
                        continue;
                    case SDL_SCANCODE_BACKSPACE:
                        if (confirm_pause_choice) pause_needs_draw = true;
                        else {
                            pause_game = false;
#ifdef CLANBOMBER_PS5
                            Controller_Joystick::pause_transition(true);
#endif
                        }
                        confirm_pause_choice = false;
                        continue;
                    case SDL_SCANCODE_RETURN:
                        if (confirm_pause_choice) {
                            result = pause_choice == RestartMatch ? MatchExit::Restart : MatchExit::Title;
                            escape = true;
                        } else if (pause_choice != Resume) {
                            confirm_pause_choice = true;
                            pause_needs_draw = true;
                        } else {
                            pause_game = false;
#ifdef CLANBOMBER_PS5
                            Controller_Joystick::pause_transition(true);
#endif
                        }
                        break;
                    default:
                        break;
                    }
                    if (escape) break;
                }
#endif
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_F1:
					show_fps = !show_fps;
					break;
				case SDL_SCANCODE_F2:
					play_music = !play_music;
					break;
				case SDL_SCANCODE_P:
				case SDL_SCANCODE_F3:
					pause_game = !pause_game;
#ifdef CLANBOMBER_PS5
                    Controller_Joystick::pause_transition(false);
#endif
#if defined(CLANBOMBER_PS5) || defined(CLANBOMBER_DESKTOP)
                    confirm_pause_choice = false;
                    pause_choice = Resume;
                    pause_needs_draw = true;
#ifdef CLANBOMBER_PS5
                    ps5_startup_log("local match pause", pause_game ? "paused" : "resumed");
#endif
#endif
					break;
				case SDL_SCANCODE_ESCAPE:
					escape = true;
					break;
				}
			}
		}

#if defined(CLANBOMBER_PS5) || defined(CLANBOMBER_DESKTOP)
        if (escape) break;
#endif
        if (play_music != music_requested) {
            music_requested = play_music;
            if (music_requested) {
                if (!match_music) match_music = Resources::Music();
                match_music->playLoop();
            } else if (match_music) {
                match_music->stop();
            }
        }

#ifdef CLANBOMBER_PS5
        if (escape) break;
        if (pause_game && !pause_needs_draw) {
            SDL_Delay(8);
            continue;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
#endif



		if (pause_game) {
			show_all();
			CB_FillRect(0, 0, 800, 600, 0, 0, 0, 123);
#if defined(CLANBOMBER_PS5) || defined(CLANBOMBER_DESKTOP)
            pause_needs_draw = false;
            CB_FillRect(230, 225, 340, 210, 0, 0, 255, 135);
            Resources::Font_big()->render(_("Paused"), 400, 240, cbe::FontAlignment_0topcenter);
            auto *font = Resources::Font_small();
            for (int choice = 0; choice < ChoiceCount; ++choice) {
                const int y = 294 + choice * 32;
                if (choice == pause_choice)
                    CB_FillRect(230, y - 4, 340, 30, 0, 30, 170, 170);
                font->render(choices[choice], 400, y, cbe::FontAlignment_0topcenter);
            }
            if (confirm_pause_choice) {
                font->render(pause_choice == RestartMatch ? _("Restart Match?") : _("Return to Title?"),
                             400, 400, cbe::FontAlignment_0topcenter);
            }
#else
            Resources::Font_big()->render(_("- PAUSED -"), 400, 300,
                                          cbe::FontAlignment_0topcenter);
#endif
			SDL_RenderPresent(renderer);
			continue;
		}

		if (observer != NULL) {
			observer->act();
			if (observer->end_of_game_requested()) {
				break;
			}
		}

		delete_some();
		act_all();
		show_all();

		SDL_RenderPresent(renderer);
		frame_count++;

		frame_time += Timer::time_elapsed();
		if (frame_time > 2) {
			fps = (int) (frame_count / frame_time + 0.5f);
			frame_time = 0;
			frame_count = 0;
		}
	}

#ifdef CLANBOMBER_PS5
    ps5_set_match_input(false);
    pause_game = false;
    if (result != MatchExit::Finished) {
        SDL_PumpEvents();
        SDL_FlushEvent(SDL_KEYDOWN);
        ps5_startup_log("local match", result == MatchExit::Restart ? "restart confirmed" : "return to title confirmed");
    }
#endif
    if (match_music && match_music->isPlaying())
        match_music->stop();
    return result;
}

Menu *
ClanBomberApplication::get_menu() {
	return app->menu;
}

void ClanBomberApplication::make_observer() {
	if (observer == NULL) {
		observer = new Observer(0, 0, this);
	}
}

void ClanBomberApplication::make_map() {
    if (map == NULL) {
#ifdef CLANBOMBER_PS5
        ps5_startup_log("local map", "begin construction");
#endif
        map = new Map(this);
#ifdef CLANBOMBER_PS5
        ps5_startup_log("local map", "constructed");
#endif
	}
}

Observer *
ClanBomberApplication::get_observer() {
	return observer;
}

Map *
ClanBomberApplication::get_map() {
	return map;
}

void ClanBomberApplication::delete_all_game_objects() {
	objects.clear();
	bomber_objects.clear();
}

GameObject *
ClanBomberApplication::get_object_by_id(int object_id) {
	for (const auto &bomber_object_iter : bomber_objects) {
		if ((bomber_object_iter)->get_object_id() == object_id) {
			return bomber_object_iter;
		}
	}
	for (const auto &game_object_iter : objects) {
		if ((game_object_iter)->get_object_id() == object_id) {
			return game_object_iter;
		}
	}
	return NULL;
}

void ClanBomberApplication::act_all() {
	// Map acts first
	if (map != NULL) {
		map->act();
	}

	for (const auto & object_iter : objects) {
		object_iter->act();
	}

	for (const auto & bomber_object_iter : bomber_objects) {
		bomber_object_iter->act();
	}
}

void ClanBomberApplication::delete_some() {
	// Check if we should delete some

	for (auto object_iter = objects.begin(); object_iter != objects.end();) {
		if ((*object_iter)->delete_me) {
			delete *object_iter;
			object_iter = objects.erase(object_iter);
		} else {
			++object_iter;
		}
	}
}

void ClanBomberApplication::show_all() {
	GameObject *draw_list[objects.size() + bomber_objects.size()];
	int n = 0;
	int i;

	const int top_height = 40;
	const int margin = 60;

	if (show_fps) {
		std::string nstr = std::to_string(fps) + _(" fps");
		Resources::Font_small()->render(nstr, 535, 4, cbe::FontAlignment_0topcenter);
	}

	for (const auto &object_iter : objects) {
		draw_list[n] = (object_iter);
		n++;
	}
	for (const auto &bomber_object_iter : bomber_objects) {
		draw_list[n] = (bomber_object_iter);
		n++;
	}

	std::sort(draw_list, draw_list + n, game_object_compare);

	Resources::Font_small()->render(app->map->get_name(), 80, 5, cbe::FontAlignment_0topleft);
	Resources::Game_cb_logo_small()->blit(325, 0);

	for (i = 0; i < n; i++) {
		if (draw_list[i] != NULL && draw_list[i]->get_z() < Z_GROUND) {
			draw_list[i]->show();
		}
	}

	if (map != NULL) {
		map->show();
	}
	
	for (i = 0; i < n; i++) {
		if (draw_list[i] != NULL && draw_list[i]->get_z() >= Z_GROUND) {
			draw_list[i]->show();
		}
	}

	if (observer != NULL) {
		observer->show();
	}
}

bool ClanBomberApplication::init_game(int restart_map) {
#ifdef CLANBOMBER_PS5
    ps5_startup_log("local match", "begin initialization");
#endif
	frame_count = 0;
	frame_time = 0;
	fps = 0;

	// init map
	if (map == NULL) {
		map = new Map(this);
	}

	if (!map->any_valid_map()) {
		delete map;
		map = NULL;
		return false;
	}

	if (restart_map >= 0) {
        map->load(restart_map);
    } else if (Config::get_random_map_order()) {
		map->load_random_valid();
	} else {
		if (Config::get_start_map() > map->get_map_count() - 1) {
			Config::set_start_map(map->get_map_count() - 1);
		}
		map->load_next_valid(Config::get_start_map());
	}
	// init GameObjects
	if (Config::get_random_positions()) {
		map->randomize_bomber_positions();
	}
	CL_Vector pos;
	int j = 0;
	for (int i = 0; i < 8; i++) {
		if (Config::bomber[i].is_enabled()) {
			pos = map->get_bomber_pos(j++);
#if defined(CLANBOMBER_PS5) || defined(CLANBOMBER_DESKTOP)
            const int controller_type = Config::bomber[i].get_controller();
            const bool cpu = controller_type == Controller::AI || controller_type == Controller::AI_mass;
            const unsigned long draw = cpu && Config::random_cpu_skin[i] ? fast_random() : 0;
            const int chosen_skin = skin_selection::resolve(Config::bomber[i].get_skin(),
                                                             Config::random_cpu_skin[i], cpu, draw, NR_BOMBERSKINS);
#else
            const int chosen_skin = Config::bomber[i].get_skin();
#endif
			new Bomber((int) (pos.x * 40), (int) (pos.y * 40),
					static_cast<Bomber::COLOR>(chosen_skin),
					Controller::create(
							static_cast<Controller::CONTROLLER_TYPE>(Config::bomber[i].get_controller())),
					Config::bomber[i].get_name(), Config::bomber[i].get_team(),
					i, this);
			bomber_objects.back()->set_pos(350, 270);
			bomber_objects.back()->fly_to((int) (pos.x * 40),
					(int) (pos.y * 40), 300);
			bomber_objects.back()->controller->deactivate();
		}
	}
	// this is for removing teams which only one player is in
	int team_count[] = { 0, 0, 0, 0 };
	for (int team = 0; team < 4; team++) {
		for (const auto &bomber_object_iter : bomber_objects) {
			if ((bomber_object_iter)->get_team() - 1 == team) {
				team_count[team]++;
			}
		}
	}
	for (const auto &bomber_object_iter : bomber_objects) {
		if ((bomber_object_iter)->get_team() != 0) {
			if (team_count[(bomber_object_iter)->get_team() - 1] == 1) {
				(bomber_object_iter)->set_team(0);
			}
		}
	}

	observer = new Observer(0, 0, this);

	return true;
}

#ifdef CLANBOMBER_DESKTOP
bool ClanBomberApplication::desktop_match_self_test() {
    map_path = CB_ASSET("/maps");
    local_map_path = std::filesystem::temp_directory_path() / "clanbomber-self-test-no-custom-maps";
    Config::bomber[0].set_controller(Controller::KEYMAP_1);
    Config::bomber[0].set_enabled(true);
    Config::bomber[1].set_controller(Controller::AI);
    Config::bomber[1].set_enabled(true);
    for (int i = 2; i < 8; ++i) Config::bomber[i].set_enabled(false);
    if (!init_game()) return false;
    const bool created = map && observer && bomber_objects.size() == 2;
    Timer::reset();
    for (int i = 0; i < 8; ++i) {
        SDL_Delay(10);
        Timer::time_elapsed(true);
        act_all();
        show_all();
        SDL_RenderPresent(renderer);
    }
    // Exercise the desktop pause menu and confirmed restart without user input.
    for (const SDL_Scancode key : {SDL_SCANCODE_P, SDL_SCANCODE_DOWN,
                                   SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN}) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = key;
        if (SDL_PushEvent(&event) != 1) return false;
    }
    const bool restart = run_game() == MatchExit::Restart;
    deinit_game();
    return created && restart && bomber_objects.empty();
}
#endif

void ClanBomberApplication::deinit_game() {
	// delete all GameObjects
	for (const auto &object_iter : objects) {
		delete object_iter;
	}
	objects.clear();
	for (const auto &bomber_object_iter : bomber_objects) {
		delete bomber_object_iter;
	}
	bomber_objects.clear();
	if (observer) {
		delete observer;
		observer = NULL;
	}
}

#if defined(CLANBOMBER_PS5) || defined(__ANDROID__)
static bool intro_skip_requested()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_CONTROLLERBUTTONDOWN &&
            (event.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
             event.cbutton.button == SDL_CONTROLLER_BUTTON_B ||
             event.cbutton.button == SDL_CONTROLLER_BUTTON_START))
            {
                SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
                SDL_FlushEvents(SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP);
                return true;
            }
        if (event.type == SDL_KEYDOWN && !event.key.repeat &&
            (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE ||
             event.key.keysym.scancode == SDL_SCANCODE_RETURN ||
             event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE))
            {
                SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
                SDL_FlushEvents(SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP);
                return true;
            }
    }
    return false;
}
#endif

void ClanBomberApplication::run_intro() {
	SDL_Scancode escape = SDL_SCANCODE_ESCAPE;
	float alpha = 0;
	std::string domination(_("A WORLD DOMINATION PROJECT"));
	std::string & domihack = domination;

	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

#if defined(CLANBOMBER_PS5) || defined(__ANDROID__)
    const Uint32 intro_start = SDL_GetTicks();
    while (SDL_GetTicks() - intro_start < 2000) {
        if (intro_skip_requested()) return;
        SDL_Delay(10);
    }
#else
    sleep(2);
#endif

	Timer::reset();
	Resources::Intro_winlevel()->play();
	while (!keyboard[escape]) {

#if defined(CLANBOMBER_PS5) || defined(__ANDROID__)
        if (intro_skip_requested()) return;
#else
        SDL_PumpEvents();
#endif

		SDL_RenderClear(renderer);

		if (alpha < 255) {
			Resources::Intro_fl_logo()->blit(100, 250, alpha);
			alpha += Timer::time_elapsed(true) * 130.0f;
		} else {
			Resources::Intro_fl_logo()->blit(100, 250);
			break;
		}

		SDL_RenderPresent(renderer);
	}

	int stringwidth;

	Resources::Font_small()->getSize(domination, &stringwidth, NULL);

	if (!keyboard[escape]) {
		//usleep (500000);

		for (int domispell = 0; domispell <= domihack.length(); domispell++) {

#if defined(CLANBOMBER_PS5) || defined(__ANDROID__)
            if (intro_skip_requested()) return;
#else
            SDL_PumpEvents();
#endif

			SDL_RenderClear(renderer);

			if (keyboard[escape]) {
				break;
			}

			if (domination.substr(domispell, 1) != " ") {
				Resources::Intro_typewriter()->play();
			}
			Resources::Intro_fl_logo()->blit(100, 250);
			Resources::Font_small()->render(domihack.substr(0, domispell),
					400 - stringwidth / 2, 360, cbe::FontAlignment_0topleft);

			SDL_RenderPresent(renderer);

			usleep(fast_random() % 100000 + 80000);
		}

		Resources::Menu_back()->play();

	}
}

void ClanBomberApplication::show_tutorial() {
#ifdef __SWITCH__
	// Switch port: show gamepad layout as the first help page.
	// Label column at x=70, value column pushed to x=440 so the long
	// "Select Current Map:" label doesn't collide with its single-letter value.
	{
		const int LABEL_X = 70;
		const int VALUE_X = 440;

		int y = 25;
		Resources::Titlescreen()->blit(0, 0);
		CB_FillRect(0, 0, 800, 600, 0, 0, 0, 128);
		Resources::Font_big()->render(_("Nintendo Switch Controls"), 400, y,
				cbe::FontAlignment_0topcenter);

		y += 70;
		Resources::Font_big()->render(_("Move:"), LABEL_X, y,
				cbe::FontAlignment_0topleft);
		Resources::Font_big()->render(_("D-Pad or Left Stick"), VALUE_X, y,
				cbe::FontAlignment_0topleft);

		y += 40;
		Resources::Font_big()->render(_("Drop Bomb:"), LABEL_X, y,
				cbe::FontAlignment_0topleft);
		Resources::Font_big()->render(_("A or B"), VALUE_X, y,
				cbe::FontAlignment_0topleft);

		y += 40;
		Resources::Font_big()->render(_("Quit Match:"), LABEL_X, y,
				cbe::FontAlignment_0topleft);
		Resources::Font_big()->render(_("Minus"), VALUE_X, y,
				cbe::FontAlignment_0topleft);

		y += 45;
		Resources::Font_big()->render(_("Menu Confirm:"), LABEL_X, y,
				cbe::FontAlignment_0topleft);
		Resources::Font_big()->render(_("A or Plus"), VALUE_X, y,
				cbe::FontAlignment_0topleft);

		y += 40;
		Resources::Font_big()->render(_("Menu Back:"), LABEL_X, y,
				cbe::FontAlignment_0topleft);
		Resources::Font_big()->render(_("B"), VALUE_X, y,
				cbe::FontAlignment_0topleft);

		// --- Player Setup section ---
		y += 45;
		Resources::Font_big()->render(_("Player Setup"), LABEL_X, y,
				cbe::FontAlignment_0topleft);

		y += 40;
		Resources::Font_big()->render(_("Toggle Player:"), LABEL_X, y,
				cbe::FontAlignment_0topleft);
		Resources::Font_big()->render(_("Y"), VALUE_X, y,
				cbe::FontAlignment_0topleft);

		y += 40;
		Resources::Font_big()->render(_("Highlight:"), LABEL_X, y,
				cbe::FontAlignment_0topleft);
		Resources::Font_big()->render(_("X"), VALUE_X, y,
				cbe::FontAlignment_0topleft);

		// --- Map Selection section ---
		y += 45;
		Resources::Font_big()->render(_("Map Selection"), LABEL_X, y,
				cbe::FontAlignment_0topleft);

		y += 40;
		Resources::Font_big()->render(_("Select All Maps:"), LABEL_X, y,
				cbe::FontAlignment_0topleft);
		Resources::Font_big()->render(_("L"), VALUE_X, y,
				cbe::FontAlignment_0topleft);

		y += 40;
		Resources::Font_big()->render(_("Select Current Map:"), LABEL_X, y,
				cbe::FontAlignment_0topleft);
		Resources::Font_big()->render(_("R"), VALUE_X, y,
				cbe::FontAlignment_0topleft);

		Resources::Font_big()->render(_("Press any key"), 400, 560,
				cbe::FontAlignment_0topcenter);
		SDL_RenderPresent(renderer);

		CB_WaitForKeypress();
	}
#endif

	int y = 25;
	Resources::Titlescreen()->blit(0, 0);
	CB_FillRect(0, 0, 800, 600, 0, 0, 0, 128);
	Resources::Font_big()->render(_("ClanBomber Extras"), 400, y,
			cbe::FontAlignment_0topcenter);
	y += 80;
	Resources::Extras_extras()->put_screen(15, y - 5, 0);
	Resources::Font_big()->render(_("Bomb:"), 70, y,
			cbe::FontAlignment_0topleft);
	Resources::Font_big()->render(_("You can place an additional bomb"), 250, y,
			cbe::FontAlignment_0topleft);

	y += 50;
	Resources::Extras_extras()->put_screen(15, y - 5, 1);
	Resources::Font_big()->render(_("Power:"), 70, y,
			cbe::FontAlignment_0topleft);
	Resources::Font_big()->render(_("Explosions grow one field in each "
			"direction"), 250, y, cbe::FontAlignment_0topleft);

	y += 50;
	Resources::Extras_extras()->put_screen(15, y - 5, 2);
	Resources::Font_big()->render(_("Skateboard:"), 70, y,
			cbe::FontAlignment_0topleft);
	Resources::Font_big()->render(_("Let's you move faster"), 250, y,
			cbe::FontAlignment_0topleft);

	y += 50;
	Resources::Extras_extras()->put_screen(15, y - 5, 3);
	Resources::Font_big()->render(_("Kick:"), 70, y,
			cbe::FontAlignment_0topleft);
	Resources::Font_big()->render(_("Kick bombs if you walk against one"), 250,
			y, cbe::FontAlignment_0topleft);

	y += 50;
	Resources::Extras_extras()->put_screen(15, y - 5, 4);
	Resources::Font_big()->render(_("Throw:"), 70, y,
			cbe::FontAlignment_0topleft);
	Resources::Font_big()->render(
			_("Throw Bombs if you press the button twice"), 250, y,
			cbe::FontAlignment_0topleft);
	Resources::Font_big()->render(_("without moving"), 250, y + 40,
			cbe::FontAlignment_0topleft);

	Resources::Font_big()->render(_("Press any key"), 400, 520,
			cbe::FontAlignment_0topcenter);
	SDL_RenderPresent(renderer);

	CB_WaitForKeypress();

	y = 25;
	Resources::Titlescreen()->blit(0, 0);
	CB_FillRect(0, 0, 800, 600, 0, 0, 0, 128);

	Resources::Font_big()->render(_("ClanBomber Drugs"), 400, y,
			cbe::FontAlignment_0topcenter);

	y += 80;
	Resources::Extras_extras()->put_screen(15, y - 5, 5);
	Resources::Font_big()->render(_("Joint:"), 70, y,
			cbe::FontAlignment_0topleft);
	Resources::Font_big()->render(_("Controller will be reversed"), 250, y,
			cbe::FontAlignment_0topleft);

	y += 50;
	Resources::Extras_extras()->put_screen(15, y - 5, 6);
	Resources::Font_big()->render(_("Viagra:"), 70, y,
			cbe::FontAlignment_0topleft);
	Resources::Font_big()->render(_("Autofire, this can be very dangerous!"),
			250, y, cbe::FontAlignment_0topleft);

	y += 50;
	Resources::Extras_extras()->put_screen(15, y - 5, 7);
	Resources::Font_big()->render(_("Cocaine:"), 70, y,
			cbe::FontAlignment_0topleft);
	Resources::Font_big()->render(_("Let's you move very fast!! (too fast)"),
			250, y, cbe::FontAlignment_0topleft);

	Resources::Font_big()->render(_("Press any key"), 400, 520,
			cbe::FontAlignment_0topcenter);
	SDL_RenderPresent(renderer);

	CB_WaitForKeypress();
}

std::filesystem::path ClanBomberApplication::get_map_path() {
	return map_path;
}

std::filesystem::path ClanBomberApplication::get_local_map_path() {
	return local_map_path;
}

#if !defined(__SWITCH__) && !defined(CLANBOMBER_PS5)
int main(int argc, char **argv) {

	CB_Locale();
#ifdef CLANBOMBER_DESKTOP
	bool self_test = false;
#endif
	// parse command line options
	for (int i = 1; i < argc; i++) {
#ifdef CLANBOMBER_DESKTOP
		if (!strcmp(argv[i], "--self-test")) {
			self_test = true;
			Config::set_softwareRendering(true);
		} else
#endif
		if (!strcmp(argv[i], "--help")) {
			std::cout
					<< _(
							"\n    usage: clanbomber2 [--software|--direct3d|--opengl|--opengles|--opengles2] [--fullscreen]\n\n");
			return 0;
		} else if (!strcmp(argv[i], "--fullscreen")) {
			Config::set_fullscreen(true);
		} else if (!strcmp(argv[i], "--software")) {
			Config::set_softwareRendering(true);
		} else if (!strcmp(argv[i], "--direct3d")) {
			Config::set_renderDriver("direct3d");
		} else if (!strcmp(argv[i], "--opengl")) {
			Config::set_renderDriver("opengl");
		} else if (!strcmp(argv[i], "--opengles")) {
			Config::set_renderDriver("opengles");
		} else if (!strcmp(argv[i], "--opengles2")) {
			Config::set_renderDriver("opengles2");
		} else {
			std::cout << _("Invalid argument") << std::endl;
		}
	}

	app = new ClanBomberApplication();
#ifdef CLANBOMBER_DESKTOP
	if (self_test) {
		if (app->desktop_init_for_self_test() != 0) return 1;
		bool ok = true;
		for (int skin = 0; skin < NR_BOMBERSKINS; ++skin) {
			auto *sprite = Resources::Bombers(skin);
			if (!sprite || !sprite->texture) { std::cerr << "Missing skin " << skin << std::endl; ok = false; continue; }
			for (int frame = 0; frame < 40; ++frame) sprite->put_screen(0, 0, frame);
		}
		SDL_Surface *title = IMG_Load(CB_ASSET("/pics/clanbomber_title_andi.png"));
		TTF_Font *font = TTF_OpenFont(CB_ASSET("/fonts/DejaVuSans-Bold.ttf"), 18);
		Mix_Chunk *sound = Mix_LoadWAV(CB_ASSET("/wavs/putbomb.wav"));
		if (!title || !font || !sound || !std::filesystem::exists(CB_ASSET("/maps/Ghostbear.map"))) ok = false;
		if (title) SDL_FreeSurface(title);
		if (font) TTF_CloseFont(font);
		if (sound) Mix_FreeChunk(sound);
		SDL_RenderPresent(renderer);
		if (!app->desktop_match_self_test()) { std::cerr << "Match initialization failed" << std::endl; ok = false; }
		delete app;
		std::cout << (ok ? "Desktop asset smoke test passed" : "Desktop asset smoke test failed") << std::endl;
		return ok ? 0 : 1;
	}
#endif
	int result = app->main();
	delete app;

	return result == 0 ? 0 : 1;
}
#endif /* __SWITCH__ */
