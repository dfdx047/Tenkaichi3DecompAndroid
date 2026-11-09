# EE Timer 2 system clock

## Why this is a new subsystem

After display setup, main reads the system time (`0x100658` → `0x339410`) and seeds the C library random state with it (`0x325370`). The value comes from a timer library at `0x338B90`–`0x339B74`: (software overflow count << 16 | Timer 2 COUNT) << (CLKS × 4). Startup programs Timer 2 with mode `0x382` (BUSCLK/256, count, compare and overflow interrupts) and registers handler `0x339118` for INTC cause 11. The probe models the mode write and enable only: no counter runs, no overflow interrupt is delivered and the overflow word at `0x36BAD0` stays 0.

## Evidence

- `rpc-timer-seed-after`: original result `0x1C825400` (28 overflows, COUNT `0x8254`, about 3.24 s since the timer started). The overflow word is already 0x1C at `rpc-load-poll-done`.
- The alarm list head (`0x36BAD0+0x18`) is empty in both runs; the handler's compare path processes no alarms.
- Handler `0x339118`: on EQUF it walks the alarm list; on OVFF it increments the overflow word and clears OVFF by writing MODE, then reprograms compare (`0x338EC8`).

## Bounded design

1. Logical Timer 2: count in BUSCLK/256 ticks, advanced only at existing logical VBlank points (NTSC field = 9,609.6 ticks, kept in exact fixed point). No host timer.
2. COUNT/MODE/COMP reads and writes from main, helpers and the cause-11 handler: OVFF/EQUF write-one-to-clear; COMP writes recorded.
3. When a tick advance wraps COUNT (or reaches COMP), latch OVFF/EQUF. Deliver cause 11 through the existing interrupt path at the same scheduling point when enabled, with the kernel handler ABI.
4. Select the handler and timer functions with the existing tooling; compare the overflow word, alarm list and timer state.
5. Verify: unit tests for tick accumulation, wrap, flag clearing and masked delivery; normal startup; the seed value is reported, not equated (timing is logical).

## Limits and stop conditions

The original seed is timing-dependent and is not reproduced. Do not fabricate alarm callbacks. Stop if a reached alarm needs thread wake semantics beyond the existing scheduler, or if compare programming requires cycle timing.

## Outcome (2026-10-07)

Steps 1–5 are implemented. Logical Timer 2 starts from the startup mode at COUNT 0 / COMP 0 (BIOS residue unobservable), advances one field per logical VBlank (delivered or polled) and delivers cause 11 before the VBlank handlers at the same point. The original handler acknowledges every wrap (16 by the current checkpoint) and reprograms COMP to 0. Library state matches the original apart from the overflow count (logical time) and the model-local handler ID. `verify_timer2.py` reports the native and original seeds without equating them.
