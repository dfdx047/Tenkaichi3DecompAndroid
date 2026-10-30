/* Pieces of the automatic update (gs/net_update.c) that stand alone and are checked by port/tests/update: release
   numbers compared, and SHA-256. Plain C over SDL's file access; no game state. */
#ifndef PORT_UPDATE_SIGN_H
#define PORT_UPDATE_SIGN_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* A release's number: "0.1.38.9" or "v0.1.38.9", two to four numbers. 1 and the numbers in out (missing ones 0), or
   0 for anything else ("dev", "0.1.39-rc1", ""). */
int UpdateVer_Parse(const char *text, int out[4]);
/* -1: a is older than b, 0 the same, 1 newer; -2: either is not a release's number (a "dev" build: never updated). */
int UpdateVer_Compare(const char *a, const char *b);

typedef struct {
    uint32_t h[8];
    uint64_t bytes;
    uint8_t block[64];
    size_t used;
} UpdSha256;
void UpdSha256_Init(UpdSha256 *s);
void UpdSha256_Add(UpdSha256 *s, const void *data, size_t size);
void UpdSha256_End(UpdSha256 *s, uint8_t out[32]);
/* The SHA-256 of a file as 64 lower-case hex digits; 0 if it cannot be read. */
int UpdSha256_File(const char *path, char hex[65]);
/* The SHA-256 of memory, as hex. */
void UpdSha256_Hex(const void *data, size_t size, char hex[65]);

/* Hex digits to bytes: 1 when `hex` starts with exactly 2*n of them (anything after them that is not a hex digit is
   allowed: a line's end). */
int Upd_HexBytes(const char *hex, uint8_t *out, size_t n);

#ifdef __cplusplus
}
#endif
#endif
