# Documentation

## Installation and customization

For installation, updates and language selection, start with the
[project README](../README.md).

| Guide | Scope |
| --- | --- |
| [Custom stages](port/adding_stages.md) | Map installation, thumbnails and display names |
| [Native platforms](port/native_platforms.md) | Windows, Linux and Apple Silicon installation and requirements |
| [Custom music](port/adding_songs.md) | Supported formats, track names and importing |
| [Texture packs](port/texture_packs.md) | ZIP packs, PNG/DDS files and renderer settings |
| [Menu fonts](port/menu_font.md) | Font configuration and text rendering |
| [Translations](port/translations.md) | Text catalogs and translation loading |

## Development

| Guide | Scope |
| --- | --- |
| [PC port](port/README.md) | Runtime architecture and build requirements |
| [Build tools](../tools/review/README.md) | Native builds, installer builds and layout checks |
| [BT4 profile](../BT4_PROFILE.md) | Supported game data, languages and Tag Team protocol |
| [Tag Team services](../cloudflare/README.md) | Room kernel, voice signaling and deployment |
| [Two-player matchmaking](../port/matchmaking/README.md) | Room codes, quick match and relay configuration |
| [Menu implementation](port/menus.md) | Native menu layout and integration |

## Original game reference

The following guides document the original BT3 executable and decompilation.
Use the BT4 profile and port source when checking behavior added by this project.

- [Decompilation guide](decomp_guide.md)
- [Game code overview](game_overview.md)
- [Executable address map](text_map.md)
- [Deterministic netplay analysis](netplay_notes.md)
- [Original code bugs and quirks](known_bugs.md)
- [Subsystem references](systems/)
- [VU1 microprograms](systems/vu1/README.md)

Technical references distinguish verified findings from inferred behavior.

## Attribution

See the [presentation credits](presentation/CREDITS.md),
[AGPL license](../LICENSE) and [MIT license](../LICENSE-MIT).

- [Lua script mods](port/mods.md): installation, settings and the presentation API.
