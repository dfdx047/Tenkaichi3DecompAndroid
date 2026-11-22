/* The few SDL3 calls port/src/gs/tag_link.c makes (time, mutexes, atomics, threads, string helpers), on POSIX: for this
   harness only, so it builds on a Linux machine without a desktop SDL3 (run.sh). The game is never built with it.
   tag_link.c's own logic is not touched by it; with a real SDL3 the harness builds as before (-I<SDL3 include>). */
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>
typedef uint64_t Uint64; typedef uint32_t Uint32; typedef int64_t Sint64;
#define SDLCALL
static inline Uint64 SDL_GetTicksNS(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (Uint64)t.tv_sec*1000000000ull+(Uint64)t.tv_nsec;}
static inline Uint64 SDL_GetTicks(void){return SDL_GetTicksNS()/1000000ull;}
static inline void SDL_Delay(Uint32 ms){usleep(ms*1000u);}
static inline int SDL_Init(unsigned f){(void)f;return 1;}
typedef pthread_mutex_t SDL_Mutex;
static inline SDL_Mutex *SDL_CreateMutex(void){SDL_Mutex *m=malloc(sizeof *m);pthread_mutex_init(m,NULL);return m;}
static inline void SDL_LockMutex(SDL_Mutex *m){if(m)pthread_mutex_lock(m);}
static inline void SDL_UnlockMutex(SDL_Mutex *m){if(m)pthread_mutex_unlock(m);}
typedef struct {atomic_int v;} SDL_AtomicInt;
static inline int SDL_GetAtomicInt(SDL_AtomicInt *a){return atomic_load(&a->v);}
static inline int SDL_SetAtomicInt(SDL_AtomicInt *a,int v){return atomic_exchange(&a->v,v);}
static inline int SDL_AddAtomicInt(SDL_AtomicInt *a,int v){return atomic_fetch_add(&a->v,v);}
typedef struct {pthread_t t;} SDL_Thread;
typedef int (*SDL_ThreadFunction)(void*);
struct sdl_tstart {SDL_ThreadFunction f;void *a;};
static void *sdl_trun(void *p){struct sdl_tstart s=*(struct sdl_tstart*)p;free(p);s.f(s.a);return NULL;}
static inline SDL_Thread *SDL_CreateThread(SDL_ThreadFunction f,const char *n,void *a){(void)n;SDL_Thread *t=malloc(sizeof *t);struct sdl_tstart *s=malloc(sizeof *s);s->f=f;s->a=a;pthread_create(&t->t,NULL,sdl_trun,s);return t;}
static inline void SDL_DetachThread(SDL_Thread *t){pthread_detach(t->t);free(t);}
static inline unsigned long SDL_GetCurrentThreadID(void){return (unsigned long)pthread_self();}
static inline Uint32 SDL_rand_bits(void){static int s;if(!s){s=1;srand((unsigned)SDL_GetTicksNS()^(unsigned)getpid());}return ((Uint32)rand()<<16)^(Uint32)rand();}
#define SDL_atoi atoi
#define SDL_getenv getenv
#define SDL_memcpy memcpy
#define SDL_memmove memmove
#define SDL_memcmp memcmp
#define SDL_memset memset
#define SDL_snprintf snprintf
#define SDL_strchr strchr
#define SDL_strrchr strrchr
#define SDL_strstr strstr
#define SDL_strcmp strcmp
#define SDL_strncmp strncmp
#define SDL_strlen strlen
static inline size_t SDL_strlcpy(char *d,const char *s,size_t n){size_t l=strlen(s);if(n){size_t c=l<n-1?l:n-1;memcpy(d,s,c);d[c]=0;}return l;}
static inline size_t SDL_strlcat(char *d,const char *s,size_t n){size_t l=strlen(d);if(l<n)SDL_strlcpy(d+l,s,n-l);return l+strlen(s);}
#define SDL_max(a,b) ((a)>(b)?(a):(b))
#define SDL_min(a,b) ((a)<(b)?(a):(b))
#define SDL_clamp(x,a,b) ((x)<(a)?(a):(x)>(b)?(b):(x))
