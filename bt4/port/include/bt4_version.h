#ifndef BT4_VERSION_H
#define BT4_VERSION_H
/* Upstream engine versions and BT4 revisions are independent release axes. */
#define BT4_UPSTREAM_VERSION "0.1.39.5"
#define BT4_REVISION 5
#define BT4_REVISION_PATCH 4
#define BT4_DISPLAY_VERSION "0.1.39.5 R5 1.3"
#ifdef BT4_DEVELOPMENT_BUILD
#undef BT4_DISPLAY_VERSION
#define BT4_DISPLAY_VERSION "0.1.39.5 Dev"
#endif
#define BT4_RELEASE_TITLE "0.1.39.5 R5 1.3"
#define BT4_CANONICAL_VERSION "0.1.39.5-R5.4"
#define BT4_LEGACY_BRIDGE_VERSION "0.1.39.5"
#endif
