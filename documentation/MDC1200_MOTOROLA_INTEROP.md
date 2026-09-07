# MDC-1200 Motorola Interop & Current Behavior Reference

**Version**: post-v7.6.10C (interop remap session)
**Status**: ✅ IMPLEMENTED & VERIFIED (firmware build clean, 17,556 host checks pass)
**Authoritative document** — supersedes the opcode tables in `MDC1200_PHASE2.md` and the
audio-alert descriptions in `MDC1200_RX_PATH.md` where they differ.

---

## 1. Executive Summary

The MDC-1200 implementation has been re-aligned with the **commercial Motorola protocol**:

- The RX dispatcher now uses the **Motorola standard opcode map** (previously a local 0x00–0x07 map
  that genuine Motorola traffic could not map to).
- The TX PTT-ID burst defaults to the **Motorola standard ANI (opcode 0x01, arg 0x00)** — the only
  burst a genuine Motorola recognizes as an ID.
- **Emergency ANI is transmitted 3×** back-to-back (Motorola practice) and RX emergency recognition
  now covers Motorola's `0x81`/`0x82`/`0x83`.
- The receiving radio plays a **local MDC preamble warble** through its speaker when it decodes an
  ID — audible **on simplex and through repeaters**, independent of whether the repeater relays the
  on-air burst audio (many repeater controllers mute/strip MDC data).
- The burst placement is **end-of-transmission only** (a previously added start-of-transmission
  burst was removed at operator request).
- Unit IDs are displayed in **decimal** (Motorola convention).

---

## 2. TX Behavior (end-of-transmission burst)

| Property | Value |
|---|---|
| Placement | **End of transmission only** — `RADIO_SendEndOfTransmission()` → `RADIO_SendMdcId()` (`App/radio.c`) |
| Channel gate | Per-channel `PTT ID` menu = `MDC-1200` or `MDC-1200L` (auto-saved to the channel EEPROM on menu confirm) |
| Opcode default | **`0x01` (Motorola ANI), arg `0x00`** — used whenever the EEPROM opcode is `0x00` ("unset") |
| Custom opcodes | Honored as-is when configured via CHIRP (whitelist, see §5) |
| Frame | Standard 26 B (`MDC-1200`) or 46 B composite (`MDC-1200L`), full-frame FIFO, differential-encoded |
| Emergency repeat | **3× back-to-back** for `0x81`/`0x82` (and legacy `0x05–0x07`) |
| Ordering constraint | The burst runs **before** `DTMF_SendEndOfTransmission()` — after the DTMF mute point the FSK data path does not modulate |

> **Note:** the burst is transmitted on the repeater *input* frequency and is usually **not audible
> back through the repeater** (relay latency + controllers that mute MDC data). This is expected;
> the audible indication is the RX-side warble described below.

## 3. RX Behavior (decode + local warble)

```
Carrier up → squelch opens → FSK demod armed (RADIO_SetupRegisters, App/radio.c:925)
  → BK4829 FSK RX collects frame → FSK_RX_FINISHED IRQ (10 ms slice)
  → APP_HandleMDC1200Receive() [App/app/app.c]
      → read FIFO words → MDC1200_DiffDecodeFrame() → MDC1200_DecodeFrameWords()
      → re-arm FSK RX (next frame decodes)
  → MDC_DispatchFrame() [App/mdc_handler.c]
      → duplicate suppression (identical unit+op within 10 s → silent; emergencies exempt)
      → Motorola opcode switch → handler
          → MDC_TriggerDisplay()  → center-line "MDC:" alert (decimal Unit ID)
          → MDC_PlayAlert()       → AUDIO_PlayMDCWarble() [App/audio.c] — LOCAL speaker
```

Key points:

- **`AUDIO_PlayMDCWarble()`** reproduces the preamble sound (rapid 1200/1800 Hz alternation) through
  the local speaker. It is explicitly permitted during `FUNCTION_RECEIVE`/`FUNCTION_MONITOR`
  (`AUDIO_PlayBeep()` refuses to play during RX — the reason MDC RX alerts never sounded before).
- **RX decoder arming is a configuration requirement:** the **receiving** radio's channel must have
  its `PTT ID` menu set to `MDC-1200`/`MDC-1200L`. Without it, no decode occurs on simplex *or*
  repeater. The same per-channel field selects the TX burst mode.
- **Duplicate suppression:** identical Unit ID + opcode re-decodes within 10 s are silenced;
  emergency frames are always alerted.
