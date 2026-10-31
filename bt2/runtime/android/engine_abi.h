// What libbt2.so and Dragon Rage's app expect of each other: android_main.cpp's SDL_main arguments and the JNI
// functions it exports (TouchBridge, Bt2Bridge). A motor package (bt2/android/build.py package) carries this number
// in its manifest and the app (Bt2Engine, BuildConfig.BT2_ENGINE_ABI, read from this file) imports only a package
// with its own. Raise it whenever either side of that line changes.
#pragma once

#define BT2_ENGINE_ABI 1
