# Bugs and quirks in the original code

Things a port must either reproduce exactly (when game behaviour depends on them) or fix on
purpose. Each is confirmed by C that compiles to the original bytes unless marked inferred.

## Behaviour-affecting

| Where | What | Consequence |
|---|---|---|
| `Rand_Next` (`src/sys/rand.c`) | The MT19937 refill has only its first loop and the final word; the second loop is missing. | 396 of 624 state words never change after seeding. The game's random sequence is not MT19937. A port must copy the broken refill to reproduce behaviour. |
| `BtlSeq_JudgeByHealth` | On equal health with no rule flag, outside the mode-0 time-up draw, the winner is `rand() & 1`. | A double KO can be decided by a coin flip from the C library generator. |
| `BtlMember_Init` (`battle_load.c`) | Applies items (and so looks up the default AI type) before storing the character id. | The default AI type is always looked up for character 0. |
| `Dma_PutTexStrips` | The float path adds the X offset without the `<< 4` the integer path applies. | Textured strips whose width does not divide evenly are positioned differently. |
| `Fade_IsDone` | Returns 1 when the done flag is set, which is one frame before the final colour is computed, and also for a slot that is off. | Callers proceed one frame early. |
| `Pad_Update` | A digital-only pad is reset after being read. | Controllers without analog sticks give no input at all. |
| `Pad_Reset` | Does not clear the game-level stick copies. | They keep stale values while a pad is unplugged. |

## Missing checks (crash or corruption if the limit is hit)

| Where | What |
|---|---|
| `BtlPool_Alloc` | No capacity check: an arena that overflows runs into the next one. |
| `BtlPool_CarveArenas` | Slot 5 tests slot 4's mask bit and slot 8 tests slot 7's. Harmless only because every bit is always set. |
| `File_Request` | No check for an empty free list: a 33rd pending request dereferences NULL. |
| `Snd` command queue | A 33rd command in one frame is dropped (the play returns -1); this one is handled. |
| `File_LoadSyncEx`, `Overlay_Load`, boot | Every disc operation retries forever; there is no failure path. |
| `Heap_Free` | A pointer that is not a live block calls an empty report function and then frees a NULL block. |

## Dead or broken code that nothing calls

| Where | What |
|---|---|
| `Quat_Normalize` | Reads the length from its output argument, so it is only correct in place. No callers. |
| `Save_UnlockAll` | Debug "unlock everything". No callers. |
| `Disc_Identify`, `Disc_GetDriveState` | Disc check for the three Tenkaichi games. No callers. |
| `Snd_Term` | Its loop unloads only empty slots, so it does nothing. No callers. |
| `Fade_Init` | Clears only slot 0 of three; the others rely on zeroed memory. |
| `Heap_SetDebug*`, `Dbg_*`, `Gfx_MarkPass`, `Heap_ReportBadFree` | Stripped debug hooks, compiled to empty functions. |
| ~25 list/queue/alignment utilities | Never called: a general utility library was linked in. |

## Fighter behavior

| Where | What | Consequence |
|---|---|---|
| `Mathf_WrapAngle` | The intended lower bound is overwritten, so every angle below 2 pi is pushed up by 2 pi and brought back. | Angles are quantised before `sinf`; a port must keep the sequence. A huge angle loops forever. |
| `BtlObj_UpdateVisibility` | The distance loop means to take the minimum over four box corners but passes the same corner four times. | Distance fade uses one corner. |
| `BtlObj` fighter work buffers | The code accepts a third fighter object, whose buffer would overlap the next pool. | Latent; only two fighters exist. |
| `BtlChars_UpdateFreeze` | At hit-stop level 2 the loop stops at the first exempt fighter. | Fighter 1's exemption is never considered when fighter 0 is at level 2: roster order matters. |
| `SpuHeap_Alloc` / `Free` | No failure path: exhaustion, more than 17 ranges, or an unknown address dereference NULL. | |
| `Gsc_StepTask` | An unknown command id calls a NULL handler. | |
| `BtlScript` triggers | Command 7 fills up to 50 requests with no capacity check. | |
| Replay recording | At 9000 frames the recorder silently stops storing. | A fight longer than 5 minutes of input-taking frames cannot be replayed to the end. |
| Replays and `rand()` | Nothing reseeds `rand()` for a replay. | A replayed double KO can resolve differently from the original (inferred). |
| `Timer_GetFrames` | The multiply wraps after about 71.6 s. | Used only by the movie player. |

## Fighter state handling

| Where | What | Consequence |
|---|---|---|
| `BtlChange_Update` / time stop | The fight is frozen for as long as a transformation, fusion or switch model takes to load. | Duration depends on disc speed; a deterministic port must fix it. |
| `BtlColl_Update`, `BtlHit_ApplyHit` | Fighter 0 is tested and applied first; its hit writes fighter 1's reaction and health before fighter 1's hit is applied. | Player 1 / player 2 asymmetry. |
| `BtlAiPad_Set` | The stick is averaged against a per-frame accumulator that starts at zero. | The CPU sends half the stick deflection it asks for. |
| Script commands 13 / 14, option `-B` | Calls the ki adder instead of the blast adder. | Scripts cannot add blast gauge. |
| Script command 1602, option `-h` | The channel value is stored and then overwritten with 0. | Voice always plays on channel 0. |
| Fighter flags | Flags written before the first stage of a fight are never promoted; `ClearFlag` does not refresh the previous plain bit. | Edge tests can misreport in those cases. |
| `prev` action (fighter +0x950) | Never written anywhere in the executable. | Reads 0, so every branch on the previous action is constant: ki blasts never alternate hands, clash C always plays its first strike animation, dash start animations never vary. |
| `BtlMove_CalcApproachPoint` | Takes `sqrt(len^2 - dy^2)`, which can be negative (inferred hazard). | The PS2 returns a number where a PC returns NaN. |
| `BtlMove_CalcVerticalSpeed`, `BtlMove_CalcJumpSpeed` | Loops end on a sign change (inferred hazard). | Would not end on a NaN. |
| `BtlCharSnd_PlayCommonFar` | near = far = 100000, so the attenuation divides by zero. | |
| Fighter sound requests | A fifth request in one frame is dropped silently. | |
| `BtlColl_TryGuard` | Allows a guard kind 6 that skips the from-behind test, but the classifier never returns 6. | Dead branch. |

## Differential behavior findings

- `EftChain_BlendKeys` (src/battle/eft_s.c, chain effect key blending): the original computes
  `endAlpha = row20[k0] + (row9[k1] - row9[k0]) * t`, i.e. it interpolates the end alpha with
  the DELTA OF ROW 9 (the shrink rate) instead of row 20's own delta: a copy-paste slip in
  the original source. Visual only. Our C attempt had "corrected" it silently; it now
  reproduces the original (marked `sic` in the source).
- Uninitialised stack reads in the original that reach outputs (a PC build inherits them
  unless the locals are initialised): `EftGeyser_StartSmoke` (8 bytes of the 156-byte block
  passed to `EftSmoke_Create`, from byte +76), `HudGauge_UpdateAura` (u16 locals reaching
  memory stores). Visual only; a port should zero these and accept that the PS2's result
  depended on stack junk.
