#!/usr/bin/env python3
"""Minimal regression checks for the CW receiver classifier."""


def classify_current(mark_ticks, dit_ticks):
    dah_threshold = (dit_ticks * 5) // 2
    if mark_ticks < max(2, dit_ticks // 2):
        return False
    if mark_ticks >= dah_threshold:
        return True
    return mark_ticks >= (dit_ticks * 3) // 2


def classify_improved(mark_ticks, dit_ticks):
    if mark_ticks < max(2, dit_ticks // 2):
        return False
    return mark_ticks >= max(3, dit_ticks * 2)


def main():
    # Borderline case: a mark at 1.83x the dit estimate should stay a dot.
    borderline = 11
    dit_ticks = 6
    assert classify_current(borderline, dit_ticks) is False, "current classifier should not turn this into a dash"
    assert classify_improved(borderline, dit_ticks) is False, "improved classifier should keep this as a dot"
    print("CW decoder regression passed")


if __name__ == "__main__":
    main()
