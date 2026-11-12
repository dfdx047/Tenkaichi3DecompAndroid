/*
 * Lua 5.4 (port/third_party/lua, MIT licence: port/third_party/lua/LICENSE) in one object, for the script mods
 * (gs/mods.c). The order is the one of Lua's own one-file build (onelua.c). Only the core and the libraries a script
 * needs without reaching outside the game are compiled: no io, os, package or debug library, so a script cannot
 * open a file, run a program or load a native library.
 */
#define LUA_CORE /* (the core and the libraries: their private macros, l_likely and the like) */
#define LUA_LIB
#include "../../third_party/lua/lprefix.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../third_party/lua/luaconf.h"

#undef LUAI_FUNC /* (one object: the core's own functions are private to it) */
#undef LUAI_DDEC
#undef LUAI_DDEF
#define LUAI_FUNC static
#define LUAI_DDEC(dec) static dec
#define LUAI_DDEF static

#include "../../third_party/lua/lzio.c"
#include "../../third_party/lua/lctype.c"
#include "../../third_party/lua/lopcodes.c"
#include "../../third_party/lua/lmem.c"
#include "../../third_party/lua/lundump.c"
#include "../../third_party/lua/ldump.c"
#include "../../third_party/lua/lstate.c"
#include "../../third_party/lua/lgc.c"
#include "../../third_party/lua/llex.c"
#include "../../third_party/lua/lcode.c"
#include "../../third_party/lua/lparser.c"
#include "../../third_party/lua/ldebug.c"
#include "../../third_party/lua/lfunc.c"
#include "../../third_party/lua/lobject.c"
#include "../../third_party/lua/ltm.c"
#include "../../third_party/lua/lstring.c"
#include "../../third_party/lua/ltable.c"
#include "../../third_party/lua/ldo.c"
#include "../../third_party/lua/lvm.c"
#include "../../third_party/lua/lapi.c"

#include "../../third_party/lua/lauxlib.c"

#include "../../third_party/lua/lbaselib.c"
#include "../../third_party/lua/lcorolib.c"
#include "../../third_party/lua/lmathlib.c"
#include "../../third_party/lua/lstrlib.c"
#include "../../third_party/lua/ltablib.c"
#include "../../third_party/lua/lutf8lib.c"
