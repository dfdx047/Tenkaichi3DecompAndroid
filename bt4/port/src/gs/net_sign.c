/*
 * The build key: an official release signs its requests to the matchmaking service (port/matchmaking/worker.js).
 *
 * What is sent with a request, as three headers (net_match.c), by a program that was built with a key:
 *     X-BT3-Release: 134                  PORT_RELEASE
 *     X-BT3-Time:    1791900000           seconds since 1970, by the service's clock as far as it is known
 *     X-BT3-Sign:    <64 hex digits>      HMAC-SHA256(key, text), where text is these six lines joined by "\n"
 *                                         (no "\n" after the last):
 *         BT3-SIGN-1
 *         POST
 *         /v1/quick                       the path
 *         134                             the release, as in the header
 *         1791900000                      the time, as in the header
 *         <64 hex digits>                 SHA-256 of the request's body, exactly as sent
 * A program built without a key sends none of the three and is in every way the program it was before.
 *
 * Where the key comes from: the build's environment, BT3_BUILD_KEY (port/tools/buildkey.py hands it to this file's
 * compilation and to nothing else). It is in no file of the repository.
 *
 * What the key is not: it is not kept from anyone who looks for it. It is stored scrambled (XOR with a number
 * sequence whose start lies beside it), which only keeps it out of a list of the program's texts (`strings`);
 * someone who takes the program apart has it in an afternoon. That is why every release has its own, and why the
 * service can forget one (see port/matchmaking/README.md). It is not a check against cheating.
 *
 * SHA-256 (FIPS 180-4) and HMAC (RFC 2104) are small textbook versions, as net_relay.c's MD5 and SHA-1 are: the
 * port has no cryptography library.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "net_sign.h"

/* ---- SHA-256 ---- */

typedef struct Sha256 { uint32_t h[8]; uint8_t block[64]; uint64_t len; int fill; } Sha256;

static const uint32_t kSha256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74,
    0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d,
    0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e,
    0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(Sha256 *c) {
    uint32_t w[64], v[8], t1, t2;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = (uint32_t)c->block[i * 4] << 24 | (uint32_t)c->block[i * 4 + 1] << 16 | (uint32_t)c->block[i * 4 + 2] << 8 | c->block[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    memcpy(v, c->h, sizeof(v));
    for (i = 0; i < 64; i++) {
        t1 = v[7] + (ROR(v[4], 6) ^ ROR(v[4], 11) ^ ROR(v[4], 25)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) + kSha256[i] + w[i];
        t2 = (ROR(v[0], 2) ^ ROR(v[0], 13) ^ ROR(v[0], 22)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
        v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
        v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + t2;
    }
    for (i = 0; i < 8; i++) {
        c->h[i] += v[i];
    }
}

static void sha256_init(Sha256 *c) {
    static const uint32_t start[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(c->h, start, sizeof(start));
    c->len = 0;
    c->fill = 0;
}

static void sha256_add(Sha256 *c, const uint8_t *p, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        c->block[c->fill++] = p[i];
        if (c->fill == 64) {
            sha256_block(c);
            c->fill = 0;
        }
    }
    c->len += n;
}

static void sha256_end(Sha256 *c, uint8_t out[32]) {
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80, zero = 0, lenb[8];
    int i;

    sha256_add(c, &pad, 1);
    while (c->fill != 56) {
        sha256_add(c, &zero, 1);
    }
    for (i = 0; i < 8; i++) {
        lenb[i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    sha256_add(c, lenb, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(c->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(c->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(c->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)c->h[i];
    }
}

void Sign_Sha256(const uint8_t *msg, size_t len, uint8_t out[32]) {
    Sha256 c;
    sha256_init(&c);
    sha256_add(&c, msg, len);
    sha256_end(&c, out);
}

void Sign_Hmac256(const uint8_t *key, size_t keyLen, const uint8_t *msg, size_t len, uint8_t out[32]) {
    uint8_t k[64], pad[64], inner[32];
    Sha256 c;
    int i;

    memset(k, 0, sizeof(k));
    if (keyLen > 64) { /* (a key longer than a block is used as its hash) */
        Sign_Sha256(key, keyLen, k);
    } else {
        memcpy(k, key, keyLen);
    }
    for (i = 0; i < 64; i++) { pad[i] = k[i] ^ 0x36; }
    sha256_init(&c);
    sha256_add(&c, pad, 64);
    sha256_add(&c, msg, len);
    sha256_end(&c, inner);
    for (i = 0; i < 64; i++) { pad[i] = k[i] ^ 0x5c; }
    sha256_init(&c);
    sha256_add(&c, pad, 64);
    sha256_add(&c, inner, 32);
    sha256_end(&c, out);
    memset(k, 0, sizeof(k));
    memset(pad, 0, sizeof(pad));
}

static void hex(const uint8_t *p, int n, char *out) {
    static const char digits[] = "0123456789abcdef";
    int i;
    for (i = 0; i < n; i++) {
        out[i * 2] = digits[p[i] >> 4];
        out[i * 2 + 1] = digits[p[i] & 15];
    }
    out[n * 2] = '\0';
}

/* ---- the key ---- */

/* BT3_BUILD_KEY_SEED and BT3_BUILD_KEY_DATA are given on this file's compiler command line by port/tools/buildkey.py
   when the build's environment has BT3_BUILD_KEY: the key's bytes, each XORed with the next byte of the sequence
   below started at the seed. Scrambled, not hidden: see the top of this file. */
#ifdef BT3_BUILD_KEY_DATA
static const uint8_t kKeyData[] = {BT3_BUILD_KEY_DATA};
static volatile uint32_t sKeySeed = BT3_BUILD_KEY_SEED; /* (volatile: the compiler does not work the key out itself) */
#define KEY_LEN ((int)sizeof(kKeyData))
#else
#define KEY_LEN 0
#endif
#define KEY_MAX 128

int Sign_HasKey(void) {
    return KEY_LEN > 0;
}

/* The key in the clear, for the length of one signature. Returns its length. */
static int key_get(uint8_t out[KEY_MAX]) {
#ifdef BT3_BUILD_KEY_DATA
    uint32_t x = sKeySeed;
    int i, n = KEY_LEN > KEY_MAX ? KEY_MAX : KEY_LEN;
    for (i = 0; i < n; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        out[i] = (uint8_t)(kKeyData[i] ^ (x >> 11));
    }
    return n;
#else
    (void)out;
    return 0;
#endif
}

/* ---- a request ---- */

int Sign_Canonical(const char *method, const char *path, int release, long long when, const char *body, char *out, size_t size) {
    uint8_t digest[32];
    char bodyHex[65];
    int n;

    Sign_Sha256((const uint8_t *)body, strlen(body), digest);
    hex(digest, 32, bodyHex);
    n = snprintf(out, size, "BT3-SIGN-1\n%s\n%s\n%d\n%lld\n%s", method, path, release, when, bodyHex);
    return n > 0 && (size_t)n < size ? n : 0;
}

static int sign_with(const uint8_t *key, int keyLen, const char *method, const char *path, int release, long long when, const char *body, char sig[65]) {
    char text[320];
    uint8_t mac[32];
    int n = Sign_Canonical(method, path, release, when, body, text, sizeof(text));

    if (n == 0) {
        return 0;
    }
    Sign_Hmac256(key, (size_t)keyLen, (const uint8_t *)text, (size_t)n, mac);
    hex(mac, 32, sig);
    return 1;
}

int Sign_Request(const char *method, const char *path, int release, long long when, const char *body, char sig[65]) {
    uint8_t key[KEY_MAX];
    int n = key_get(key), ok;

    if (n == 0) {
        return 0;
    }
    ok = sign_with(key, n, method, path, release, when, body, sig);
    memset(key, 0, sizeof(key));
    return ok;
}

/* ---- the test (BT3_SIGN_TEST) ----
   One fixed request. The same one is in port/matchmaking/test_sign.py (worked out with Python's hmac and hashlib)
   and in port/matchmaking/test.mjs (given to the Worker). The dummy key is a test value and no secret; the
   program's own key is never printed, only what it makes of this request, whose time is long past. */
void Sign_SelfTest(void) {
    static const char dummy[] = "dummy-test-key-0123456789-not-a-secret";
    static const char body[] = "{\"v\":4,\"rel\":134,\"name\":\"Test\",\"addrs\":[\"203.0.113.5:7000\"],\"queue\":\"S\",\"who\":\"abcdefghijklmnop\"}";
    char text[320], sig[65], *p;
    uint8_t digest[32];

    Sign_Sha256((const uint8_t *)"abc", 3, digest);
    hex(digest, 32, sig);
    printf("sign: sha256(abc) %s\n", sig);
    Sign_Canonical("POST", "/v1/quick", 134, 1791900000LL, body, text, sizeof(text));
    for (p = text; *p != '\0'; p++) { /* (on one line: the line ends shown as '|') */
        if (*p == '\n') {
            *p = '|';
        }
    }
    printf("sign: text %s\n", text);
    sign_with((const uint8_t *)dummy, (int)sizeof(dummy) - 1, "POST", "/v1/quick", 134, 1791900000LL, body, sig);
    printf("sign: dummy key %s\n", sig);
    if (Sign_Request("POST", "/v1/quick", 134, 1791900000LL, body, sig)) {
        printf("sign: build key yes %s\n", sig);
    } else {
        printf("sign: build key none\n");
    }
}
