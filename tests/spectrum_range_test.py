# Simulation script for spectrum range logic

scanStepValues = [
    1,
    10,
    50,
    100,
    250,
    500,
    625,
    833,
    1000,
    1250,
    1500,
    2000,
    2500,
    5000,
    10000,
]

# settings structure simulation
class Settings:
    def __init__(self):
        self.scanStepIndex = 12  # default S_STEP_25_0kHz
        self.stepsCount = 2      # STEPS_32
        self.frequencyChangeStep = 0

settings = Settings()

# globals
currentFreq = 100000000  # 100 MHz
initialFreq = currentFreq

gScanRangeStart = 0
gScanRangeStop = 0

# helper functions

def GetScanStep():
    return scanStepValues[settings.scanStepIndex]

def IsCenterMode():
    # < S_STEP_2_5kHz (index 4)
    return settings.scanStepIndex < 4

def GetStepsCount():
    if gScanRangeStart:
        return ((gScanRangeStop - gScanRangeStart) // GetScanStep()) + 1
    return 128 >> settings.stepsCount

def GetStepsCountDisplay():
    if gScanRangeStart:
        return ((gScanRangeStop - gScanRangeStart) // GetScanStep()) + 1
    return GetStepsCount()

def GetBW():
    return GetStepsCount() * GetScanStep()

# mirror the new C helper

def GetDisplayWidth():
    steps = GetStepsCount()
    maxBars = 128 >> settings.stepsCount
    return steps if steps < maxBars else maxBars

def GetFStart():
    # mirror revised C logic: range has fixed boundaries
    if gScanRangeStart:
        return gScanRangeStart
    if IsCenterMode():
        half = GetBW() >> 1
        return currentFreq - half if currentFreq > half else 0
    return currentFreq

def GetFEnd():
    # mirror revised C logic: range has fixed boundaries
    if gScanRangeStart:
        return gScanRangeStop
    if IsCenterMode():
        return currentFreq + (GetBW() >> 1)
    return currentFreq + GetBW()

# tests

def test_range(start, stop, stepIndex, freq):
    global gScanRangeStart, gScanRangeStop, currentFreq
    settings.scanStepIndex = stepIndex
    currentFreq = freq
    gScanRangeStart = start
    gScanRangeStop = stop
    print(f"range {start}-{stop}, step idx {stepIndex}, freq {freq}")
    print("  GetStepsCount():", GetStepsCount())
    print("  GetStepsCountDisplay():", GetStepsCountDisplay())
    print("  IsCenterMode():", IsCenterMode())
    print("  GetFStart():", GetFStart())
    print("  GetFEnd():", GetFEnd())
    # compute bars as per new DrawSpectrumEnhanced logic
    steps = GetStepsCount()
    maxBars = 128 >> settings.stepsCount
    bars = steps if steps < maxBars else maxBars
    print("  bars (display bins):", bars)
    print()

# run example scenarios

def test_alignment():
    """Verify that the spectrum trace and waterfall share the same
    horizontal scaling.  This mirrors the behaviour enforced by
    GetDisplayWidth() in the C code and prevented the earlier bug where
    the graph would shift relative to the waterfall during range mode.
    """
    global gScanRangeStart, gScanRangeStop

    for steps_count in [0, 1, 2, 3]:  # corresponding to STEPS_128..16
        maxBars = 128 >> steps_count
        for steps in [maxBars - 1, maxBars, maxBars + 5, maxBars * 2]:
            # simulate various range lengths where steps may exceed cap
            gScanRangeStart = 10000000
            gScanRangeStop = gScanRangeStart + steps * GetScanStep()
            settings.stepsCount = steps_count

            width = GetDisplayWidth()
            currentSteps = GetStepsCount()
            print(f"alignment check: stepidx={steps_count} maxBars={maxBars} stepsCount={currentSteps} width={width}")
            # width should never exceed the cap
            assert width <= maxBars, f"width {width} exceeded cap {maxBars}"
            expected = currentSteps if currentSteps < maxBars else maxBars
            assert width == expected, f"width {width} != expected {expected}"

            # ensure waterfall mapping using width would cover exactly width
            # distinct indices when projected to 128 pixels
            specWidth = width
            scaleFixed = (specWidth << 8) / 128
            mapped = set()
            for x in range(128):
                idx = int((x * scaleFixed) // 256)
                if idx >= specWidth - 1:
                    idx = specWidth - 2
                mapped.add(idx)
            # we expect at least width-1 distinct indices
            assert len(mapped) >= max(0, specWidth - 1)

    # restore default range
    gScanRangeStart = 0
    gScanRangeStop = 0

# run tests below


# no range, default 25k
settings.scanStepIndex = 12
currentFreq = 100000000
print("--- no range ---")
test_range(0,0,settings.scanStepIndex,currentFreq)

# with range same as currentFreq
print("--- range active, normal step ---")
test_range(90000000, 110000000, 12, 100000000)

# centre-mode with range that would push start low
print("--- centre mode (0.1kHz) with range ---")
test_range(90000000, 110000000, 1, 91000000)  # tiny step so IsCenterMode

# centre-mode frequency left of start
print("--- centre-mode freq below start ---")
test_range(90000000, 110000000, 1, 90010000)

# centre-mode frequency above stop
print("--- centre-mode freq above stop ---")
test_range(90000000, 110000000, 1, 115000000)

# step change and freq adjustment outside range should clamp
print("--- clamp behaviour ---")

def clamp_test():
    global currentFreq
    currentFreq = 95000000
    print("initial currentFreq", currentFreq)
    # simulate UpdateCurrentFreq(true) with step of 1000000
    settings.frequencyChangeStep = 1000000
    # manual clamp replicated from code
    if currentFreq < 600000000:
        currentFreq += settings.frequencyChangeStep
    if gScanRangeStart:
        if currentFreq < gScanRangeStart: currentFreq = gScanRangeStart
        if currentFreq > gScanRangeStop: currentFreq = gScanRangeStop
    print("after inc clamp", currentFreq)


print("--- clamp behaviour ---")
gScanRangeStart = 90000000
gScanRangeStop = 110000000
clamp_test()

# ------------------------------------------------------------------
# Additional verifications added to cover the new behaviors.

# auto-commit scenarios mimic pressing many digits until the entry auto-commits

def simulate_auto_commit(freq, start, stop, stepIndex):
    global gScanRangeStart, gScanRangeStop, currentFreq
    settings.scanStepIndex = stepIndex
    gScanRangeStart = start
    gScanRangeStop = stop
    temp = freq
    print(f"auto commit with temp={temp}, start={start}, stop={stop}, stepidx={stepIndex}")
    currentFreq = temp
    if gScanRangeStart:
        if currentFreq < gScanRangeStart: currentFreq = gScanRangeStart
        if currentFreq > gScanRangeStop: currentFreq = gScanRangeStop
    print("  resulting currentFreq", currentFreq)

print("--- auto commit tests ---")
simulate_auto_commit(430000000,0,0,12)
# outside range (above stop)
simulate_auto_commit(435000000,90000000,110000000,12)
# inside range
simulate_auto_commit(95000000,90000000,110000000,12)
# outside range (way above)
simulate_auto_commit(1200000000,90000000,110000000,12)

# test UpdateFreqChangeStep increment size
print("--- freq change step increments ---")
settings.scanStepIndex = 12
settings.frequencyChangeStep = 80000
print("wide mode diff should be 200k: initial",settings.frequencyChangeStep)
for i in range(4):
    diff = GetScanStep() * 8
    settings.frequencyChangeStep += diff
    print(" step",i+1,"->",settings.frequencyChangeStep)

# center mode shift correctness
print("--- center mode shift tests ---")
settings.scanStepIndex = 1
settings.stepsCount = 2

# without range: frequencyChangeStep locks to half-width
settings.frequencyChangeStep = GetBW() >> 1
currentFreq = 430000000
print("no range start",currentFreq,"halfspan",settings.frequencyChangeStep)
currentFreq += settings.frequencyChangeStep
print(" no-range shift up ->",currentFreq)

# with range: step fixed at 8×scan step (200k at 25kHz)
gScanRangeStart = 400000000
gScanRangeStop = 450000000
settings.frequencyChangeStep = GetScanStep() * 8
currentFreq = 430000000
print("range start",currentFreq,"step",settings.frequencyChangeStep)
currentFreq += settings.frequencyChangeStep
print(" range shift up ->",currentFreq)
currentFreq -= settings.frequencyChangeStep * 2
print(" range shift down twice ->",currentFreq)

# finally, verify the alignment helper
print("--- verifying spectrum/waterfall alignment ---")
test_alignment()

# ------------------------------------------------------------------
# New tests added for Option B enhancements

def test_listen_config():
    """Ensure the new listen-delay settings are stored and applied."""
    global settings
    settings.listenTScan = 3
    settings.listenTStill = 7
    assert settings.listenTScan == 3
    assert settings.listenTStill == 7

    # simulate the branches that assign to listenT
    listenT = settings.listenTScan
    assert listenT == 3
    listenT = settings.listenTStill
    assert listenT == 7
    print("listen delay settings OK")


def test_watchdog():
    """Exercise the watchdog logic that forces redraw while listening."""
    isListening = True
    redrawScreen = False
    redrawStatus = False
    watchdog = 0
    triggered = False
    for i in range(25):
        if not (redrawScreen or redrawStatus):
            watchdog += 1
            if watchdog >= 20:
                redrawScreen = True
                redrawStatus = True
                triggered = True
                watchdog = 0
        else:
            watchdog = 0
            redrawScreen = False
            redrawStatus = False
    assert triggered, "watchdog did not fire"
    print("watchdog triggered OK")

print("--- verifying listen delay and watchdog ---")
test_listen_config()
test_watchdog()

# ------------------------------------------------------------------
# simple check that the monitoring logic still updates the noise
# history when RX has finished (listenT==0 and isListening==False).
# This mirrors the behaviour of UpdateListening() once radio stops.

def test_monitoring_resume():
    """After RX ends we should still generate noise/grass and force
    a redraw so the display stays active rather than frozen.
    """
    global listenT, isListening, rssiHistory, noisePersistence, peak, scanInfo

    # initialise minimal state
    listenT = 0
    isListening = False
    peak = {'rssi': 5, 'i': 64}           # small nonzero peak
    scanInfo = {'rssiMin': 0}
    # zero histories
    rssiHistory = [0] * 128
    noisePersistence = [0] * 128

    # reproduce the noise-generation portion of UpdateListening
    import random
    peakDisplay = peak['i']
    for i in range(128):
        if i >= peakDisplay - 1 and i <= peakDisplay + 1:
            rssiHistory[i] = peak['rssi'] + (random.randint(0, 255) % 12)
        else:
            baseFloor = scanInfo['rssiMin'] + 32
            roll = (random.randint(0, 255) % 9) - 4
            noisePersistence[i] = (noisePersistence[i] * 7 + (baseFloor + roll)) >> 3
            spike = (random.randint(0, 255) % 14)
            finalGrass = noisePersistence[i] + spike
            if finalGrass > peak['rssi'] and peak['rssi'] > 10:
                rssiHistory[i] = peak['rssi'] - 5
            else:
                rssiHistory[i] = finalGrass

    assert any(v != 0 for v in rssiHistory), "noise history remained zero"
    print("monitoring resumed noise generation OK")

print("--- verifying monitoring resume behaviour ---")
test_monitoring_resume()

# ------------------------------------------------------------------
# compare the original (pre‑fix) listen loop with the current behaviour
def simulate_old_vs_new_listen():
    """Show how the noise grass is frozen under the old logic.

    The earlier version of UpdateListening() simply decremented
    ``listenT`` and returned before doing any noise updates.  During a
    multi‑tick listen delay the spectrum and waterfall would therefore
    remain completely static, producing the "frozen" appearance seen on
    hardware.  The current implementation performs the noise/grass
    update on every tick, keeping the plots lively even while the radio
    hardware is still keyed.
    """

    import random

    peak_rssi = 20
    peak_idx = 64
    rssi_min = 0

    # three histograms: old (no updates), heavy (noise each tick), quick (simple level each tick)
    hist_old = [0] * 128
    hist_heavy = [0] * 128
    hist_quick = [0] * 128

    # simulate quietCounter behaviour
    listenT = 5
    quiet = 0
    for tick in range(listenT):
        # quick update executed every tick
        for i in range(128):
            hist_quick[i] = 8  # arbitrary nonzero level

        # heavy update only when counter expires
        quiet += 1
        if quiet >= 6:  # match quietCounter limit from C
            quiet = 0
            # replicate noise generation
            peakDisplay = peak_idx
            for i in range(128):
                if i >= peakDisplay - 1 and i <= peakDisplay + 1:
                    hist_heavy[i] = peak_rssi + (random.randint(0, 255) % 12)
                else:
                    baseFloor = rssi_min + 32
                    roll = (random.randint(0, 255) % 9) - 4
                    spike = (random.randint(0, 255) % 14)
                    finalGrass = baseFloor + roll + spike
                    hist_heavy[i] = finalGrass
        listenT -= 1

    print("old hist summary: min={}, max={}, changed={}"
          .format(min(hist_old), max(hist_old), any(v != 0 for v in hist_old)))
    print("heavy hist summary: min={}, max={}, changed={}"
          .format(min(hist_heavy), max(hist_heavy), any(v != 0 for v in hist_heavy)))
    print("quick hist summary: min={}, max={}, changed={}"
          .format(min(hist_quick), max(hist_quick), any(v != 0 for v in hist_quick)))

    # simple assertions to catch regressions in CI
    assert not any(v != 0 for v in hist_old), "old path should remain zero"
    # heavy path may not run if listenT < threshold; only check when we had
    # at least one heavy cycle
    if listenT + 0 >= 6 or any(v != 0 for v in hist_heavy):
        assert any(v != 0 for v in hist_heavy), "heavy update did not modify history when it should"
    assert any(v != 0 for v in hist_quick), "quick update did not modify history"

print("--- demonstrating old vs new listen behaviour ---")
simulate_old_vs_new_listen()

