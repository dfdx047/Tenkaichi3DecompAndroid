#ifndef PORT_VARIANT_H
#define PORT_VARIANT_H

/* Which game the PC build runs: gPortVariant 0 = the USA release, 1 = the Budokai Tenkaichi 4 mod (SLUS_219.78), which
   renumbers many file ids and sizes (docs/port/bt4.md). PORT_V(usa, bt4) picks a value; outside the PC build it is the
   USA value, so the matching build is unchanged. */
#ifdef PORT
extern int gPortVariant; /* port/src/plat_mem.c */
#define PORT_V(usa, bt4) (gPortVariant ? (bt4) : (usa))
#else
#define PORT_V(usa, bt4) (usa)
#endif

#endif
