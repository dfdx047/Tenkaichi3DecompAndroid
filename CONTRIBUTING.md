# Contributing

## Repository layout

| Path | Responsibility |
| --- | --- |
| `src/`, `include/` | Decompiled game code and headers |
| `port/src/`, `port/include/` | Native runtime, rendering, networking and voice |
| `port/setup/` | Native installer and language installation |
| `port/release/` | Package identity, icons and portable layout |
| `tools/review/` | Build helpers and package layout checks |
| `cloudflare/` | Tag Team room and voice services |
| `port/matchmaking/` | Two-player matchmaking service |
| `docs/` | Architecture and development references |

## Building

Start with the [PC port guide](docs/port/README.md) and the
[build tool reference](tools/review/README.md). They describe the external
toolchain, dependencies and generated inputs. Supply game data from your
own compatible disc image.

For changes to decompiled functions, follow the
[decompilation guide](docs/decomp_guide.md).

## Validation

Run the portable layout checks from the repository root with Python 3.12:

```sh
python -m unittest discover -s tools/review/tests -v
```

Run the server negotiation, recovery and voice lifecycle checks with Node.js:

```sh
node --test cloudflare/tests/tag_link.test.mjs cloudflare/tests/voice_hibernation.test.mjs
```

These checks do not require game data. Test gameplay, networking, audio and
rendering changes in the affected modes; layout checks alone do not cover
runtime behavior.

Record the build, environment, cases tested and results. Explain any effect
on saves, game-data compatibility, deterministic state or network protocols.

## Bug reports

Use the [bug report form](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/issues/new/choose).
Include the contents of `BT4_VERSION.txt`, system specifications and
reproduction steps.

For combat issues, identify the characters, costumes, stage, move and player
count. For voice issues, include the input and output devices. Attach the
relevant log excerpts and remove personal information or credentials.

## Pull requests

Describe the problem, resulting behavior and validation. Keep each change
focused and retain existing credits and license notices.

Do not commit ISOs, extracted game data, player saves, credentials, logs or
build outputs.
