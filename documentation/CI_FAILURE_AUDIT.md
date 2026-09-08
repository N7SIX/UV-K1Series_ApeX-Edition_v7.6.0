# CI Failure Audit — why the repo showed a red ❌ on GitHub

**Date:** 2026-09-08 · **Scope:** GitHub Actions runs on `N7SIX/UV-K1Series_ApeX-Edition_v7.6.0`
**Method:** GitHub REST API (`/actions/runs`, `/runs/{id}/jobs`, `/commits/{sha}/check-runs`) + git history. No authentication needed for public repos.

---

## TL;DR

The red ❌ was **real and was the new `firmware` CI job** (added in commit
`6ceaebf` "perf: defer flash writes… add CI firmware build + size gate").
It failed **once, on one commit**, for one reason:

> **The GitHub runner's `gcc-arm-none-eabi` package does not ship newlib
> headers (`stdio.h`, `string.h`).** The first CI build step
> (`cmake --preset ApeX && cmake --build build/ApeX`) died at the first
> `#include <stdio.h>`.

It is **already fixed**: a Copilot agent added `libnewlib-arm-none-eabi` to
the toolchain install step (commit `4d87e0a`, merged via PR #9), and the
latest run on `main` (`9ce4e30`) is **fully green** — both
`build-test-static` and `firmware` jobs pass.

**Current repo status: GREEN.** The red ❌ you saw was historical.

---

## Evidence trail (run IDs from the API)

| Run ID | Commit | Event | Conclusion | Meaning |
|---|---|---|---|---|
| `34209454372` | `5ca1efe` | push → main | **failure** | **The red ❌.** `firmware` job died at step *"Build firmware (ApeX full profile)"*. `build-test-static` passed. |
| `34209619327`, `34209826820` | `75e624f` / `4d87e0a` | pull_request (Copilot fix branch) | failure | Copilot's work-in-progress branch runs (one shows no jobs — a run-level failure before jobs started; immaterial, the branch merged) |
| *(current)* | `9ce4e30` | push → main | **success** | Fix merged (PR #9). `build-test-static` ✅ + `firmware` ✅ — the repo is green now. |

Step breakdown of the failing run (`34209454372`, job `firmware`):

```
Install ARM toolchain ................ success   (gcc-arm-none-eabi installed — but no newlib!)
Build firmware (ApeX full profile) ... FAILURE   (missing stdio.h / string.h at first compile)
Size gate / minimal / artifacts ...... skipped  (job aborted before them)
```

## Root cause, precisely

1. **My CI job's toolchain install was incomplete.** The workflow installed
   `gcc-arm-none-eabi` from apt, which is *compiler-only* — Debian/Ubuntu
   split the C library out into `libnewlib-arm-none-eabi`.
2. **Why it worked locally and not on CI:** this machine uses the official
   *Arm GNU Toolchain 14.3 rel1*, which bundles newlib. The gap only
   manifests on the runner — exactly the class of "builds on my machine"
   issue the CI job exists to catch. (I validated the workflow's *script
   syntax* locally, but could not validate the runner's *package set*.)
3. **Trigger timing:** the failure fired on `5ca1efe` — your merge of the
   local perf work with `origin/main` — because that was the first push
   containing the new `firmware` job.
4. **The fix:** install `libnewlib-arm-none-eabi` alongside the compiler
   (`4d87e0a`, PR #9, merged → green on `9ce4e30`).

## Additional findings (secondary, worth knowing)

1. **Your local clone is 3 commits behind `origin/main`.** The fix commit
   lives only on the remote right now. Run
   `git pull --no-rebase` (you use merge-style sync) before your next
   session so the fixed workflow is on disk — otherwise a future edit to
   `ci.yml` from the stale copy would silently re-break CI.
2. **The workflow was renamed to `name: Sean, N7SIX`.** The Copilot agent
   (or the merge) replaced the previous `name: CI` with your author string —
   it now shows up as the run name in the Actions tab. Purely cosmetic but
   confusing; recommend renaming it back (e.g. `name: CI`) in a follow-up.
3. **Duplicated commit in history:** `ccf2c9f` and `6ceaebf` carry the
   identical message ("perf: defer flash writes…") from the merge dance.
   Harmless, but avoid amend-then-merge patterns that duplicate commits.
4. **One historical red ❌ also appears on the Copilot fix branch's PR
   runs.** Those were work-in-progress states and are expected; only runs on
   `main` mark the repo's main branch status.
5. The commit-status endpoint reports `state: pending` for the latest
   `main` head — this is a leftover *status context* (not a check run); the
   actual check runs are both `success`. The green ✅ in the UI comes from
   the check runs.

## Prevention

- **Already in place:** the size gate + dual-profile firmware build (this
  exact failure proves the job pays for itself — it caught an
  environment-only compile break before any user did).
- **Recommended follow-ups (quick):**
  1. `git pull` to sync the fixed workflow locally.
  2. Rename the workflow back to `name: CI`.
  3. Optional: pin a known-good runner matrix
     (`ubuntu-latest` is fine; the package lesson is now encoded in the
     install step itself).

---

**Verdict:** single-cause failure (missing `libnewlib-arm-none-eabi` on the
runner), single-commit blast radius, already fixed and green on `main`.
No code regression — the firmware itself never failed to build; only the
runner's toolchain was incomplete.
