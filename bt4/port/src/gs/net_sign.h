/* The build key (net_sign.c): signing the requests to the matchmaking service. */
#ifndef NET_SIGN_H
#define NET_SIGN_H

#include <stddef.h>
#include <stdint.h>

void Sign_Sha256(const uint8_t *msg, size_t len, uint8_t out[32]);
void Sign_Hmac256(const uint8_t *key, size_t keyLen, const uint8_t *msg, size_t len, uint8_t out[32]);

/* 1 if this program was built with a key (BT3_BUILD_KEY in the build's environment). */
int Sign_HasKey(void);

/* The text that is signed for one request (see net_sign.c); its length, 0 if it does not fit. */
int Sign_Canonical(const char *method, const char *path, int release, long long when, const char *body, char *out, size_t size);

/* The signature of one request with the program's own key, as 64 hex digits. 0 = built without a key. */
int Sign_Request(const char *method, const char *path, int release, long long when, const char *body, char sig[65]);

/* BT3_SIGN_TEST: prints the text and the signature of a fixed request (with a fixed dummy key, and with the
   program's own if it has one), for port/matchmaking/test_sign.py. */
void Sign_SelfTest(void);

#endif
