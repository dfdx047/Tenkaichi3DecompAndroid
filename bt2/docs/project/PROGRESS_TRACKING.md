# Progress tracking

## Where each kind of information belongs

| File | Purpose | Update timing |
| --- | --- | --- |
| `docs/PROG.md` | Verified checkpoint, current blocker, counts, milestones and next gate | After a measured checkpoint or status change |
| Root `README.md` | Exact copy of the marked checkpoint summary | Synchronize whenever the summary changes |
| `docs/progress/ACTIVE_PASS.md` | Current unfinished pass: changes, evidence, remaining checks and resume steps | Start of a pass, meaningful transitions and interruptions |
| `docs/progress/YYYY-MM.md` | Completed pass records, previous checkpoints and detailed evidence | Append after verification; preserve history |
| Subsystem plans | Design, evidence requirements and bounded implementation steps | When the plan or evidence changes |

`PROG.md` is the source of truth for current verified progress. Keep it concise; move completed detail into the monthly log. The active handoff describes only the unfinished pass and is replaced when a new pass starts. Preserve its completed outcome in the monthly log first. README links and other references are relative Markdown links for GitHub.

## Status vocabulary and verification gates

- **Planned:** no implementation claim.
- **Implemented, unverified:** local changes exist; required checks remain. List the exact missing checks in the active handoff.
- **Verified in isolation:** a bounded test passed. Record its scope; this does not establish full startup coverage.
- **Verified in startup:** the full native startup probe executed the behavior, with the applicable capture comparison and build/tests passing.
- **Blocked:** identify the actual boundary and missing evidence, access or design requirement.

Only verified evidence updates verified counters or checks a milestone. Include measurement date, test scope, report location and the measured boundary. Pending changes may be visible beside the checkpoint but must carry an explicit unverified label. Counts of unique entries, call events, provisional map entries and fully restored gameplay functions are different metrics. Do not infer a completion percentage or a first-frame date from function counts.

## Every pass, including an interrupted pass

1. Read `PROG.md` and the active handoff. Record the pass goal and starting checkpoint.
2. Follow the "continue" rule in AGENTS.md: work through the implementation plan's current next step. Update pending changes and remaining gates as work progresses.
3. Before interruption, record enough file paths, evidence and commands to resume. Do not rely on chat history or transient terminal session IDs. A question during implementation does not close the pass.
4. Run required checks; record what passed and what remains. Keep last-known-good counts if the latest changes have not been run successfully.
5. After verification, refresh `PROG.md`, append the monthly outcome and clear or replace the active handoff. Record the source commit/report used to measure results; the documentation commit can follow that measurement.
6. Clean pass-generated residue as AGENTS.md describes: keep intentional build outputs and required diagnostic artifacts, and verify anything else is unused before deleting it.
7. Synchronize and review README, encoding, links and staged files, then commit and push. Do not include unverified implementation in a documentation-only commit.

## README synchronization

Edit the block between `progress-summary` markers in `docs/PROG.md`, then run:

```powershell
python tools/docs/render_status.py
python tools/docs/render_status.py --check
python tools/docs/sync_progress.py
python tools/docs/sync_progress.py --check
```

The tool replaces only the marked README block. It refuses missing or duplicated markers. Run the check before committing progress changes. The summary must remain readable as plain Markdown on GitHub.
## Dashboard measurements

Track useful subsystem outcomes as well as counts: map/generation scope, distinct EE coverage, EE call/return events, IOP instructions/entry targets, module loads, heaps/pools, RPC completion, disc read/lookup completion, card query scope, DMA consumption, GS submissions and actual presentation. Record audio, title/menu, input and battle outcomes when they become observable. Keep unknown values explicit; do not invent performance or completion estimates.

The component map groups Foundation, Runtime and Game scopes. Green marks the stated verified scope, amber marks partial or active work, and gray marks pending work. The map presents statuses without a completion percentage. A subsystem can have verified initialization while its general implementation remains incomplete. Compare run deltas only when their scope and checkpoint are stated; do not compare isolated service tests with full startup counts.
## Machine-readable measurements and visuals

`progress/latest.json` is a sanitized numeric measurement snapshot, with date, scope, code verification state, boundary and SHA-256 of the private source report. `progress/dashboard.svg` is generated separately from authored `progress/components.json` scopes for PROG.md and README. Numeric activity does not automatically promote a component status. Neither artifact contains RAM, function-address lists, asset data or raw report strings. Keep the full probe report ignored. The hash identifies the measured report; it does not mean the implementation is committed or fully verified.

After an actual startup measurement, publish only its allowlisted counters:

```powershell
python tools/docs/export_startup_progress.py --report work/bt2-generation/entry-stop.json --measured-on YYYY-MM-DD
python tools/docs/export_startup_progress.py --report work/bt2-generation/entry-stop.json --measured-on YYYY-MM-DD --check
python tools/docs/render_status.py
python tools/docs/render_status.py --check
python tools/docs/sync_progress.py
python tools/docs/sync_progress.py --check
```

Review the snapshot against PROG.md's tables and pass `--verification verified` only when the active pass has passed its required gates. The exporter measures a startup checkpoint; its default unverified label remains until required gates pass. It does not certify build/tests, infer subsystem completion, or run the game. Capture/test gates remain in the active handoff. README synchronization adjusts only the dashboard's relative image path.

Reference: [Chronicle's progress workflow](https://github.com/TheMoonPeople/Chronicle/blob/master/.github/workflows/progress.yml) publishes generated per-region objdiff reports with function and byte matching measures; its [report generator](https://github.com/TheMoonPeople/Chronicle/blob/master/scripts/build/progress_report.py) separates executable/overlay categories. We adapt the generated-report/category model to native execution and subsystem evidence. Their decompilation matching percentages do not measure BT2 runtime completeness. No reference implementation was copied, and no external reporting account or game-data CI upload was configured.
## Compact component map and README layout

Edit `progress/components.json` when scoped evidence changes, run `tools/docs/render_status.py`, then synchronize README. Keep labels short and state verified scope in each note. Keep detailed counters in PROG.md and the numeric snapshot; README displays the map, milestone table, current boundary and a few key measurements. There is no automatic inference that a whole SDK library or hardware subsystem works because some functions execute.

The milestone table and component map describe BT2-specific evidence. No weighted completion estimate is used.