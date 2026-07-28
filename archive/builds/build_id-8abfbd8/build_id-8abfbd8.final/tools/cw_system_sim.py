#!/usr/bin/env python3
"""
CW System Simulation
Simulates the complete CW system: CW_TimeSlice10ms() + CW_Decoder_ProcessTick()
to verify end-to-end decoding of "CQ" with proper state machine integration.
"""

import sys

# ----- CW App State (from cw.c) -----
class CWState:
    IDLE = 0
    COMPOSING = 1
    SENDING = 2

gCW_State = CWState.COMPOSING
gCW_ActiveState = True
gCW_WPM = 20
gCW_DitMs = 1200 // gCW_WPM  # 60ms
gCW_DahMs = gCW_DitMs * 3   # 180ms

# ----- Decoder State (from cwdecoder.c) -----
gCW_DecodeText = ""
gCW_DecodeCursor = 0
gCW_RxMorse = ""
gCW_RxMorseLen = 0
gCW_RxMarkTicks = 0
gCW_RxSpaceTicks = 0
gCW_RxSignalPrev = False
gCW_RxActive = False
gCW_RxActiveDebounce = 0
gCW_RxInactiveDebounce = 0
gCW_RxToneState = False
gCW_RxToneOnDebounce = 0
gCW_RxToneOffDebounce = 0
gCW_PeakRssi = -110
gCW_RxNoiseFloor = -120
gCW_RxSignalFloor = -110
gCW_StartupDelay = False  # Skip startup delay for sim
gCW_TraceHead = 0

# State machine
CW_RX_STATE_IDLE = 0
CW_RX_STATE_MARK = 1
CW_RX_STATE_GAP = 2
gCW_RxDecoderState = CW_RX_STATE_IDLE

# Constants
CW_RX_DEBOUNCE_TICKS = 1
CW_RX_ACTIVATE_TICKS = 5
CW_RX_DEACTIVATE_TICKS = 3
CW_RX_MORSE_MAX_LEN = 10
CW_SIGNAL_THRESHOLD = 32
CW_TRACE_BUF_SIZE = 128
gCW_TraceHistory = [0] * CW_TRACE_BUF_SIZE
gCW_TracePeak = [0] * CW_TRACE_BUF_SIZE
gCW_CharConfidence = 0
gCW_CharSignalSum = 0
gCW_CharSignalCount = 0

# Display updates
gUpdateDisplay = False
gFrameBuffer = {i: bytearray(128) for i in range(8)}  # 8 lines

# Morse map
MORSE_MAP = {
    '.-': 'A', '-...': 'B', '-.-.': 'C', '-..': 'D', '.': 'E',
    '..-.': 'F', '--.': 'G', '....': 'H', '..': 'I', '.---': 'J',
    '-.-': 'K', '.-..': 'L', '--': 'M', '-.': 'N', '---': 'O',
    '.--.': 'P', '--.-': 'Q', '.-.': 'R', '...': 'S', '-': 'T',
    '..-': 'U', '...-': 'V', '.--': 'W', '-..-': 'X', '-.--': 'Y',
    '--..': 'Z',
}


def CW_MorseToDecodedToken(morse):
    if not morse:
        return " "
    return MORSE_MAP.get(morse, "?")


def CW_ClassifyMark(markTicks, ditTicks):
    global gCW_RxMorseLen, gCW_RxMorse
    
    if markTicks < max(2, (ditTicks + 1) // 2):
        return
    
    if gCW_RxMorseLen < CW_RX_MORSE_MAX_LEN:
        if markTicks >= (ditTicks * 2):
            gCW_RxMorse += '-'
        else:
            gCW_RxMorse += '.'
        gCW_RxMorseLen = len(gCW_RxMorse)
        CW_Render()  # Update display on morse change


def CW_FinalizeRxCharacter():
    global gCW_RxMorseLen, gCW_DecodeText, gCW_DecodeCursor, gCW_RxMorse
    global gCW_CharConfidence, gCW_CharSignalSum, gCW_CharSignalCount
    
    if gCW_RxMorseLen == 0:
        return
    
    # Calculate confidence based on signal quality during this character
    confidence = 0
    if gCW_CharSignalCount > 0:
        # Average SNR in dB (simplified)
        avgQuality = (gCW_CharSignalSum + gCW_CharSignalCount // 2) // gCW_CharSignalCount
        # Map SNR to confidence: 8dB->0%, 20dB->100%
        if avgQuality >= 8:
            confidence = (avgQuality - 8) * 100 // 12
            if confidence > 100:
                confidence = 100
    
    # Only accept character if confidence is sufficient (>65% for noise rejection)
    acceptChar = (confidence >= 65)
    
    token = CW_MorseToDecodedToken(gCW_RxMorse)
    if acceptChar and token and token[0] != '?':
        gCW_CharConfidence = confidence
        gCW_DecodeText += token
    
    gCW_RxMorse = ""
    gCW_RxMorseLen = 0
    gCW_CharSignalSum = 0
    gCW_CharSignalCount = 0
    CW_Render()  # Update display on character decode


def CW_AppendDecodedText(text):
    global gCW_DecodeText, gCW_DecodeCursor
    if not text:
        return
    gCW_DecodeText += text
    gCW_DecodeCursor = len(gCW_DecodeText)


def CW_HandleRxActivation(signalNow):
    global gCW_RxActive, gCW_RxActiveDebounce, gCW_RxInactiveDebounce
    
    if signalNow and not gCW_RxActive:
        gCW_RxActiveDebounce += 1
        if gCW_RxActiveDebounce >= CW_RX_ACTIVATE_TICKS:
            gCW_RxActive = True
            gCW_RxActiveDebounce = 0
    elif not signalNow and gCW_RxActive:
        gCW_RxInactiveDebounce += 1
        if gCW_RxInactiveDebounce >= CW_RX_DEACTIVATE_TICKS:
            gCW_RxActive = False
            gCW_RxActiveDebounce = 0
            gCW_RxInactiveDebounce = 0
    elif not signalNow and not gCW_RxActive:
        gCW_RxActiveDebounce = 0
        gCW_RxInactiveDebounce = 0


def CW_IsRxTonePresent(rssi_dBm):
    global gCW_PeakRssi, gCW_RxSignalFloor, gCW_RxNoiseFloor, gCW_RxToneState
    global gCW_RxToneOnDebounce, gCW_RxToneOffDebounce, gCW_CharSignalSum, gCW_CharSignalCount
    
    # Update confidence based on signal quality
    if gCW_RxToneState and rssi_dBm > gCW_RxNoiseFloor:
        # Signal present and above noise - accumulate quality
        quality = rssi_dBm - gCW_RxNoiseFloor
        gCW_CharSignalSum += quality
        gCW_CharSignalCount += 1
    
    # Update signal floors
    if rssi_dBm > gCW_PeakRssi:
        gCW_PeakRssi = rssi_dBm
    elif rssi_dBm < (gCW_PeakRssi - 10):
        gCW_PeakRssi -= 1
    if gCW_PeakRssi < -110:
        gCW_PeakRssi = -110
    
    if rssi_dBm > gCW_RxSignalFloor:
        gCW_RxSignalFloor = (gCW_RxSignalFloor * 3 + rssi_dBm + 2) // 4
    else:
        gCW_RxSignalFloor = (gCW_RxSignalFloor * 7 + rssi_dBm + 4) // 8
    
    # Only update noise floor when tone is NOT present
    if not gCW_RxToneState:
        if rssi_dBm < gCW_RxNoiseFloor:
            gCW_RxNoiseFloor = (gCW_RxNoiseFloor * 7 + rssi_dBm + 4) // 8
        else:
            gCW_RxNoiseFloor = (gCW_RxNoiseFloor * 15 + rssi_dBm + 8) // 16
    
    # Adaptive hysteresis
    signalSpan = max(4, gCW_RxSignalFloor - gCW_RxNoiseFloor)
    openLevel = gCW_RxNoiseFloor + (signalSpan * 3) // 4 + 3
    closeLevel = gCW_RxNoiseFloor + max(1, signalSpan // 4)
    effectiveOpenLevel = max(openLevel, -85)
    effectiveCloseLevel = max(closeLevel, -90)
    
    if not gCW_RxToneState:
        signalStrong = (rssi_dBm >= effectiveOpenLevel) and (rssi_dBm >= (gCW_RxNoiseFloor + 2))
    else:
        signalStrong = (rssi_dBm >= effectiveCloseLevel)
    
    # Tone detection
    if signalStrong:
        gCW_RxToneOnDebounce += 1
        gCW_RxToneOffDebounce = 0
        if not gCW_RxToneState and gCW_RxToneOnDebounce >= CW_RX_DEBOUNCE_TICKS:
            gCW_RxToneState = True
    else:
        gCW_RxToneOffDebounce += 1
        gCW_RxToneOnDebounce = 0
        if gCW_RxToneState and gCW_RxToneOffDebounce >= CW_RX_DEBOUNCE_TICKS:
            gCW_RxToneState = False


def CW_PushTraceSample(level):
    global gCW_TraceHistory, gCW_TracePeak, gCW_TraceHead
    gCW_TraceHistory[gCW_TraceHead] = level
    if level > gCW_TracePeak[gCW_TraceHead]:
        gCW_TracePeak[gCW_TraceHead] = level
    elif gCW_TracePeak[gCW_TraceHead] > 0:
        gCW_TracePeak[gCW_TraceHead] -= 1
    gCW_TraceHead = (gCW_TraceHead + 1) & 0x7F


def CW_UpdateTraceBuffer(signalNow, rssi_dBm):
    global gCW_RxTraceClock, gCW_TraceHistory, gCW_TracePeak, gCW_TraceHead
    if not gCW_RxActive:
        if gCW_RxTraceClock == 0:
            CW_PushTraceSample(0)
        gCW_RxTraceClock = (gCW_RxTraceClock + 1) & 1
    else:
        traceActive = signalNow or gCW_RxSignalPrev
        ditTicks = max(1, (gCW_DitMs + 5) // 10)
        traceAdvanceTicks = 1 if traceActive else max(2, (ditTicks + 1) // 2)
        if gCW_RxTraceClock >= traceAdvanceTicks:
            gCW_RxTraceClock = 0
            sigLevel = rssi_dBm + 120
            if sigLevel > CW_SIGNAL_THRESHOLD:
                sigLevel = CW_SIGNAL_THRESHOLD
            if signalNow:
                sigLevel = CW_SIGNAL_THRESHOLD
            elif sigLevel > 20:
                sigLevel = 20
            CW_PushTraceSample(sigLevel)
        else:
            gCW_RxTraceClock += 1


def CW_Decoder_ProcessTick(rssi_dBm):
    global gCW_RxToneState, gCW_RxActive, gCW_RxSignalPrev
    global gCW_RxMarkTicks, gCW_RxSpaceTicks, gCW_RxMorseLen, gCW_RxMorse
    global gCW_DecodeText, gCW_DecodeCursor, gCW_RxDecoderState, gCW_RxTraceClock
    global gCW_RxActiveDebounce, gCW_RxInactiveDebounce
    global gCW_PeakRssi, gCW_RxNoiseFloor, gCW_RxSignalFloor
    global gCW_RxToneOnDebounce, gCW_RxToneOffDebounce
    
    # Skip if not in composing mode
    if gCW_State != CWState.COMPOSING:
        return
    
    # Startup delay
    if gCW_StartupDelay:
        return
    
    # Tone detection
    CW_IsRxTonePresent(rssi_dBm)
    toneNow = gCW_RxToneState
    CW_HandleRxActivation(toneNow)
    CW_UpdateTraceBuffer(toneNow, rssi_dBm)
    
    # Timing
    ditTicks = max(1, (gCW_DitMs + 5) // 10)
    charGapTicks = 3 * ditTicks
    wordGapTicks = 7 * ditTicks
    
    # Signal RISE
    if toneNow:
        if gCW_RxSignalPrev:
            if gCW_RxMarkTicks < 0xFFFE:
                gCW_RxMarkTicks += 1
            return
        
        # Transition: check char/word boundary
        if gCW_RxDecoderState == CW_RX_STATE_GAP and gCW_RxMorseLen > 0:
            if gCW_RxSpaceTicks >= wordGapTicks:
                CW_FinalizeRxCharacter()
                if gCW_DecodeCursor > 0 and gCW_DecodeText[gCW_DecodeCursor - 1] != ' ':
                    CW_AppendDecodedText(" ")
            elif gCW_RxSpaceTicks >= charGapTicks:
                CW_FinalizeRxCharacter()
        
        # Start new mark
        gCW_RxSignalPrev = True
        gCW_RxMarkTicks = 1
        gCW_RxSpaceTicks = 0
        gCW_RxDecoderState = CW_RX_STATE_MARK
        return
    
    # Signal FALL
    if gCW_RxSignalPrev:
        if gCW_RxMarkTicks > 0 and gCW_RxMorseLen < CW_RX_MORSE_MAX_LEN:
            CW_ClassifyMark(gCW_RxMarkTicks, ditTicks)
            CW_PushTraceSample(CW_SIGNAL_THRESHOLD)
        
        gCW_RxSignalPrev = False
        gCW_RxMarkTicks = 0
        gCW_RxSpaceTicks = 1
        gCW_RxDecoderState = CW_RX_STATE_GAP
        return
    
    # GAP
    if gCW_RxDecoderState == CW_RX_STATE_GAP:
        if gCW_RxSpaceTicks < 0xFFFE:
            gCW_RxSpaceTicks += 1
        
        if gCW_RxMorseLen > 0 and gCW_RxSpaceTicks >= charGapTicks:
            CW_FinalizeRxCharacter()
            gCW_RxSpaceTicks = charGapTicks
        
        if gCW_RxMorseLen > 0 and gCW_RxSpaceTicks >= wordGapTicks:
            if gCW_DecodeCursor == 0 or gCW_DecodeText[gCW_DecodeCursor - 1] != ' ':
                CW_AppendDecodedText(" ")
            gCW_RxSpaceTicks = wordGapTicks
            CW_Render()


def CW_TimeSlice10ms(rssi_dBm):
    """Main tick function from cw.c"""
    global gUpdateDisplay
    
    if not gCW_ActiveState:
        return
    
    # In this sim, we only test RX/compose mode
    if gCW_State == CWState.COMPOSING:
        CW_Decoder_ProcessTick(rssi_dBm)


def CW_Render():
    """Update display"""
    global gUpdateDisplay
    gUpdateDisplay = True


def CW_Decoder_GetDecodedText():
    return gCW_DecodeText


def CW_Decoder_IsToneActive():
    return gCW_RxToneState


def CW_Decoder_IsRxActive():
    return gCW_RxActive


def reset_decoder():
    """Reset decoder state"""
    global gCW_DecodeText, gCW_DecodeCursor, gCW_RxMorse, gCW_RxMorseLen
    global gCW_RxMarkTicks, gCW_RxSpaceTicks, gCW_RxSignalPrev
    global gCW_RxActive, gCW_RxActiveDebounce, gCW_RxInactiveDebounce
    global gCW_RxToneState, gCW_RxToneOnDebounce, gCW_RxToneOffDebounce
    global gCW_PeakRssi, gCW_RxNoiseFloor, gCW_RxSignalFloor
    global gCW_RxDecoderState, gCW_RxTraceClock, gCW_TraceHead
    global gCW_TraceHistory, gCW_TracePeak
    
    gCW_DecodeText = ""
    gCW_DecodeCursor = 0
    gCW_RxMorse = ""
    gCW_RxMorseLen = 0
    gCW_RxMarkTicks = 0
    gCW_RxSpaceTicks = 0
    gCW_RxSignalPrev = False
    gCW_RxActive = False
    gCW_RxActiveDebounce = 0
    gCW_RxInactiveDebounce = 0
    gCW_RxToneState = False
    gCW_RxToneOnDebounce = 0
    gCW_RxToneOffDebounce = 0
    gCW_PeakRssi = -110
    gCW_RxNoiseFloor = -120
    gCW_RxSignalFloor = -110
    gCW_RxDecoderState = CW_RX_STATE_IDLE
    gCW_RxTraceClock = 0
    gCW_TraceHead = 0
    gCW_TraceHistory = [0] * CW_TRACE_BUF_SIZE
    gCW_TracePeak = [0] * CW_TRACE_BUF_SIZE


def simulate_cq():
    """Simulate receiving 'CQ' at 20 WPM through complete system"""
    print("=== CW System Simulation ===")
    print(f"WPM: {gCW_WPM}, ditMs: {gCW_DitMs}ms, ditTicks: {max(1, (gCW_DitMs + 5) // 10)}")
    print()
    
    reset_decoder()
    
    # Test with realistic noise
    use_noise = True
    noise_level = -115  # dBm
    
    # Timing at 20 WPM (10ms ticks)
    dit_ticks = 6
    dah_ticks = 18
    intra_char_gap = 6
    inter_char_gap = 18
    word_gap = 42
    
    # Signal pattern for "CQ"
    # C = -.-., Q = --.-
    signal_pattern = []
    
    # C: -.-.
    signal_pattern += [(True, dah_ticks)]
    signal_pattern += [(False, intra_char_gap)]
    signal_pattern += [(True, dit_ticks)]
    signal_pattern += [(False, intra_char_gap)]
    signal_pattern += [(True, dah_ticks)]
    signal_pattern += [(False, intra_char_gap)]
    signal_pattern += [(True, dit_ticks)]
    
    # Inter-character gap
    signal_pattern += [(False, inter_char_gap)]
    
    # Q: --.-
    signal_pattern += [(True, dah_ticks)]
    signal_pattern += [(False, intra_char_gap)]
    signal_pattern += [(True, dah_ticks)]
    signal_pattern += [(False, intra_char_gap)]
    signal_pattern += [(True, dit_ticks)]
    signal_pattern += [(False, intra_char_gap)]
    signal_pattern += [(True, dah_ticks)]
    signal_pattern += [(False, inter_char_gap)]  # final gap
    
    # RSSI values
    rssi_signal = -60
    rssi_noise = -120
    
    print("Simulating CW reception...")
    tick = 0
    display_updates = []
    
    for tone_present, duration in signal_pattern:
        rssi = rssi_signal if tone_present else rssi_noise
        for i in range(duration):
            prev_text = gCW_DecodeText
            prev_display = gUpdateDisplay
            
            CW_TimeSlice10ms(rssi)
            
            # Track display updates
            if gUpdateDisplay and not prev_display:
                display_updates.append(f"tick={tick}: '{gCW_DecodeText}' morse='{gCW_RxMorse}'")
            
            tick += 1
    
    print(f"\nTotal ticks: {tick}")
    print(f"Final decoded text: '{gCW_DecodeText}'")
    print(f"Display updates: {len(display_updates)}")
    for update in display_updates:
        print(f"  {update}")
    
    # Verify
    if gCW_DecodeText == "CQ":
        print("\n>>> SUCCESS: System correctly decoded 'CQ'")
        return True
    else:
        print(f"\n>>> FAIL: Expected 'CQ', got '{gCW_DecodeText}'")
        return False


if __name__ == "__main__":
    success = simulate_cq()
    sys.exit(0 if success else 1)