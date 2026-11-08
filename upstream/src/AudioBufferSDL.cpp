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

#include "AudioBufferSDL.h"

#ifdef CLANBOMBER_PS5
#include <cstdio>
#include <cstring>
#include <memory>
void ps5_startup_log(const char *, const char *);

namespace {
constexpr size_t explosion_frames = 42716;
constexpr size_t source_rate = 22050;
constexpr size_t max_rate = 48000;
Uint8 explosion_source[explosion_frames];
Sint16 explosion_pcm[((explosion_frames * max_rate + source_rate - 1) / source_rate) * 2];

Mix_Chunk *load_explosion_pcm(const char *filename) {
    int rate = 0, channels = 0;
    Uint16 format = 0;
    const int opened = Mix_QuerySpec(&rate, &format, &channels);
    char spec[96];
    std::snprintf(spec, sizeof(spec), "open=%d rate=%d format=0x%04x channels=%d",
                  opened, rate, static_cast<unsigned>(format), channels);
    ps5_startup_log("explosion mixer spec", spec);
    if (!opened || rate <= 0 || rate > static_cast<int>(max_rate) ||
        format != AUDIO_S16SYS || (channels != 1 && channels != 2))
        return nullptr;

    std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(filename, "rb"), std::fclose);
    if (!file) {
        ps5_startup_log("explosion PCM", "source open failed");
        return nullptr;
    }
    // Exact vendored WAV header: 22050 Hz, unsigned 8-bit mono PCM.
    constexpr unsigned char expected[] = {
        82, 73, 70, 70, 0, 167, 0, 0, 87, 65, 86, 69, 102, 109, 116, 32, 16, 0, 0, 0, 1, 0, 1, 0, 34, 86, 0, 0, 34, 86, 0, 0, 1, 0, 8, 0, 100, 97, 116, 97, 220, 166, 0, 0
    };
    unsigned char header[sizeof(expected)];
    if (std::fread(header, 1, sizeof(header), file.get()) != sizeof(header) ||
        std::memcmp(header, expected, sizeof(header)) != 0) {
        ps5_startup_log("explosion PCM", "unexpected WAV header");
        return nullptr;
    }

    if (std::fread(explosion_source, 1, explosion_frames, file.get()) != explosion_frames) {
        ps5_startup_log("explosion PCM", "short WAV payload");
        return nullptr;
    }
    const size_t frames = (explosion_frames * rate + source_rate - 1) / source_rate;
    for (size_t i = 0; i < frames; ++i) {
        const size_t source = i * source_rate / rate;
        const auto sample = static_cast<Sint16>((static_cast<int>(explosion_source[source]) - 128) * 256);
        for (int channel = 0; channel < channels; ++channel)
            explosion_pcm[i * channels + channel] = sample;
    }
    auto *chunk = Mix_QuickLoad_RAW(reinterpret_cast<Uint8 *>(explosion_pcm),
                                   frames * channels * sizeof(Sint16));
    if (!chunk) ps5_startup_log("explosion PCM chunk failed", Mix_GetError());
    return chunk;
}
}

#endif

namespace cbe
{
  AudioBufferSDL::AudioBufferSDL(std::filesystem::path filename)
  {
#ifdef CLANBOMBER_PS5
    sound_path = filename.filename();
#endif
#ifdef CLANBOMBER_PS5
    chunk = sound_path == "explode.wav" ? load_explosion_pcm(filename.c_str()) : nullptr;
    if (chunk) ps5_startup_log("SFX load", "explode.wav using static PCM");
    if (!chunk)
#endif
      chunk = Mix_LoadWAV(filename.string().c_str());
    if(chunk == NULL) {
#ifdef CLANBOMBER_PS5
      char detail[384];
      std::snprintf(detail, sizeof(detail), "%.160s: %.150s", sound_path.c_str(), Mix_GetError());
      ps5_startup_log("SFX load failed", detail);
#else
      printf("Cannot load sound file");
#endif
      //TODO launch an exception
    }
  }

  AudioBufferSDL::~AudioBufferSDL()
  {
    Mix_FreeChunk(chunk);
  }

  void AudioBufferSDL::play()
  {
#ifdef CLANBOMBER_PS5
    if (!chunk) return;
    const int channel = Mix_PlayChannel(-1, chunk, 0);
    if (!play_reported) {
      char detail[384];
      std::snprintf(detail, sizeof(detail), "%.160s channel=%d %.130s",
                    sound_path.c_str(), channel, channel < 0 ? Mix_GetError() : "");
      ps5_startup_log("SFX first play", detail);
      play_reported = true;
    }
#else
    Mix_PlayChannel(-1, chunk, 0);
#endif
  }
};
