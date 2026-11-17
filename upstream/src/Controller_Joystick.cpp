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

#include "ClanBomber.h"
#include "Controller_Joystick.h"

#include <SDL2/SDL.h>
#include <cstdlib>
#include <algorithm>

Controller_Joystick::Controller_Joystick(int joystick_nr) : Controller()
{
    joystick = NULL;
    gamecontroller = NULL;

    // Prefer the SDL_GameController API: it gives a stable, named button /
    // axis layout (BUTTON_A, AXIS_LEFTX, ...) across platforms — including
    // Switch JoyCons / Pro Controller. Fall back to raw SDL_Joystick for
    // generic devices SDL doesn't recognise.
    if (SDL_IsGameController(joystick_nr))
    {
        gamecontroller = SDL_GameControllerOpen(joystick_nr);
    }
    if (gamecontroller == NULL)
    {
        joystick = SDL_JoystickOpen(joystick_nr);
    }
#ifdef CLANBOMBER_PS5
    SDL_Joystick *device = gamecontroller ? SDL_GameControllerGetJoystick(gamecontroller) : joystick;
    if (device) {
        instance_id = SDL_JoystickInstanceID(device);
        controllers.push_back(this);
    }
#endif
}

#ifdef CLANBOMBER_PS5
std::vector<Controller_Joystick *> Controller_Joystick::controllers;

void Controller_Joystick::handle_event(const SDL_Event &event, bool paused)
{
    for (auto *controller : controllers) {
        int button = -1;
        bool down = false;
        if (controller->gamecontroller &&
            (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) &&
            event.cbutton.which == controller->instance_id) {
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) button = 0;
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) button = 1;
            down = event.type == SDL_CONTROLLERBUTTONDOWN;
        } else if (controller->joystick &&
                   (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP) &&
                   event.jbutton.which == controller->instance_id && event.jbutton.button < 2) {
            button = event.jbutton.button;
            down = event.type == SDL_JOYBUTTONDOWN;
        }
        if (button < 0) continue;
        const unsigned bit = 1u << button;
        if (down) controller->bomb_buttons_held |= bit;
        else controller->bomb_buttons_held &= ~bit;
        if (!down && controller->bomb_buttons_held == 0)
            controller->wait_for_bomb_release = false;
        if (down && !paused && !controller->wait_for_bomb_release)
            controller->bomb_press_pending = true;
    }
}

extern SDL_JoystickID ps5_menu_pad_id();

void Controller_Joystick::pause_transition(bool menu_button)
{
    for (auto *controller : controllers) {
        controller->bomb_press_pending = false;
        controller->wait_for_bomb_release = controller->bomb_buttons_held != 0 ||
            (menu_button && controller->instance_id == ps5_menu_pad_id());
    }
}
#endif

Controller_Joystick::~Controller_Joystick()
{
#ifdef CLANBOMBER_PS5
    controllers.erase(std::remove(controllers.begin(), controllers.end(), this), controllers.end());
#endif
    if (gamecontroller != NULL)
    {
        SDL_GameControllerClose(gamecontroller);
        gamecontroller = NULL;
    }
    if (joystick != NULL)
    {
        SDL_JoystickClose(joystick);
        joystick = NULL;
    }
}

void Controller_Joystick::update()
{
    const Sint16 tolerance = 12000;
    if (!active) {
#ifdef CLANBOMBER_PS5
        bomb_press_pending = false;
#endif
        return;
    }

    bool b_left = false, b_right = false, b_up = false, b_down = false;
    bool b_bomb = false;

    if (gamecontroller != NULL)
    {
        // Left analog stick
        Sint16 ax = SDL_GameControllerGetAxis(gamecontroller, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ay = SDL_GameControllerGetAxis(gamecontroller, SDL_CONTROLLER_AXIS_LEFTY);
        if (ax >  tolerance) b_right = true;
        else if (ax < -tolerance) b_left  = true;
        if (ay >  tolerance) b_down  = true;
        else if (ay < -tolerance) b_up    = true;

        // D-Pad (OR with stick — either input drives the bomber)
        if (SDL_GameControllerGetButton(gamecontroller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  b_left  = true;
        if (SDL_GameControllerGetButton(gamecontroller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) b_right = true;
        if (SDL_GameControllerGetButton(gamecontroller, SDL_CONTROLLER_BUTTON_DPAD_UP))    b_up    = true;
        if (SDL_GameControllerGetButton(gamecontroller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  b_down  = true;

        // Bomb: A or B
        if (SDL_GameControllerGetButton(gamecontroller, SDL_CONTROLLER_BUTTON_A) ||
            SDL_GameControllerGetButton(gamecontroller, SDL_CONTROLLER_BUTTON_B))
        {
            b_bomb = true;
        }
    }
    else if (joystick != NULL)
    {
        // Fallback for raw SDL_Joystick devices.
        Sint16 x = SDL_JoystickGetAxis(joystick, 0);
        Sint16 y = SDL_JoystickGetAxis(joystick, 1);
        if (x >  tolerance) b_right = true;
        else if (x < -tolerance) b_left  = true;
        if (y >  tolerance) b_down  = true;
        else if (y < -tolerance) b_up    = true;

        if (SDL_JoystickNumHats(joystick) > 0)
        {
            Uint8 hat = SDL_JoystickGetHat(joystick, 0);
            if (hat & SDL_HAT_LEFT)  b_left  = true;
            if (hat & SDL_HAT_RIGHT) b_right = true;
            if (hat & SDL_HAT_UP)    b_up    = true;
            if (hat & SDL_HAT_DOWN)  b_down  = true;
        }

        int n = SDL_JoystickNumButtons(joystick);
        if ((n > 0 && SDL_JoystickGetButton(joystick, 0)) ||
            (n > 1 && SDL_JoystickGetButton(joystick, 1)))
        {
            b_bomb = true;
        }
    }

    left  = b_left;
    right = b_right;
    up    = b_up;
    down  = b_down;

    // Reverse handling (Joint disease etc.)
    if (reverse)
    {
        bool r_right = left;
        left  = right;
        right = r_right;

        bool r_down = up;
        up   = down;
        down = r_down;
    }

    // Bomb edge-trigger (one bomb per button press)
    if (b_bomb)
    {
        if (bomb_button_down)
        {
            put_bomb = true;
        }
        else
        {
            put_bomb = false;
        }
        bomb_button_down = false;
    }
    else
    {
        put_bomb = false;
        bomb_button_down = true;
    }
#ifdef CLANBOMBER_PS5
    if (wait_for_bomb_release) {
        put_bomb = false;
        bomb_press_pending = false;
        if (!b_bomb) wait_for_bomb_release = false;
    } else {
        put_bomb = bomb_press_pending || put_bomb;
        bomb_press_pending = false;
    }
#endif
}

void Controller_Joystick::reset()
{
    right = false;
    left  = false;
    down  = false;
    up    = false;

    put_bomb = false;
    bomb_button_down = true;
#ifdef CLANBOMBER_PS5
    bomb_press_pending = false;
#endif

    reverse = false;
}

bool Controller_Joystick::is_left()  { return left;  }
bool Controller_Joystick::is_right() { return right; }
bool Controller_Joystick::is_up()    { return up;    }
bool Controller_Joystick::is_down()  { return down;  }

bool Controller_Joystick::is_bomb()
{
    switch (bomb_mode)
    {
    case NEVER:
        return false;
    case ALWAYS:
        return true;
    default:
        break;
    }
    return put_bomb && active;
}
