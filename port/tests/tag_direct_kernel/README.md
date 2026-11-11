# Native/WASM authority kernel parity

Run `python port/tests/tag_direct_kernel/run.py --output <disposable-folder>`
from the repository. A native C compiler and Node.js are required. Set `CC` to
the compiler executable when it is not named `cc` or `gcc` on PATH. No ISO,
installed client, graphics backend, audio device or network socket is used.

The native fixture links the production `tag_team_authority.c` wrapper and room
model. Node loads the shipped `cloudflare/kernel.wasm`. Both execute the same
binary event stream and produce command-state traces plus complete 512-byte
wire packets. Those outputs are compared byte for byte.

Each of seven roster configurations runs 2,000 authoritative frames: 2 humans
with 0, 2 or 8 CPU fighters; 4 humans with 0, 2 or 6 CPU fighters; and 10 humans.
Fixtures cover missing, duplicate, conflicting, reordered, late and out-of-window
input; a three-frame held-input gap followed by neutral input; one-shot command
clearing; queued input followed by disconnect; and rejection of later input from
the disconnected seat. Invalid import cases cover running snapshots, protocol,
result, slot count, host, actor flags, load digest and fewer than two humans.
Rejected imports must leave the current kernel state untouched.

`RESULTS.json` records exact production source and WASM digests, per-case byte
counts and hashes, and the first differing byte with its event when parity
fails. Additional assertions check neutral CPU/disconnected controls and frame
ordering independently of parity. The test does not exercise RTC/WSS routing,
real-time host scheduling, battle physics or Internet latency.
