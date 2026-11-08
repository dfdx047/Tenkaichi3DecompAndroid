/* The PUBLIC half of the key that release archives are signed with, for the automatic update (gs/net_update.c).
 *
 * A release's workflow (.github/workflows/release.yml) signs each zip with the PRIVATE half, kept in the repository
 * secret UPDATE_SIGNING_KEY, and attaches <zip>.sig beside it. The game downloads both and installs the zip only when
 * the signature is right for the key below: someone who could change the release's files on their way (or a fork's
 * release) cannot make a game install them.
 *
 * Made once by the repository's keeper:   python3 port/tools/update_key.py generate
 * (it writes the private half to a file for the secret and prints the line to put here). See port/release/README.md.
 *
 * ALL ZEROS = NO KEY SET: a program built so does not install updates itself; it says that an update exists and
 * shows the release page, as before. The workflow refuses to sign while this is still zeros.
 *
 * A fork that publishes its own releases makes its own key and puts its public half here.
 */
#ifndef PORT_UPDATE_KEY_H
#define PORT_UPDATE_KEY_H
/* PLACEHOLDER (zeros): REPLACE with the output of `update_key.py generate` before the first signed release. */
#define PORT_UPDATE_KEY_RELEASE "e6d94b546355ddbb34f549df298cb9bcbeda034d29f4b182f77e5a19d177dcad"

/* (Testing only: a developer's build made with BT3_UPDATE_TEST_KEY=<hex> in its environment checks with that key
   instead, so that archives signed with a throw-away key can be tried; port/tools/undefined.py writes it into the
   generated port_version.h. The release container is never given that variable.) */
#ifdef PORT_UPDATE_TEST_KEY
#define PORT_UPDATE_KEY PORT_UPDATE_TEST_KEY
#else
#define PORT_UPDATE_KEY PORT_UPDATE_KEY_RELEASE
#endif
#endif
