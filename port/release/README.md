# Making a release

`.github/workflows/release.yml` builds both archives for every push to `main` (as artifacts of the run) and, for a
tag `v*`, a **draft** release with them attached; publishing the draft makes it the one games see. The same build
runs locally with `port/release/build.sh` (docker). The tag is the release's full number: `v0.1.38.10` builds
programs that call themselves `0.1.38.10` (`BT3_VERSION`); the games compare that with the newest release's tag to
offer updates. Raise `PORT_RELEASE` (port/src/port_release.h) when the matchmaking service must tell versions apart.

When a release changes what goes over the line (`NET_VERSION`, port/src/gs/net.c), older games cannot play it
anywhere: the service keeps its queues and codes apart by that number, and by address each side says "another
version". After publishing such a release, set the Worker's variable `MIN_RELEASE` to its `PORT_RELEASE` (Cloudflare
dashboard > the Worker > Settings > Variables), so that older games are told to update instead of finding nobody;
if the Worker checks build keys (`BUILD_KEYS`), the new release's key must be in it first. 0.1.39 is such a release
(`PORT_RELEASE` 139, `NET_VERSION` 6: lobbies replace the rooms of two): `MIN_RELEASE` 139 once it is out.

Two repository secrets (Settings > Secrets and variables > Actions):

| secret | what | without it |
|---|---|---|
| `BT3_BUILD_KEY` | the build key the programs sign their matchmaking requests with (port/matchmaking/README.md) | the programs sign nothing |
| `UPDATE_SIGNING_KEY` | the private key the archives are signed with for the automatic update | the release is unsigned: games announce it but do not install it themselves (the workflow log says so) |

## The update signing key (once)

The game installs an update only when the zip's signature (`<zip>.sig`, attached by the workflow) is right for the
public key compiled into it. Until the key below is set, `port/src/update_key.h` holds a placeholder of zeros and no
game installs anything by itself.

1. On your own computer, in the repository:

       python3 port/tools/update_key.py generate

   It writes the **private** key to `update_signing_key.txt` (not shown on the screen; the file is readable by you
   only; `--out <file>` for another place) and prints the **public** key as a line for the source.
2. On GitHub: Settings > Secrets and variables > Actions > New repository secret. Name `UPDATE_SIGNING_KEY`; value:
   the one line of `update_signing_key.txt`. Then delete that file, or keep it offline. Whoever has it can make
   updates that every game installs. Never commit it.
3. In `port/src/update_key.h`, replace the zeros of `PORT_UPDATE_KEY_RELEASE` with the printed public key, and commit.
   (`python3 port/tools/update_key.py public --key-file update_signing_key.txt` prints it again.)
4. Tag the next release. The workflow signs both zips (`update_key.py sign`), checks the signatures, and attaches
   `Tenkaichi3Decomp-linux-x64.zip.sig` and `Tenkaichi3Decomp-windows-x64.zip.sig` to the draft. If the secret does
   not match the key in update_key.h the job stops: no game could check those signatures.

Games built before step 3 cannot update themselves (they have no key): their players update by hand once, to the
first release that has the key; from then on the games update themselves. A new key later works the same way, and
again only for games built with it. A fork that publishes its own releases makes its own key and names its own
repository in `PORT_UPDATE_REPO`.

Checking a release by hand: `python3 port/tools/update_key.py verify Tenkaichi3Decomp-linux-x64.zip` (with the .sig
beside it) says `signature good`.

## Credits

The in-game updater (download, progress bar, unpacking with miniz, the rename trick that replaces a running program,
the restart) is based on PR #107 by JosephLlacchua; the signature check, the version from the tag, the player's
choice of when, the protection of the player's files and the undoing of a failed install were added on top of it.
How it works: docs/port/README.md, "Automatic updates".
