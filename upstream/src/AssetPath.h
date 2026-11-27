#pragma once

#if defined(CLANBOMBER_DESKTOP)
#include <string>
std::string desktop_app_path(const char *relative);
#define CB_ASSET(suffix) desktop_app_path(CB_DATADIR suffix).c_str()
#elif defined(CLANBOMBER_PS5)
#include <string>
std::string ps5_app_path(const char *relative);
#define CB_ASSET(suffix) ps5_app_path(CB_DATADIR suffix).c_str()
#else
#define CB_ASSET(suffix) CB_DATADIR suffix
#endif
