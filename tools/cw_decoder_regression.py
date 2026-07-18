#!/usr/bin/env python3
"""Regression checks for the CW decoder classifier.

Matches CW_ClassifyMark() in cwdecoder.c (fixed WPM-based timing):
  - Returns true (dah) if markTicks >= dahThreshold (2.5 * ditTicks)
  - Returns false (dit or too short) otherwise
  - All timing derived from WPM setting: dah = 3×dit, dah_threshold = 2.5×dit
"""


def classify_mark(mark_ticks, dit_ticks):
    """Mirrors CW_ClassifyMark() — fixed WPM-based."""
    threshold = (dit_ticks * 5) // 2
    if mark_ticks < max(2, (dit_ticks + 1) // 2):
        return False  # too short → ignore
    if mark_ticks >= threshold:
        return True   # dah
    return False      # dit


def main():
    # Standard 20 WPM: dit = 6 ticks (60ms)
    d = 6
    assert classify_mark(11, d) is False, "11 ticks is dit (<15)"
    assert classify_mark(15, d) is True,  "15 ticks is dah (threshold)"
    assert classify_mark(18, d) is True,  "18 ticks is dah (3×dit)"
    assert classify_mark(14, d) is False, "14 ticks is dit"
    assert classify_mark(2,  d) is False, "2 ticks too short"

    # Slow: 10 WPM: dit = 12 ticks (120ms)
    d = 12
    assert classify_mark(12,  d) is False, "12 ticks is dit"
    assert classify_mark(29,  d) is False, "29 ticks is dit (<30)"
    assert classify_mark(30,  d) is True,  "30 ticks is dah (threshold)"
    assert classify_mark(36,  d) is True,  "36 ticks is dah"

    # Fast: 40 WPM: dit = 3 ticks (30ms), threshold=7
    d = 3
    assert classify_mark(1,   d) is False, "1 tick too short"
    assert classify_mark(2,   d) is False, "2 ticks is dit"
    assert classify_mark(3,   d) is False, "3 ticks is dit (<7)"
    assert classify_mark(7,   d) is True,  "7 ticks is dah (threshold)"
    assert classify_mark(8,   d) is True,  "8 ticks is dah"
    assert classify_mark(9,   d) is True,  "9 ticks is dah (3×dit)"

    # Very fast: 50 WPM: dit = 2 ticks (20ms), threshold=5
    d = 2
    assert classify_mark(1,   d) is False, "1 tick too short"
    assert classify_mark(2,   d) is False, "2 ticks is dit (<5)"
    assert classify_mark(5,   d) is True,  "5 ticks is dah (threshold)"
    assert classify_mark(6,   d) is True,  "6 ticks is dah (3×dit)"
    assert classify_mark(4,   d) is False, "4 ticks is dit (<5)"

    # Boundary: dah threshold = 2.5×dit for all valid speeds
    for dit in range(1, 25):  # covers 2.5 to 60 WPM
        thresh = (dit * 5) // 2
        if thresh > 1:
            assert classify_mark(thresh - 1, dit) is False, f"dit={dit}: {thresh-1} = dit"
        assert classify_mark(thresh, dit) is True, f"dit={dit}: {thresh} = dah"

    print("CW decoder regression passed (fixed WPM timing)")


if __name__ == "__main__":
    main()