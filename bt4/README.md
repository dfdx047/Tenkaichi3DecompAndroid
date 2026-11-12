<div align="center">
  <img src="docs/presentation/bt4-intro-hq.gif" width="800" alt="Dragon Ball Z Budokai Tenkaichi 4 Tag Team opening animation">
  <h1>BT4decomp</h1>
  <p><strong>Budokai Tenkaichi 4 B14 Rev2. Native desktop runtime. Simultaneous team battles.</strong></p>
  <p>Online Tag Team, tournaments, spectator rooms and proximity voice, built on Tenkaichi3Decomp.</p>
  <p><strong><a href="https://discord.gg/EBhh5ef3T6">Join the official Discord community</a></strong> for matchmaking, project news and support.</p>
  <p>
    <a href="https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases/latest"><img src="https://img.shields.io/badge/Download-Latest%20release-ef8b24?style=flat-square" alt="Download the latest published release"></a>
    <img src="https://img.shields.io/badge/platforms-Windows%20%7C%20Linux%20%7C%20macOS-1f6feb?style=flat-square" alt="Windows, Linux and macOS">
    <img src="https://img.shields.io/badge/upstream-0.1.39.5-7c3aed?style=flat-square" alt="Tenkaichi3Decomp engine 0.1.39.5">
    <a href="https://ko-fi.com/nautaro"><img src="https://img.shields.io/badge/Ko--fi-Support%20NauTaro-ff5e5b?style=flat-square&amp;logo=ko-fi&amp;logoColor=white" alt="Support NauTaro on Ko-fi"></a>
  </p>
  <p><strong><a href="https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases/latest">Download</a> &nbsp; · &nbsp; <a href="#installation">Install</a> &nbsp; · &nbsp; <a href="docs/README.md">Documentation</a> &nbsp; · &nbsp; <a href="https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/issues/new/choose">Report an issue</a></strong></p>
</div>

BT4decomp adapts **BT4 B14 Rev2** for Windows, Linux and Apple Silicon. It runs the game code natively rather than emulating a console. Installation requires your own compatible Spanish or English BT4 ISO; disc images and extracted game data are not distributed here.

> **Windows security review in progress.** Users reported antivirus alerts for the R5 package. See the [investigation and executable hashes](docs/security/windows-antivirus-review.md).

## Fight together

<img src="docs/presentation/tag-team-battle.jpg" width="960" alt="Simultaneous five-versus-five Tag Team combat with ally health bars, target indicators and radar">

| Mode | What it offers |
| --- | --- |
| **Tag Team** | Simultaneous battles with up to five fighter slots per team, six-player Free for All, room codes and individual targeting. Tag Team Quick Match gathers four players for 2 vs 2; Create Room offers team and Free for All presets. |
| **1 vs 1** | Peer rollback netplay, custom rooms with configurable rollback and input delay, and duel Quick Match. |
| **Tournaments** | Two to eight entrants, automatic byes, a live bracket and match rejoining. |
| **Spectators** | Join without occupying a fighter slot; follow either side or use a split view. |

Game menus support **English and Spanish**, with an optional Brazilian Portuguese pack. Setup supports **English, Spanish and Brazilian Portuguese**. F1 settings support **English, Spanish and Brazilian Portuguese**. **F1 > Spectator** offers free camera, fighter tracking and fight overview, with target, speed, zoom and reset controls. Keyboard and controller camera input replaces local fighter input while the camera is active; Start remains available. Panoramic mode expands menus and HUD across the viewport. Hiding the combat HUD also dismisses the Start pause menu; press Start to open it again.

## Voice and presentation

<table>
  <tr>
    <td width="48%" valign="top"><img src="docs/presentation/voice-chat-settings.png" width="420" alt="Microphone, voice activation, input level and playback volume settings"></td>
    <td valign="top">
      <h3>Proximity voice</h3>
      <p>Talk across both teams, with volume adjusted by fighter distance. Choose microphone and output devices, voice activation, threshold and playback volume.</p>
      <h3>Display controls</h3>
      <p>Windows offers Vulkan, Direct3D 12 and OpenGL. Adjust render scale, aspect ratio, effects and motion presentation independently of combat simulation.</p>
      <h3>Custom content</h3>
      <p>Add compatible stages with matching PNG previews, or install custom music. See the <a href="docs/port/adding_stages.md">stage guide</a> and <a href="docs/port/adding_songs.md">music guide</a>.</p>
    </td>
  </tr>
</table>

## Script mods

F1 > Mods manages the included Modern HUD, Modern Camera, Modern Look and Skill List scripts. Mods start disabled. Each player can choose their own presentation settings; existing scripts and settings are preserved by automatic updates. F1 > Controls offers optional right-stick enemy targeting. F1 > Spectator can keep the normal camera during special attacks. See the [Lua mod guide](docs/port/mods.md). F1 > Tech offers optional mechanics adjustments for offline duels; online matches, Tag Team and replays retain their original rules.

## Installation

| System | Package | Requirement |
| --- | --- | --- |
| Windows | `BT4decomp-windows-x64.zip` | x64 |
| Linux | `BT4decomp-linux-x64.zip` | x86-64, glibc 2.35 or later |
| macOS | `BT4decomp-macos-arm64.zip` | Apple Silicon, macOS 15 or later |

1. Download your platform's ZIP from the [latest published release](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases/latest) and extract it completely.
2. Run `BT4decomp-setup.exe` on Windows, or `./BT4decomp-setup` on Linux and macOS.
3. Select the setup language, game language and your compatible **BT4 B14 Rev2 ISO**.
4. Start `BT4decomp.exe` on Windows, or `./BT4decomp` on Linux and macOS.

<details>
<summary><strong>Language installation and setup preview</strong></summary>

A single-language installation opens directly in that language. Selecting both languages downloads the alternate package from this repository and enables language selection at startup. Public language and HD texture packages download without GitHub sign-in.

<p align="center"><img src="docs/presentation/installer-en.png" width="640" alt="English installer with Spanish, English and bilingual installation options"></p>

For Linux permissions, macOS download handling and source builds, see [native platform setup](docs/port/native_platforms.md). Intel Macs and Linux ARM are not targets of these packages.

</details>

## Automatic updates

Open **F1 > Updates > Check for updates** to look for a new version. The game also checks this repository automatically for the latest signed package for its platform. It verifies the archive SHA-256 and Ed25519 signature, then asks for a game restart to apply the update at the offline main menu. Saves, settings, installed game data and custom content remain outside the replacement list. Updating does not restart an active battle or online session.

Installations that predate the signed updater need one initial package installation. Subsequent published signed updates can install through the launcher.

Revision tags follow the upstream engine version, for example `v0.1.39.5-R1`. A new upstream adaptation starts at R1; subsequent BT4 changes increment the revision. Patch revisions are also supported. All Tag Team participants must use the same room protocol and a compatible published build.

## Source and online compatibility

The source integrates **Tenkaichi3Decomp 0.1.39.5**, with BT4 revisions tracked separately as **R1, R2, R3** and onward. The `main` branch can contain changes awaiting release; the Download button points to the published package. See the [changelog](CHANGELOG.md) for source changes and [release notes](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases) for packaged builds.

Use compatible BT4 data and the same published launcher on every participating computer. The selected language does not define the online compatibility family.

<details>
<summary><strong>Networking implementation and validation</strong></summary>

1 vs 1 uses the upstream peer rollback path. Lobby matches can relay through the lobby owner when the assigned fighters cannot connect directly.

Rooms and Quick Match use authenticated private Supabase signaling. Tag Team room authority runs on the owner's computer from admission onward; gameplay and voice travel between participants. The default connection no longer uses Cloudflare coordination or a hosted game/audio relay. STUN assists direct connection discovery. Networks that block peer traffic, including some CGNAT configurations, can still prevent a direct connection. No public relay is bundled.

The Free signaling project has provider quotas, including concurrent Realtime connection limits. P2P reduces server traffic; it does not provide unlimited hosting capacity. R6 was validated with local native multi-client combat, tournaments and synthetic voice; this is not a cross-ISP or load-capacity benchmark.

The online hub shows an authenticated aggregate of players in the online menus, refreshed every five minutes. Entries expire after a missed refresh. The active route supplies measured ping. A single human fighting CPU uses local input scheduling. If the native coordinator leaves, the round ends; live coordinator migration is not implemented. Geographic routes and network conditions still affect latency.

Voice and gameplay have separate queues. Native packages include the RTC SDK. SDP and ICE negotiation use private Supabase inboxes; Opus audio travels over peer media tracks. The default provider does not relay voice through WebSockets.

See the [compatibility and validation profile](BT4_PROFILE.md), [signaling architecture](supabase/README.md) and [native SDK build notes](port/release/RTC_SDK.md). Local tests and package builds do not establish performance on every remote PC, router or graphics driver.

</details>

## Development and feedback

Start with the [contribution guide](CONTRIBUTING.md) and [documentation index](docs/README.md). Bug reports should include `BT4_VERSION.txt`, system specifications, game mode, renderer, resolution scale, reproduction steps and relevant excerpts from `bt3_log.txt` or `bt3_crash.txt`.

For combat issues, include characters, costumes, stage and player count. Remove credentials and personal information before attaching logs. [Open a bug report](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/issues/new/choose).

## Support development

Support work on BT4decomp through [NauTaro's Ko-fi](https://ko-fi.com/nautaro).

<a href="https://ko-fi.com/nautaro"><img src="https://img.shields.io/badge/Support%20BT4decomp-Ko--fi-ff5e5b?style=for-the-badge&amp;logo=ko-fi&amp;logoColor=white" alt="Support BT4decomp on Ko-fi"></a>

## Credits and licenses

The original decompilation and native runtime are developed by the [Tenkaichi3Decomp contributors](https://github.com/z3xox/Tenkaichi3Decomp). BT4decomp adapts that work for BT4 B14 Rev2. [UPSTREAM_LOCK.json](UPSTREAM_LOCK.json) records the exact source revision; [README_UPSTREAM.md](README_UPSTREAM.md) preserves the upstream reference.

BT4 content, artwork and icons retain their respective authors' rights. See [presentation credits](docs/presentation/CREDITS.md), [AGPL](LICENSE), [MIT](LICENSE-MIT) and the bundled third-party notices.
