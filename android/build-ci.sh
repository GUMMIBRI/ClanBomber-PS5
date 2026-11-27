#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
deps="$root/android-deps"
project="$root/android-generated"
mkdir -p "$deps"
fetch() {
  local name="$1" url="$2" hash="$3"
  curl --fail --location --retry 3 "$url" -o "$deps/$name.tar.gz"
  echo "$hash  $deps/$name.tar.gz" | sha256sum --check
  mkdir -p "$deps/$name"
  tar -xzf "$deps/$name.tar.gz" -C "$deps/$name" --strip-components=1
}
fetch SDL2 https://github.com/libsdl-org/SDL/releases/download/release-2.30.12/SDL2-2.30.12.tar.gz ac356ea55e8b9dd0b2d1fa27da40ef7e238267ccf9324704850d5d47375b48ea
fetch SDL2_image https://github.com/libsdl-org/SDL_image/releases/download/release-2.8.2/SDL2_image-2.8.2.tar.gz 8f486bbfbcf8464dd58c9e5d93394ab0255ce68b51c5a966a918244820a76ddc
fetch SDL2_mixer https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.8.0/SDL2_mixer-2.8.0.tar.gz 1cfb34c87b26dbdbc7afd68c4f545c0116ab5f90bbfecc5aebe2a9cb4bb31549
fetch SDL2_ttf https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz d48cbd1ce475b9e178206bf3b72d56b66d84d44f64ac05803328396234d67723
git clone -q https://github.com/libsdl-org/libxmp.git "$deps/SDL2_mixer/external/libxmp"
git -C "$deps/SDL2_mixer/external/libxmp" checkout -q f59573ed254d2c5a5b02bac2d2b09a4496d5d6fc
cp -r "$deps/SDL2/android-project" "$project"
cp "$root/android/app.gradle" "$project/app/build.gradle"
cp "$root/android/AndroidManifest.xml" "$project/app/src/main/AndroidManifest.xml"
mkdir -p "$project/app/src/main/java/org/clanbomber/game" "$project/app/src/main/assets/ClanBomber2"
cp "$root/android/ClanBomberActivity.java" "$project/app/src/main/java/org/clanbomber/game/"
cp -r "$root/upstream/src/"{fonts,maps,pics,wavs} "$project/app/src/main/assets/ClanBomber2/"
find "$project/app/src/main/assets/ClanBomber2" -name Makefile.am -delete
export CLAN_SOURCE_ROOT="$root" CLAN_DEPS_ROOT="$deps"
cd "$project"
chmod +x gradlew
./gradlew --no-daemon --stacktrace assembleDebug
