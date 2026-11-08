cmake_minimum_required(VERSION 3.20)
project(ClanBomber2PS5 LANGUAGES CXX)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)

file(GLOB game_sources CONFIGURE_DEPENDS upstream/src/*.cpp)
add_executable(clanbomber2 ${game_sources} src/ps5_main.cpp)
set_target_properties(clanbomber2 PROPERTIES SUFFIX ".elf")
target_compile_features(clanbomber2 PRIVATE cxx_std_17)
target_include_directories(clanbomber2 PRIVATE src upstream/src)
target_compile_definitions(clanbomber2 PRIVATE
    CLANBOMBER_PS5
    PACKAGE="clanbomber2"
    PACKAGE_VERSION="2.3.0-ps5"
    CB_DATADIR="ClanBomber2"
    CB_LOCALEDIR="ClanBomber2/locale")
target_link_libraries(clanbomber2 PRIVATE SDL2::SDL2main SDL2_image::SDL2_image-static
    SDL2_mixer::SDL2_mixer-static SDL2_ttf::SDL2_ttf-static SDL2::SDL2-static)

install(TARGETS clanbomber2 RUNTIME DESTINATION build-input)
install(DIRECTORY upstream/src/fonts upstream/src/maps upstream/src/pics upstream/src/wavs
    DESTINATION ClanBomber2 PATTERN "Makefile.am" EXCLUDE)
install(DIRECTORY characters DESTINATION .)
install(FILES LICENSE upstream/LICENSE.DEJAVU ASSET-PROVENANCE.md DESTINATION .)
