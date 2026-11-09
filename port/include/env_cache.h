/* getenv with each call site remembering its answer, as in the renderer (port/src/gs/gs_internal.h): the names are
   literals and the BT3_TAG_* switches do not change while the game runs. The Tag Team mod asks for its switches in
   code that runs for every fighter in every frame (port/src/tagteam_trace.c, port/src/gs/tag_net.c); a real getenv
   there is a walk through the whole environment each time. Include after the system headers. */
#ifndef PORT_ENV_CACHE_H
#define PORT_ENV_CACHE_H
#include <stdlib.h>
const char *Port_GetEnv(const char *name); /* gs_core.c */
#undef getenv
#define getenv(name) (__extension__({ static const char *v_; static int k_; if (!k_) { v_ = Port_GetEnv(name); k_ = 1; } v_; }))
#endif
