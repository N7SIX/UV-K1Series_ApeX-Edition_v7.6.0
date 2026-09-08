#!/usr/bin/env python3
"""Firmware size gate for Sean, N7SIX.

Parses `arm-none-eabi-size` output for one or more ELF files and fails if
FLASH (text+data) or RAM (data+bss) usage exceeds a percentage of the MCU
region. The linker already fails at 100%; this gate fails EARLIER so that
memory growth is caught in review instead of at the flash cliff.

Note: the linker script reserves heap+stack INSIDE .bss (". = . + _Min_..." ),
so data+bss from `size` is the true RAM footprint - no adjustment needed.

Usage:
    check_size.py [--flash-kb 118] [--ram-kb 16] [--fail-pct 97]
                  [--warn-pct 90] ELF [ELF ...]

Exit codes: 0 = all within budget, 1 = one or more over budget.
GitHub annotations (::warning:: / ::error::) are emitted when GITHUB_ACTIONS
is set, so violations show up inline on the PR.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys


def read_sizes(elf_path: str):
    """Return (flash_bytes, ram_bytes) for an ELF via a `size` tool."""
    tool = shutil.which("arm-none-eabi-size") or shutil.which("size")
    if not tool:
        sys.exit("error: no arm-none-eabi-size or size tool found on PATH")
    proc = subprocess.run([tool, elf_path], capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit(f"error: {tool} failed on {elf_path}:\n{proc.stderr}")
    rows = [ln for ln in proc.stdout.splitlines() if re.match(r"\s*\d+\s+\d+\s+\d+", ln)]
    if not rows:
        sys.exit(f"error: could not parse size output for {elf_path}:\n{proc.stdout}")
    text, data, bss = (int(x) for x in rows[-1].split()[:3])
    return text + data, data + bss


def annotate(kind: str, message: str):
    if os.environ.get("GITHUB_ACTIONS") == "true":
        print(f"::{kind}::{message}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("elf", nargs="+", help="ELF file(s) to check")
    ap.add_argument("--flash-kb", type=int, default=118, help="FLASH region size in KiB (default 118)")
    ap.add_argument("--ram-kb", type=int, default=16, help="RAM region size in KiB (default 16)")
    ap.add_argument("--fail-pct", type=int, default=97, help="fail above this %% of the region (default 97)")
    ap.add_argument("--warn-pct", type=int, default=90, help="warn above this %% of the region (default 90)")
    args = ap.parse_args()

    flash_limit = args.flash_kb * 1024
    ram_limit = args.ram_kb * 1024
    fail_at = args.fail_pct / 100.0
    warn_at = args.warn_pct / 100.0

    failed = False
    for elf in args.elf:
        if not os.path.isfile(elf):
            sys.exit(f"error: ELF not found: {elf}")
        flash, ram = read_sizes(elf)
        flash_pct = flash / flash_limit
        ram_pct = ram / ram_limit
        name = os.path.basename(os.path.dirname(os.path.abspath(elf))) or elf

        print(f"[{name}] FLASH {flash}/{flash_limit} B ({flash_pct * 100:.2f}%)  "
              f"RAM {ram}/{ram_limit} B ({ram_pct * 100:.2f}%)  "
              f"budget: fail>{args.fail_pct}%, warn>{args.warn_pct}%")

        for label, used, pct, limit in (("FLASH", flash, flash_pct, flash_limit),
                                        ("RAM", ram, ram_pct, ram_limit)):
            if pct > fail_at:
                annotate("error", f"{name}: {label} {used} B is {pct * 100:.2f}% of the "
                                  f"{limit} B region (budget {args.fail_pct}%). Shrink the "
                                  f"change or extend --{label.lower()}-kb deliberately.")
                failed = True
            elif pct > warn_at:
                annotate("warning", f"{name}: {label} at {pct * 100:.2f}% of region "
                                    f"(warn {args.warn_pct}%). Approaching the size gate.")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
