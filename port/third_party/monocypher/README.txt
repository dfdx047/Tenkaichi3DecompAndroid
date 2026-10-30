Monocypher 4.0.2, https://monocypher.org (download: https://monocypher.org/download/monocypher-4.0.2.tar.gz,
SHA-256 of the archive 38d07179738c0c90677dba3ceb7a7b8496bcfea758ba1a53e803fed30ae0879c).
src/monocypher.c, .h and src/optional/monocypher-ed25519.c, .h as they are; LICENCE.md (BSD-2-Clause or CC0, at
the user's choice). Used by the game for the automatic update's signature check (port/src/gs/update_crypto.c,
update_monocypher.c): Ed25519 as RFC 8032 has it.
