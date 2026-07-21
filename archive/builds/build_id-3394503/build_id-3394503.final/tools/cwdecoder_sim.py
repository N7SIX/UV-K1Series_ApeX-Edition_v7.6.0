#!/usr/bin/env python3
"""
CW Decoder Simulation
Simulates CW_Decoder_ProcessTick() behavior with synthetic tone state input
to verify decoder logic processes "CQ" correctly.
"""

import sys

# Decoder state (mirrors cwdecoder.c)
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
gCW_RxTraceClock = 0
gCW_RxToneState = False
gCW_RxToneOnDebounce = 0
gCW_RxToneOffDebounce = 0
gCW_PeakRssi = -110
gCW_RxNoiseFloor = -120
gCW_RxSignalFloor = -110
gCW_StartupDelay = False
gCW_TraceHead = 0

# State machine states
CW_RX_STATE_IDLE = 0
CW_RX_STATE_MARK = 1
CW_RX_STATE_GAP = 2
gCW_RxDecoderState = CW_RX_STATE_IDLE

# CW timing (20 WPM)
gCW_DitMs = 60

CW_RX_DEBOUNCE_TICKS = 1
CW_RX_ACTIVATE_TICKS = 5
CW_RX_DEACTIVATE_TICKS = 3

# Morse code map
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
        return  # too short
    
    if gCW_RxMorseLen < 10:
        if markTicks >= (ditTicks * 2):
            gCW_RxMorse += '-'
        else:
            gCW_RxMorse += '.'
        gCW_RxMorseLen = len(gCW_RxMorse)


def CW_FinalizeRxCharacter():
    global gCW_RxMorseLen, gCW_DecodeText, gCW_DecodeCursor, gCW_RxMorse
    
    if gCW_RxMorseLen == 0:
        return
    
    token = CW_MorseToDecodedToken(gCW_RxMorse)
    if token and token[0] != '?':
        gCW_DecodeText += token
    
    gCW_RxMorse = ""
    gCW_RxMorseLen = 0


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


def CW_Decoder_ProcessTick(tonePresent):
    global gCW_RxToneState, gCW_RxActive, gCW_RxSignalPrev
    global gCW_RxMarkTicks, gCW_RxSpaceTicks, gCW_RxMorseLen, gCW_RxMorse
    global gCW_DecodeText, gCW_DecodeCursor, gCW_RxDecoderState
    global gCW_RxActiveDebounce, gCW_RxInactiveDebounce
    
    # Startup delay
    if gCW_StartupDelay:
        return
    
    # Use provided tone state directly (bypassing RSSI detection)
    gCW_RxToneState = tonePresent
    toneNow = gCW_RxToneState
    CW_HandleRxActivation(toneNow)
    
    # --- Timing derived from WPM setting only ---
    ditTicks = max(1, (gCW_DitMs + 5) // 10)
    charGapTicks = 3 * ditTicks
    wordGapTicks = 7 * ditTicks
    
    # --- Signal RISE (tone ON, was OFF → start timing a mark) ---
    if toneNow:
        if gCW_RxSignalPrev:
            # Continue counting an existing mark
            if gCW_RxMarkTicks < 0xFFFE:
                gCW_RxMarkTicks += 1
            return
        
        # Transition: was silent, now tone → check if preceding gap was a character/word boundary
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
    
    # --- Signal FALL (tone OFF, was ON → classify the mark) ---
    if gCW_RxSignalPrev:
        if gCW_RxMarkTicks > 0 and gCW_RxMorseLen < 10:
            CW_ClassifyMark(gCW_RxMarkTicks, ditTicks)
        
        gCW_RxSignalPrev = False
        gCW_RxMarkTicks = 0
        gCW_RxSpaceTicks = 1
        gCW_RxDecoderState = CW_RX_STATE_GAP
        return
    
    # --- GAP (tone OFF, was OFF → accumulate space, check for char/word boundaries) ---
    if gCW_RxDecoderState == CW_RX_STATE_GAP:
        if gCW_RxSpaceTicks < 0xFFFE:
            gCW_RxSpaceTicks += 1
        
        if gCW_RxMorseLen > 0 and gCW_RxSpaceTicks >= charGapTicks:
            CW_FinalizeRxCharacter()
            gCW_RxSpaceTicks = charGapTicks  # prevent re-finalization
        
        if gCW_RxMorseLen > 0 and gCW_RxSpaceTicks >= wordGapTicks:
            if gCW_DecodeCursor == 0 or gCW_DecodeText[gCW_DecodeCursor - 1] != ' ':
                CW_AppendDecodedText(" ")
            gCW_RxSpaceTicks = wordGapTicks


def simulate_cq():
    """Simulate receiving 'CQ' at 20 WPM"""
    print("Simulating CW reception of 'CQ' at 20 WPM...")
    print(f"ditMs = {gCW_DitMs}ms, ditTicks (at 10ms/tick) = {max(1, (gCW_DitMs + 5) // 10)}")
    
    # CW timing at 20 WPM (10ms ticks):
    # dit = 6 ticks, dah = 18 ticks
    # intra-char gap = 6 ticks (between elements within a letter)
    # inter-char gap = 18 ticks (between letters)
    # word gap = 42 ticks (between words)
    
    dit_ticks = 6
    dah_ticks = 18
    intra_char_gap = 6   # gap between elements within a letter
    inter_char_gap = 18  # gap between letters
    word_gap = 42        # gap between words
    
    # Build signal timeline: list of (tone_present, duration_ticks)
    # C = -.-. (dah-dit-dah-dit)
    # Q = --.- (dah-dah-dit-dah)
    signal_pattern = []
    
    # C: -.-.
    signal_pattern += [(True, dah_ticks)]      # dah
    signal_pattern += [(False, intra_char_gap)]  # gap
    signal_pattern += [(True, dit_ticks)]       # dit
    signal_pattern += [(False, intra_char_gap)]  # gap
    signal_pattern += [(True, dah_ticks)]       # dah
    signal_pattern += [(False, intra_char_gap)]  # gap
    signal_pattern += [(True, dit_ticks)]       # dit
    
    # Inter-character gap
    signal_pattern += [(False, inter_char_gap)]  # gap
    
    # Q: --.-
    signal_pattern += [(True, dah_ticks)]       # dah
    signal_pattern += [(False, intra_char_gap)]  # gap
    signal_pattern += [(True, dah_ticks)]       # dah
    signal_pattern += [(False, intra_char_gap)]  # gap
    signal_pattern += [(True, dit_ticks)]       # dit
    signal_pattern += [(False, intra_char_gap)]  # gap
    signal_pattern += [(True, dah_ticks)]       # dah
    signal_pattern += [(False, inter_char_gap)]  # final gap to finalize last char
    
    # Run simulation
    tick = 0
    decoded_chars = []
    
    for tone_present, duration in signal_pattern:
        for i in range(duration):
            prev_text = gCW_DecodeText
            prev_morse = gCW_RxMorse
            CW_Decoder_ProcessTick(tone_present)
            
            if gCW_DecodeText != prev_text or (gCW_RxMorseLen > 0 and gCW_RxMorseLen != len(prev_morse)):
                decoded_chars.append(f"tick={tick}: '{gCW_DecodeText}' (morse='{gCW_RxMorse}', state={gCW_RxDecoderState})")
            tick += 1
    
    # Finalize any remaining character
    print(f"Before finalize: '{gCW_DecodeText}', morse='{gCW_RxMorse}', len={gCW_RxMorseLen}")
    CW_FinalizeRxCharacter()
    print(f"After finalize: '{gCW_DecodeText}'")
    if decoded_chars:
        last_decoded = decoded_chars[-1].split(":")[1].strip().strip("'")
        if gCW_DecodeText != last_decoded:
            decoded_chars.append(f"tick={tick}: '{gCW_DecodeText}' (finalize)")
    
    print(f"\nTotal ticks simulated: {tick}")
    print(f"Final decoded text: '{gCW_DecodeText}'")
    print(f"Decoded progression:")
    for change in decoded_chars:
        print(f"  {change}")
    
    # Verify
    if gCW_DecodeText == "CQ":
        print("\nSUCCESS: Decoder correctly decoded 'CQ'")
        return True
    else:
        print(f"\nFAIL: Expected 'CQ', got '{gCW_DecodeText}'")
        return False


if __name__ == "__main__":
    success = simulate_cq()
    sys.exit(0 if success else 1)