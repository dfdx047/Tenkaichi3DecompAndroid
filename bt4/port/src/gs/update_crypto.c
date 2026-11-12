/*
 * The automatic update's signature check: Ed25519 (RFC 8032) by Monocypher 4.0.2 (port/third_party/monocypher, its
 * licence BSD-2-Clause or CC0: licenses/monocypher.txt in a release), compiled as part of the game: its Ed25519 part
 * here, the rest in update_monocypher.c (the two have static helpers of the same names). Only the check is used; the
 * signing is port/tools/update_key.py's, on GitHub's machines.
 */
#include "../../third_party/monocypher/monocypher-ed25519.c"

/* 1: `sig` is the signature of the `size` bytes at `msg` by the key whose public half is `pub`. */
int UpdCrypto_Check(const uint8_t sig[64], const uint8_t pub[32], const uint8_t *msg, size_t size) {
    return crypto_ed25519_check(sig, pub, msg, size) == 0;
}
