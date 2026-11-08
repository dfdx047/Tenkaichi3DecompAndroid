#pragma once
#include "ee/ee_runtime.h"
#include <functional>
#include <string>
#include <vector>

// The engine's side of the line to Dragon Rage's own library (bt2/plugin/bt2_plugin.h: libbt2online.so, in the APK, built
// without the game's code): the Online Mode item of the main menu, its screens, the opening's music, later the online play.
// Loads it from `library_dir` (the app's native libraries) when there is one and gives it what it asks of the engine; the
// renderer and the app reach it through the Bt2MenuItem_* functions below, which do nothing without it. False: no library
// (or one this engine cannot work with): the menu stays the game's.
bool installPlugin(EeRuntime& runtime, const std::string& library_dir, const std::string& pictures_dir, const std::string& online_dir);

// The field's sound, as the CRI service gives it to the device: the library's music is added here when set (cri_service.cpp).
extern std::function<void(std::vector<std::int16_t>&)> host_music_mix;
