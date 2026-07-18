# CW Validation Report Template

Date: ____________________
Tester: ____________________
Firmware Build ID: ____________________
Radio Model: ____________________
Second Radio / Source: ____________________
Environment: ____________________

## Summary

- Total checks: 25
- Passed: ____
- Failed: ____
- Blocked/Not run: ____
- Overall result: PASS / FAIL

## Execution Notes

- WPM used: ____________________
- Tone used: ____________________
- Distance / RF level context: ____________________
- Any notable interference/noise: ____________________

## Checklist Results

### Build

- [ ] 1. Build ApeX preset succeeds (compile-with-docker.sh ApeX).
- [ ] 2. Flash image and cold boot radio.

### Activation

- [ ] 3. From MAIN screen: press F then 7 (normal cadence) -> CW opens.
- [ ] 4. From MAIN screen: press-and-hold 7 after F -> CW still opens.
- [ ] 5. Press EXIT in CW -> returns to MAIN cleanly.

### Compose UI

- [ ] 6. Status line shows mode/WPM/case (example: CW C 20 U).
- [ ] 7. MENU cycles WPM 10/15/20/25/30 and wraps to 10.
- [ ] 8. STAR toggles case U/L and typed letters follow case.
- [ ] 9. Multi-tap works: KEY_2 cycles A/B/C (or a/b/c in lower).
- [ ] 10. KEY_0 inserts space without corrupting previous char.
- [ ] 11. KEY_F clears message in CW compose mode.
- [ ] 11a. Message accepts punctuation and numerals (example: CQ 73 ?).

### Transmit

- [ ] 12. PTT in CW sends message as Morse tone over RF.
- [ ] 13. TX status line appears (CW TX ...) with progress bar movement.
- [ ] 14. After send, radio returns to normal RX state (no stuck TX LED/state).
- [ ] 15. Empty message + PTT does not key TX unexpectedly.
- [ ] 15a. International set TX check: send TEST 123 CQ ? and verify expected rhythm.
- [ ] 15b. Prosign TX check: send <AR> <BT> <SK> <KN> <AS> <SN> <SOS>.
- [ ] 15c. Prosign token case-insensitive: <ar> <Sk> <sos> transmits same as uppercase.

### Receive / Decode

- [ ] 16. RX line updates as Morse is received (prefix RX:).
- [ ] 17. Decode letters/digits: transmit from 2nd radio CQ 73 -> RX shows CQ 73.
- [ ] 18. Decode punctuation: send ? and @ and verify symbols render on RX line.
- [ ] 19. Decode prosigns: send AR/BT/SK/KN/AS/SN/SOS -> RX shows <AR>/<BT>/<SK>/<KN>/<AS>/<SN>/<SOS>.
- [ ] 20. Gap handling: normal word spacing inserts a single blank between decoded words.
- [ ] 21. No false stuck decode: after carrier drops, RX line stabilizes without rapid garbage append.

### Interference / Regression

- [ ] 22. VOX action still works when CW feature is disabled build variant.
- [ ] 23. F-key icon/state is not stuck after entering/exiting CW.
- [ ] 24. Main UI status row remains intact while CW is active.
- [ ] 25. During CW, only single VFO view is shown (no conflicting dual VFO draw).

## Failed Checks Detail

Use one block per failed item.

- Check ID: ______
- Observed behavior:
- Expected behavior:
- Reproduction steps:
- Frequency/WPM/tone context:
- Severity: Critical / High / Medium / Low
- Suspected area(s):
- Notes:

## Quick Triage Map (Likely Code Areas)

- Activation and F+7 path:
  - App/app/main.c
  - App/app/cw.c
- Key input and compose behavior:
  - App/app/cw.c
- TX tone, pacing, and teardown:
  - App/app/cw.c
  - App/app/app.c
- RX decode and symbol classification:
  - App/app/cw.c
- Main screen integration and single-VFO display:
  - App/ui/main.c

## Verdict

- Release-ready for CW: YES / NO
- If NO, blocking check IDs:
- Recommended next action:
