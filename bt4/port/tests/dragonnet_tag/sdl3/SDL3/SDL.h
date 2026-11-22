/* The SDL3 calls gs/dragonnet_tag.cpp makes, on POSIX: for this test only (run.sh); the game is never built with it. */
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
typedef uint64_t Uint64; typedef uint32_t Uint32; typedef int32_t Sint32;
static inline Uint64 SDL_GetTicks(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (Uint64)t.tv_sec * 1000 + (Uint64)t.tv_nsec / 1000000; }
static inline int SDL_atoi(const char *s) { return atoi(s); }
static inline const char *SDL_getenv(const char *n) { return getenv(n); }
static inline const char *SDL_GetBasePath(void) { return NULL; }
typedef struct SDL_SharedObject SDL_SharedObject;
static inline SDL_SharedObject *SDL_LoadObject(const char *n) { (void)n; return NULL; }
static inline void *SDL_LoadFunction(SDL_SharedObject *o, const char *n) { (void)o; (void)n; return NULL; }
typedef struct SDL_Semaphore { pthread_mutex_t m; pthread_cond_t c; unsigned n; } SDL_Semaphore;
static inline SDL_Semaphore *SDL_CreateSemaphore(Uint32 n) { SDL_Semaphore *s = (SDL_Semaphore *)calloc(1, sizeof(*s)); pthread_mutex_init(&s->m, NULL); pthread_cond_init(&s->c, NULL); s->n = n; return s; }
static inline void SDL_DestroySemaphore(SDL_Semaphore *s) { if (s) { pthread_mutex_destroy(&s->m); pthread_cond_destroy(&s->c); free(s); } }
static inline void SDL_SignalSemaphore(SDL_Semaphore *s) { pthread_mutex_lock(&s->m); s->n++; pthread_cond_signal(&s->c); pthread_mutex_unlock(&s->m); }
static inline bool SDL_WaitSemaphoreTimeout(SDL_Semaphore *s, Sint32 ms) {
    struct timespec t; bool got; clock_gettime(CLOCK_REALTIME, &t); t.tv_sec += ms / 1000; t.tv_nsec += (long)(ms % 1000) * 1000000; if (t.tv_nsec >= 1000000000) { t.tv_sec++; t.tv_nsec -= 1000000000; }
    pthread_mutex_lock(&s->m); while (!s->n) if (pthread_cond_timedwait(&s->c, &s->m, &t) == ETIMEDOUT) break; got = s->n > 0; if (got) s->n--; pthread_mutex_unlock(&s->m); return got;
}
typedef struct SDL_Thread { pthread_t t; } SDL_Thread;
typedef int (*SDL_ThreadFunction)(void *);
struct SDL_ThreadStart { SDL_ThreadFunction f; void *a; };
static void *SDL_ThreadRun(void *p) { SDL_ThreadStart s = *(SDL_ThreadStart *)p; free(p); s.f(s.a); return NULL; }
static inline SDL_Thread *SDL_CreateThread(SDL_ThreadFunction f, const char *name, void *a) { (void)name; SDL_Thread *t = (SDL_Thread *)malloc(sizeof(*t)); SDL_ThreadStart *s = (SDL_ThreadStart *)malloc(sizeof(*s)); s->f = f; s->a = a; pthread_create(&t->t, NULL, SDL_ThreadRun, s); return t; }
