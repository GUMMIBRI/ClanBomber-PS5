/*
 * This file is part of ClanBomber;
 * you can get it at "http://www.nongnu.org/clanbomber".
 *
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

#include "SurfaceSDL.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

extern SDL_Renderer *renderer;

#ifdef CLANBOMBER_PS5
#include <cerrno>
#include <cstdio>
#include <memory>
#include <libpng16/png.h>

void ps5_startup_log(const char *, const char *);

SDL_Surface *ps5_load_png(const char *filename, bool optional) {
    std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(filename, "rb"), std::fclose);
    if (!file) {
        if (!optional) {
            char detail[384];
            std::snprintf(detail, sizeof(detail), "errno=%d %.320s", errno, filename);
            ps5_startup_log("PNG open failed", detail);
        }
        return nullptr;
    }
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    const auto log_decode = [&] {
        char detail[384];
        std::snprintf(detail, sizeof(detail), "%.180s: %.150s", filename, image.message);
        ps5_startup_log("PNG decode failed", detail);
    };
    if (!png_image_begin_read_from_stdio(&image, file.get())) {
        log_decode();
        png_image_free(&image);
        return nullptr;
    }
    if (image.width == 0 || image.height == 0 || image.width > 4096 || image.height > 4096) {
        ps5_startup_log("PNG dimensions rejected", filename);
        png_image_free(&image);
        return nullptr;
    }
    image.format = PNG_FORMAT_RGBA;
    auto *decoded = SDL_CreateRGBSurfaceWithFormat(
        0, image.width, image.height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!decoded) {
        char detail[384];
        std::snprintf(detail, sizeof(detail), "%.180s: %.150s", filename, SDL_GetError());
        ps5_startup_log("PNG surface allocation failed", detail);
    } else if (!png_image_finish_read(&image, nullptr, decoded->pixels, decoded->pitch, nullptr)) {
        log_decode();
        SDL_FreeSurface(decoded);
        decoded = nullptr;
    }
    png_image_free(&image);
    return decoded;
}
#endif

namespace cbe
{
  SurfaceSDL::SurfaceSDL(const char *filename)
  {
#ifdef CLANBOMBER_PS5
    SDL_Surface *surface = ps5_load_png(filename, false);
#else
    SDL_Surface *surface = IMG_Load(filename);
#endif
    texture = surface ? SDL_CreateTextureFromSurface(renderer, surface) : nullptr;
#ifdef CLANBOMBER_PS5
    if (surface && !texture) {
        char detail[384];
        std::snprintf(detail, sizeof(detail), "%.180s: %.150s", filename, SDL_GetError());
        ps5_startup_log("PNG texture failed", detail);
    }
#endif
    if (texture) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    width = surface ? surface->w : 0;
    height = surface ? surface->h : 0;
    SDL_FreeSurface(surface);
  }

  SurfaceSDL::~SurfaceSDL()
  {
    SDL_DestroyTexture(texture);
  }

  void SurfaceSDL::blit(int x, int y, uint8_t opacity)
  {
    if (!texture) return;
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;

    SDL_Rect orig;
    orig.x=0;
    orig.y=0;
    orig.w=width;
    orig.h=height;

    SDL_SetTextureAlphaMod(texture, opacity);
    SDL_RenderCopy(renderer, texture, &orig, &rect);
    SDL_SetTextureAlphaMod(texture, SDL_ALPHA_OPAQUE);
  }
  void SurfaceSDL::scaled_blit(int x, int y, float scale_x, float scale_y, uint8_t opacity)
  {
    if (!texture) return;
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = width * scale_x;
    rect.h = height * scale_y;

    SDL_Rect orig;
    orig.x=0;
    orig.y=0;
    orig.w=width;
    orig.h=height;

    SDL_SetTextureAlphaMod(texture, opacity);
    SDL_RenderCopy(renderer, texture, &orig, &rect);
    SDL_SetTextureAlphaMod(texture, SDL_ALPHA_OPAQUE);
  }
};
