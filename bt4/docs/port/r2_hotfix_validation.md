> Historical preliminary R2 Hotfix measurements. Superseded by the R3 validation record. The interpolation-off label below was incorrect: the R2 migration forced interpolation on. The indexed cross-address replacement cache described below has been removed from R3.

# R2 Hotfix validation

This candidate starts from public R2 (`f103106`), not the withdrawn R2.1 renderer or input-delay changes. Its intended release title is `0.1.39.5 R2 Hotfix`; the next distinct package version is `0.1.39.5-R2.2`.

## Performance changes

Resident pack replacements now have a hash index over their complete sampling identity and a validated GS address alias. Cache compaction rebuilds both indexes. An asynchronous pack replacement cancels the pending software artwork enlargement for that entry. CPU pack eviction starts above the full budget, retaining the existing 75% eviction target.

No rendering quality preset, simulation speed, texture-pack image, or simulation/network ABI changed. This work does not add DLSS. The reported profile was dominated by CPU texture lookup, and the local validation GPU was Intel Iris Xe. DLSS integration would require a separate renderer project and compatible hardware. NVIDIA's integration requirements are described at https://developer.nvidia.com/blog/how-to-integrate-nvidia-dlss-4-into-your-game-with-nvidia-streamline/ .

## Local renderer comparison

Hidden Direct3D 12 windows, 1280x720 output, internal scale 2, original menu fonts, interpolation off, mods off, the same installed Spanish data and HD pack. Each run lasted about 18 seconds. Samples below are logged render-frame times after frame 120, not average displayed FPS or input-to-display latency. This short synthetic scene selection does not cover every character, stage or hardware combination.

| Scene | R2 frame time p95 | Candidate frame time p95 | R2 texture lookup p95 | Candidate texture lookup p95 |
| --- | ---: | ---: | ---: | ---: |
| Demo duel, HD, repeat run | 72.2 ms | 21.6 ms | 56.5 ms | 3.8 ms |
| Paused native duel, HD | 57.0 ms | 24.4 ms | 48.1 ms | 3.7 ms |
| Story battle script 0, HD | 79.8 ms | 23.6 ms | 67.3 ms | 6.6 ms |
| Paused native duel, pack disabled | 20.7 ms | 23.3 ms | 1.6 ms | 1.9 ms |

The pack-disabled case did not establish an improvement. Individual loading/driver spikes remain; the comparison does not establish permanent frame pacing or long-session memory stability. All scenarios reached combat. The paused fixtures froze the battle clock after injected Start input. Test processes were deliberately terminated after the measurement interval.

## Network and development validation

- Discovery tests compile the production client and cover omitted mode/role metadata, explicit lobby routing, spectator separation, and ordered admission requests.
- Three native clients against the live signaling service completed host, spectator and fighter admission and 100 UDP echoes per guest. The original R2 client also passed this test against the current service, so the user's particular 1v1 room failure was not reproduced.
- Native Tag Team timing tests cover short taps, one-shot commands, continuous frame scheduling, solo CPU input, epoch reset, duplicate/reordered feedback, and RTT filtering. R2's initial one-frame cushion, eight-frame ceiling and clean-frame decay threshold remain in place. Remote gameplay latency has not been measured in this candidate.
- A new development executable, built from this candidate with `BT4_DEVELOPMENT_BUILD`, connected to the published R2 game over local UDP, passed character selection, and entered combat on both clients. The simulation family, data checks, duel wire version and Tag Team protocol remain shared. This does not validate an older unrelated Dev executable or altered battle data.
- Renderer helper and content tests cover stale aliases, dimensions, TEXA, alpha identity, eviction/compaction, asynchronous replacement, CPU cache watermarks, xBRZ output, render fences, rematch epochs, ZIP/PNG loading and audio/song handling.

## Release gate

The candidate game and installer were built locally for Windows. Local portable ZIP and source snapshot are prepared for manual evaluation. No candidate branch, tag, assets or release were uploaded. User gameplay approval precedes uploading a draft. Linux/macOS package builds and signatures are still required in the release workflow before a complete draft can be published by the maintainer.
