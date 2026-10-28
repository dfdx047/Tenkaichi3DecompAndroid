/* Checks of the automatic update's pieces (port/src/gs/update_sign.c, update_crypto.c): release numbers, SHA-256,
   the Ed25519 check. Build and run: sh port/tests/update/run.sh */
#include <stdio.h>
#include <string.h>
#include "../../src/gs/update_sign.h"

int UpdCrypto_Check(const uint8_t sig[64], const uint8_t pub[32], const uint8_t *msg, size_t size);
static int sFailed, sCount;

static void check(int ok, const char *what) {
    sCount++;
    if (!ok) {
        sFailed++;
        printf("FAILED: %s\n", what);
    }
}

int main(void) {
    static const struct { const char *a, *b; int want; } cmp[] = {
        {"0.1.38.7", "0.1.38.9", -1}, {"0.1.38.9", "0.1.38.10", -1}, {"0.1.38.10", "0.1.39", -1}, {"0.1.38.7", "0.1.39", -1},
        {"0.1.38", "0.1.38.1", -1}, {"0.1.38.9", "v0.1.38.9", 0}, {"0.1.38", "0.1.38.0", 0}, {"0.1.39", "0.1.38.10", 1},
        {"0.2", "0.1.99.99", 1}, {"1.0.0.0", "0.9.9.9", 1}, {"0.1.38.10", "0.1.38.9", 1},
        {"dev", "0.1.38.10", -2}, {"0.1.38.9", "dev", -2}, {"dev", "dev", -2}, {"", "0.1", -2},
        {"0.1.39-rc1", "0.1.38", -2}, {"0.1.38.9.1", "0.1.38.9", -2}, {"0.1..38", "0.1.38", -2}, {"0", "0.1", -2},
        {"0.1.38.", "0.1.38", -2}, {"V0.1.38.9", "0.1.38.8", 1},
        /* 0.1.39 (lobbies replace rooms): its tags are newer than the last 0.1.38 everywhere */
        {"0.1.38.9", "v0.1.39", -1}, {"0.1.38.9", "v0.1.39.1", -1}, {"0.1.38.10", "v0.1.39", -1}, {"0.1.39", "v0.1.39.1", -1},
        {"0.1.39", "v0.1.39", 0}, {"0.1.39.0", "v0.1.39", 0}, {"0.1.39.1", "v0.1.39", 1}, {"0.1.39.1", "0.1.38.9", 1},
        {"0.1.39.10", "v0.1.39.9", 1}, {"0.1.39.1", "v0.1.40", -1},
    };
    for (size_t i = 0; i < sizeof(cmp) / sizeof(cmp[0]); i++) {
        char what[96];
        snprintf(what, sizeof(what), "compare %s %s = %d", cmp[i].a, cmp[i].b, cmp[i].want);
        check(UpdateVer_Compare(cmp[i].a, cmp[i].b) == cmp[i].want, what);
    }
    /* SHA-256 (FIPS 180-4 examples) */
    {
        char hex[65];
        static char million[1000000];
        UpdSha256_Hex("", 0, hex);
        check(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0, "sha256 empty");
        UpdSha256_Hex("abc", 3, hex);
        check(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0, "sha256 abc");
        UpdSha256_Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, hex);
        check(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0, "sha256 56 bytes");
        memset(million, 'a', sizeof(million));
        UpdSha256_Hex(million, sizeof(million), hex);
        check(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0, "sha256 a million a");
        {   /* the same in pieces of odd sizes */
            UpdSha256 s;
            uint8_t d[32];
            size_t at = 0, step = 1;
            UpdSha256_Init(&s);
            while (at < sizeof(million)) {
                size_t n = step < sizeof(million) - at ? step : sizeof(million) - at;
                UpdSha256_Add(&s, million + at, n);
                at += n;
                step = step * 3 % 1000 + 1;
            }
            UpdSha256_End(&s, d);
            check(d[0] == 0xcd && d[31] == 0xd0, "sha256 in pieces");
        }
    }
    /* Ed25519: RFC 8032, section 7.1, test 2 (one byte, 0x72) */
    {
        uint8_t pub[32], sig[64], msg = 0x72;
        check(Upd_HexBytes("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", pub, 32), "hex public");
        check(Upd_HexBytes("92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00\n", sig, 64), "hex signature");
        check(UpdCrypto_Check(sig, pub, &msg, 1), "ed25519 RFC 8032 test 2");
        msg = 0x73;
        check(!UpdCrypto_Check(sig, pub, &msg, 1), "ed25519 another message refused");
        msg = 0x72;
        sig[63] ^= 1;
        check(!UpdCrypto_Check(sig, pub, &msg, 1), "ed25519 a changed signature refused");
        sig[63] ^= 1;
        pub[0] ^= 1;
        check(!UpdCrypto_Check(sig, pub, &msg, 1), "ed25519 another key refused");
        check(!Upd_HexBytes("3d40", pub, 32), "hex too short");
        check(!Upd_HexBytes("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c0", pub, 32), "hex too long");
    }
    printf("%d of %d checks pass\n", sCount - sFailed, sCount);
    return sFailed != 0;
}
