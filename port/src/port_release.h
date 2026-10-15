/* The number of the release this source is: 0.1.29 is 129 (minor * 100 + patch; a 0.2.0 would be 200). Raised by
   hand in the commit a release is tagged on. The matchmaking service is told it (gs/net_match.c) and can refuse
   releases older than the one its keeper names (port/matchmaking: the variable MIN_RELEASE). */
#ifndef PORT_RELEASE_H
#define PORT_RELEASE_H
#define PORT_RELEASE 135
/* Where releases are published: the game asks GitHub for the newest one's number once per start (gs/net_match.c,
   "Check for updates") and opens this repository's release page from the settings window. A fork that publishes
   its own releases names its own repository here. */
#define PORT_UPDATE_REPO "z3xox/Tenkaichi3Decomp"
#endif
