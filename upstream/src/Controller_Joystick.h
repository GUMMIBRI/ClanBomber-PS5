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

#ifndef CONTROLLER_JOYSTICK_H
#define CONTROLLER_JOYSTICK_H

#include "Controller.h"

#include <SDL2/SDL.h>
#ifdef CLANBOMBER_PS5
#include <vector>
#endif

class Bomber;

class Controller_Joystick : public Controller
{
public:
    Controller_Joystick(int joystick_nr);
    ~Controller_Joystick();

    void update();
    void reset();
    bool is_left();
    bool is_right();
    bool is_up();
    bool is_down();
    bool is_bomb();
#ifdef CLANBOMBER_PS5
    static void handle_event(const SDL_Event &event, bool paused);
    static void pause_transition(bool menu_button);
#endif

private:
    SDL_Joystick        *joystick;
    SDL_GameController  *gamecontroller;

#ifdef CLANBOMBER_PS5
    static std::vector<Controller_Joystick *> controllers;
    SDL_JoystickID instance_id = -1;
    bool bomb_press_pending = false;
    bool wait_for_bomb_release = false;
    unsigned bomb_buttons_held = 0;
#endif
    bool bomb_button_down;

    bool left;
    bool right;
    bool up;
    bool down;
};

#endif
