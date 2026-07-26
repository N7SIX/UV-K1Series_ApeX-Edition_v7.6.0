#!/usr/bin/env sh
set -eu

OUT_FILE=""
if [ "${1:-}" = "--record" ]; then
  OUT_FILE="${2:-cw_regression_results_$(date +%Y%m%d_%H%M%S).txt}"
fi

print_line() {
  printf "%s\n" "$1"
  if [ -n "$OUT_FILE" ]; then
    printf "%s\n" "$1" >> "$OUT_FILE"
  fi
}

print_line "CW Regression Checklist"
print_line "Date: $(date)"
print_line ""
print_line "Build"
print_line "[ ] 1. Build ApeX preset succeeds (compile-with-docker.sh ApeX)."
print_line "[ ] 2. Flash image and cold boot radio."
print_line ""
print_line "Activation"
print_line "[ ] 3. From MAIN screen: press F then 7 (normal cadence) -> CW opens."
print_line "[ ] 4. From MAIN screen: press-and-hold 7 after F -> CW still opens."
print_line "[ ] 5. Press EXIT in CW -> returns to MAIN cleanly."
print_line ""
print_line "Compose UI"
print_line "[ ] 6. Status line shows mode/WPM/case (example: 'CW C 20 U')."
print_line "[ ] 7. MENU cycles WPM 10/15/20/25/30 and wraps to 10."
print_line "[ ] 8. STAR toggles case U/L and typed letters follow case."
print_line "[ ] 9. Multi-tap works: KEY_2 cycles A/B/C (or a/b/c in lower)."
print_line "[ ] 10. KEY_0 inserts space without corrupting previous char."
print_line "[ ] 11. KEY_F clears message in CW compose mode."
print_line "[ ] 11a. Message accepts punctuation and numerals (example: 'CQ 73 ?')."
print_line ""
print_line "Transmit"
print_line "[ ] 12. PTT in CW sends message as Morse tone over RF."
print_line "[ ] 13. TX status line appears ('CW TX ...') with progress bar movement."
print_line "[ ] 14. After send, radio returns to normal RX state (no stuck TX LED/state)."
print_line "[ ] 15. Empty message + PTT does not key TX unexpectedly."
print_line "[ ] 15a. International set TX check: send 'TEST 123 CQ ?' and verify expected rhythm."
print_line "[ ] 15b. Prosign TX check: send '<AR> <BT> <SK> <KN> <AS> <SN> <SOS>'."
print_line "[ ] 15c. Prosign token case-insensitive: '<ar> <Sk> <sos>' transmits same as uppercase."
print_line ""
print_line "Receive / Decode"
print_line "[ ] 16. RX line updates as Morse is received (prefix 'RX:')."
print_line "[ ] 17. Decode letters/digits: transmit from 2nd radio 'CQ 73' -> RX shows 'CQ 73'."
print_line "[ ] 18. Decode punctuation: send '?' and '@' and verify symbols render on RX line."
print_line "[ ] 19. Decode prosigns: send AR/BT/SK/KN/AS/SN/SOS -> RX shows <AR>/<BT>/<SK>/<KN>/<AS>/<SN>/<SOS>."
print_line "[ ] 20. Gap handling: normal word spacing inserts a single blank between decoded words."
print_line "[ ] 21. No false stuck decode: after carrier drops, RX line stabilizes without rapid garbage append."
print_line ""
print_line "Interference / Regression"
print_line "[ ] 22. VOX action still works when CW feature is disabled build variant."
print_line "[ ] 23. F-key icon/state is not stuck after entering/exiting CW."
print_line "[ ] 24. Main UI status row remains intact while CW is active."
print_line "[ ] 25. During CW, only single VFO view is shown (no conflicting dual VFO draw)."
print_line ""
print_line "Result"
print_line "[ ] PASS all"
print_line "[ ] FAIL (note steps):"
print_line ""

if [ -n "$OUT_FILE" ]; then
  print_line "Saved checklist to: $OUT_FILE"
else
  print_line "Tip: use '--record <file>' to save output."
fi
