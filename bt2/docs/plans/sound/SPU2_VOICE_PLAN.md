# SPU2 voice state for the SOUNDS timer tick

## Why this is a new subsystem

The overlay's frame path waits 400 H-BLANKs on Timer 1. The first field boundary inside that wait delivers a logical VBlank, which advances the IOP's 240 Hz SOUNDS hardware timer for the first time in normal startup. The timer handler wakes the SOUNDS worker. Its LIBSD path (IOP PC `0x937A4`) then reads core 0's ENDX register (`0x1F900340`), the per-voice "reached a loop-end block" flags.

So far SPU2 registers have been answered only as retained parameters, or as one immediate readback right after a command write (KON, KOFF, ENDX clear). ENDX read later depends on how far each keyed-on voice has played. Answering it needs a voice playback model rather than a stored value.

## Evidence available

- **Sound RAM.** Contents are already owned: the initialization I/O transfer and every voice DMA (DMA4/DMA7) write into `spu2_sound_ram`. Uploaded bytes match the original where compared (`verify_sound_bank.py`).
- **Voice registers.** Pitch, ADSR, start address (SSA), loop address and volumes are retained in `spu2_configuration` as the driver writes them.
- **Key-on and key-off.** Retained as `spu2_pending_key_on` / `spu2_pending_key_off`; the voices keyed so far were initialized with zero volume.
- **Logical time.** Each SOUNDS timer interval is 4,167 µs of IOP time (36.864 MHz clock, 615,014.4 clocks per field).

## Bounded design

1. **Voice state.** Per keyed voice: current block address, sub-sample position (pitch in 4.12 fixed point at 48 kHz), and the ENDX bit. No audio samples are produced.
2. **Advance.** Advance voices by the elapsed output samples whenever logical IOP time advances (timer intervals). Walk 16-byte ADPCM blocks (28 samples each) using only their flag byte:
   - loop end → set ENDX;
   - with repeat → continue at the loop address;
   - without repeat → stop the voice.
   The loop-start flag sets the loop address.
3. **Registers.** ENDX reads return the modeled flags and writes clear them. KON starts a voice at SSA, and KOFF moves it to release (envelope not modeled; ENVX reads stay checked).
4. **Unit tests.** Use synthetic block chains: end without repeat, repeat loop, pitch scaling and KOFF.
5. **Comparison.** Compare with the original where observable. LIBSD's software copies of voice state in IOP RAM at a capture point after the first tick are one source. ENDX timing itself is not observable in PCSX2 captures, so that is stated as a limit.

## Limits and stop conditions

- No audio output, envelope (ENVX) values or mixing are claimed.
- Stop if the driver needs envelope levels, or sample-accurate timing that can't be derived from logical time and pitch.

## Implemented scope for the current pass

The authored position model admits the reached muted startup voices. Their pitch/SSA are captured at KON; active pitch changes and manual loop-address writes remain unsupported by the MMIO adapter. The clock begins at the admitted SOUNDS timer intervals. Initialization-to-timer playback phase, decoder lookahead and exact ENDX sample timing are not compared. Logical sample-clock residue is carried between intervals and fields; each timer handler sees advancement to its own compare boundary. KOFF records release and keeps positions advancing because no ADSR release duration is owned.

## Progress (2026-10-08, unverified)

- **Envelope:** ADSR is modelled per output sample from the public ADSR1/ADSR2 layout (PCSX2's rate-counter scheme as the reference). ENVX reads return the level, a finished envelope stops the voice, and an ADPCM end without repeat zeroes it.
- **Live registers:** pitch, ADSR and LSA writes apply to a playing voice. An LSA written after key-on overrides block loop-start flags until the next key-on, as in PCSX2. Volumes and routing masks are retained, since mixing is not modelled.
- **Not claimed:** decoded samples, mixing and VOLX.
