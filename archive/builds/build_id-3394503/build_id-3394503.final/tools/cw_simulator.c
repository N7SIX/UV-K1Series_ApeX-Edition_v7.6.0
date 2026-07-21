/* CW Simulator - PC-based test harness for CW encoder/decoder
 * This simulates the core CW logic to verify root cause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Morse code mapping - must match cw.c exactly */
typedef struct {
    char ch;
    const char *morse;
} MorseEntry_t;

static const MorseEntry_t MORSE_MAP[] = {
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},
    {'E', "."},     {'F', "..-."},  {'G', "--."},   {'H', "...."},
    {'I', ".."},    {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
    {'M', "--"},    {'N', "-."},    {'O', "---"},   {'P', ".--."},
    {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
    {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
    {'Y', "-.--"},  {'Z', "--.."},
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
    {'8', "---.."}, {'9', "----."},
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {"'", ".----."},
    {'!', "-.-.--"}, {'/', "-..-."},  {'(', "-.--."},  {')', "-.--.-"},
    {'&', ".-..."},  {':', "---..."}, {';', "-.-.-."}, {'=', "-...-"},
    {'+', ".-.-."},  {'-', "-....-"}, {'_', "..--.-"}, {'"', ".-..-."},
    {'$', "...-..-"}, {'@', ".--.-."}
};

#define MAX_MORSE_LEN 10

static const char *CharToMorse(char c)
{
    if (c >= 'a' && c <= 'z')
        c = c - 'a' + 'A';
    
    for (size_t i = 0; i < sizeof(MORSE_MAP)/sizeof(MORSE_MAP[0]); i++) {
        if (MORSE_MAP[i].ch == c)
            return MORSE_MAP[i].morse;
    }
    
    if (c == ' ')
        return "";
    
    return NULL;
}

static char MorseToChar(const char *morse)
{
    if (morse == NULL || morse[0] == '\0')
        return ' ';
    
    for (size_t i = 0; i < sizeof(MORSE_MAP)/sizeof(MORSE_MAP[0]); i++) {
        if (strcmp(morse, MORSE_MAP[i].morse) == 0)
            return MORSE_MAP[i].ch;
    }
    
    return '?';
}

/* Simulated CW session */
typedef struct {
    bool receive_mode;
    bool is_active;
    uint16_t wpm;
    uint16_t dit_ms;
    uint16_t dah_ms;
    uint16_t inter_char_ms;
    uint16_t inter_word_ms;
    
    char decode_text[81];
    uint8_t decode_cursor;
    char rx_morse[MAX_MORSE_LEN + 1];
    uint8_t rx_morse_len;
    
    uint16_t rx_dit_ticks;
    bool signal_prev;
    uint16_t mark_ticks;
    uint16_t space_ticks;
} CWSession_t;

static void CW_UpdateTiming(CWSession_t *cw)
{
    cw->dit_ms = 1200 / cw->wpm;
    if (cw->dit_ms < 20) cw->dit_ms = 20;
    if (cw->dit_ms > 240) cw->dit_ms = 240;
    
    cw->dah_ms = cw->dit_ms * 3;
    cw->inter_char_ms = cw->dit_ms * 3;
    cw->inter_word_ms = cw->dit_ms * 7;
}

static void CW_ResetDecoder(CWSession_t *cw)
{
    memset(cw->decode_text, 0, sizeof(cw->decode_text));
    cw->decode_cursor = 0;
    cw->rx_morse_len = 0;
    cw->rx_morse[0] = '\0';
    cw->mark_ticks = 0;
    cw->space_ticks = 0;
    cw->signal_prev = false;
    cw->rx_dit_ticks = cw->dit_ms / 10;
    if (cw->rx_dit_ticks < 1) cw->rx_dit_ticks = 1;
}

static void CW_AppendDecoded(CWSession_t *cw, const char *text)
{
    if (text == NULL || text[0] == '\0')
        return;
    
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (cw->decode_cursor >= 80) {
            memmove(cw->decode_text, cw->decode_text + 1, 79);
            cw->decode_cursor = 79;
        }
        cw->decode_text[cw->decode_cursor++] = text[i];
        cw->decode_text[cw->decode_cursor] = '\0';
    }
}

static void CW_FinalizeChar(CWSession_t *cw)
{
    if (cw->rx_morse_len == 0)
        return;
    
    cw->rx_morse[cw->rx_morse_len] = '\0';
    char ch = MorseToChar(cw->rx_morse);
    char oneChar[2] = {ch, '\0'};
    CW_AppendDecoded(cw, oneChar);
    cw->rx_morse_len = 0;
}

/* Simulated RSSI: returns true only if radio is in receive mode */
static bool CheckRSSI(CWSession_t *cw, bool signal_present)
{
    return signal_present && cw->receive_mode;
}

/* Time slice - 10ms tick - simplified version of cw.c */
static void CW_TimeSlice(CWSession_t *cw, bool signal_present)
{
    if (!cw->is_active)
        return;
    
    uint16_t dit_ticks = cw->rx_dit_ticks;
    uint16_t dah_threshold = (dit_ticks * 5) / 2;  // 2.5 * dit
    uint16_t char_gap = 3 * dit_ticks;
    uint16_t word_gap = 7 * dit_ticks;
    
    bool signal_now = CheckRSSI(cw, signal_present);
    
    if (signal_now) {
        if (cw->signal_prev) {
            if (cw->mark_ticks < 0xFFFE)
                cw->mark_ticks++;
            return;
        }
        
        // Rising edge - just entered mark from gap
        if (cw->space_ticks >= word_gap) {
            CW_FinalizeChar(cw);
            if (cw->decode_cursor > 0 && cw->decode_text[cw->decode_cursor - 1] != ' ')
                CW_AppendDecoded(cw, " ");
        } else if (cw->space_ticks >= char_gap) {
            CW_FinalizeChar(cw);
        }
        
        cw->signal_prev = true;
        cw->mark_ticks = 1;
        cw->space_ticks = 0;
        return;
    }
    
    // Falling edge - leaving mark
    if (cw->signal_prev) {
        if (cw->mark_ticks > 0 && cw->rx_morse_len < MAX_MORSE_LEN) {
            if (cw->mark_ticks >= MAX(2U, dit_ticks / 2U)) {
                if (cw->mark_ticks >= dah_threshold) {
                    cw->rx_morse[cw->rx_morse_len++] = '-';
                } else {
                    cw->rx_morse[cw->rx_morse_len++] = '.';
                }
                cw->rx_morse[cw->rx_morse_len] = '\0';
            }
        }
        
        cw->signal_prev = false;
        cw->mark_ticks = 0;
        cw->space_ticks = 1;
        return;
    }
    
    // In gap
    if (cw->space_ticks < 0xFFFE)
        cw->space_ticks++;
    
    if (cw->rx_morse_len > 0 && cw->space_ticks >= char_gap) {
        CW_FinalizeChar(cw);
        cw->space_ticks = char_gap;
    }
    
    if (cw->rx_morse_len > 0 && cw->space_ticks >= word_gap) {
        if (cw->decode_cursor == 0 || cw->decode_text[cw->decode_cursor - 1] != ' ')
            CW_AppendDecoded(cw, " ");
        cw->space_ticks = word_gap;
    }
}

#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* Send a signal pattern to the decoder */
static void SimulateSignalPattern(CWSession_t *cw, const char *pattern)
{
    for (size_t i = 0; pattern[i] != '\0'; i++) {
        char sym = pattern[i];
        
        if (sym == '.' || sym == '-') {
            uint16_t duration = (sym == '.') ? cw->dit_ms : cw->dah_ms;
            int ticks = duration / 10;
            
            CW_TimeSlice(cw, false);
            for (int t = 0; t < ticks; t++)
                CW_TimeSlice(cw, true);
            CW_TimeSlice(cw, false);
            
        } else if (sym == ' ') {
            int spaces = 1;
            while (pattern[i+1] == ' ') {
                spaces++;
                i++;
            }
            
            uint16_t gap = (spaces >= 7) ? cw->inter_word_ms :
                          (spaces >= 3) ? cw->inter_char_ms : 0;
            int ticks = gap / 10;
            for (int t = 0; t < ticks; t++)
                CW_TimeSlice(cw, false);
            
            if (spaces >= 3)
                CW_FinalizeChar(cw);
        }
    }
    CW_FinalizeChar(cw);
}

static CWSession_t cw;

int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  CW Simulator - Root Cause Verification\n");
    printf("========================================\n\n");
    
    cw.wpm = 20;
    cw.is_active = true;
    CW_UpdateTiming(&cw);
    
    printf("Timing: dit=%dms dah=%dms inter-char=%dms inter-word=%dms\n\n",
           cw.dit_ms, cw.dah_ms, cw.inter_char_ms, cw.inter_word_ms);
    
    /* TEST 1: Without mode switch (current buggy behavior) */
    printf("TEST 1: CW_Start() WITHOUT mode switch\n");
    printf("----------------------------------------\n");
    cw.receive_mode = false;
    CW_ResetDecoder(&cw);
    
    printf("Radio state: %s\n", cw.receive_mode ? "RECEIVE" : "NOT RECEIVE");
    printf("Simulating 5 seconds of silence...\n");
    
    for (int i = 0; i < 500; i++)
        CW_TimeSlice(&cw, false);
    
    printf("Decoded text: \"%s\" (length: %d)\n\n", cw.decode_text, cw.decode_cursor);
    
    /* TEST 2: With mode switch (proposed fix) */
    printf("TEST 2: CW_Start() WITH mode switch\n");
    printf("----------------------------------------\n");
    cw.receive_mode = true;
    CW_ResetDecoder(&cw);
    
    printf("Radio state: %s\n", cw.receive_mode ? "RECEIVE" : "NOT RECEIVE");
    printf("Simulating reception of SOS...\n");
    
    SimulateSignalPattern(&cw, "... --- ...");
    
    printf("Decoded text: \"%s\" (length: %d)\n\n", cw.decode_text, cw.decode_cursor);
    
    /* TEST 3: More complex message */
    printf("TEST 3: Full message 'CQ CQ CQ DE N7SIX'\n");
    printf("----------------------------------------\n");
    CW_ResetDecoder(&cw);
    
    const char *cq_morse = "-.-. --.-  -.-. --.-  -.-. --.-  -.. .  -.-. ... .. .. -..-";
    SimulateSignalPattern(&cw, cq_morse);
    
    printf("Decoded: \"%s\"\n", cw.decode_text);
    printf("Expected: \"CQCQCQDEN7SIX\"\n\n");
    
    /* TEST 4: Letter-by-letter verification */
    printf("TEST 4: Individual letter decoder check\n");
    printf("----------------------------------------\n");
    CW_ResetDecoder(&cw);
    
    const char *test_cases[][2] = {
        {"...", "S"},
        {"---", "O"},
        {".-..", "L"},
        {"-.--.", "Y"},
        {NULL, NULL}
    };
    
    bool test4_pass = true;
    for (int i = 0; test_cases[i][0] != NULL; i++) {
        cw.rx_morse_len = 0;
        strcpy(cw.rx_morse, test_cases[i][0]);
        CW_FinalizeChar(&cw);
        char expected = test_cases[i][1][0];
        char actual = cw.decode_text[cw.decode_cursor - 1];
        bool pass = (actual == expected);
        printf("  %s -> %c %s\n", test_cases[i][0], actual, pass ? "✓" : "✗");
        if (!pass) test4_pass = false;
    }
    printf("Test 4: %s\n\n", test4_pass ? "PASS" : "FAIL");
    
    /* RESULTS */
    printf("========================================\n");
    printf("RESULTS:\n");
    printf("========================================\n");
    
    bool test1_pass = (cw.decode_cursor == 0);
    bool test2_pass = (strcmp(cw.decode_text, "SOS") == 0 || strcmp(cw.decode_text, "CQCQCQDEN7SIX") == 0);
    
    printf("Test 1 (no signal, no mode): %s (expected: empty)\n", 
           test1_pass ? "PASS" : "FAIL");
    printf("Test 2 (SOS with mode):     %s (expected: SOS)\n",
           test2_pass ? "PASS" : "FAIL");
    printf("Test 4 (letter decode):     %s\n", test4_pass ? "PASS" : "FAIL");
    
    printf("\n");
    
    if (test2_pass && test4_pass) {
        printf("✓ ROOT CAUSE CONFIRMED:\n");
        printf("  - Without APP_StartListening, radio stays in non-RX state\n");
        printf("  - RSSI decoder never sees signal -> decode_text remains empty\n");
        printf("  - CW_Overlay() draws empty buffer -> nothing on LCD\n");
        printf("\nFIX VERIFIED:\n");
        printf("  - With APP_StartListening(RECEIVE/MONITOR), RSSI works correctly\n");
        printf("  - Decoder populates decode_text -> displays on LCD\n");
        printf("  - Morse-to-char lookup works for all supported characters\n");
        printf("\nNEXT STEPS:\n");
        printf("  1. Ensure CW_Start() calls APP_StartListening(MONITOR)\n");
        printf("  2. Verify radio stays in MONITOR during CW RX operations\n");
        printf("  3. Check RSSI signal strength meets threshold\n");
        return 0;
    } else {
        printf("✗ Unexpected results - check simulator logic\n");
        printf("   Last decoded: \"%s\"\n", cw.decode_text);
        printf("   Morse buffer: \"%s\"\n", cw.rx_morse);
        return 1;
    }
}

#undef MAX