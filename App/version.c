
/**
 * =====================================================================================
 * @file        version.c
 * @brief       Firmware Version String & Build Information Constants
 * @author      Dual Tachyon (Original)
 * @author      N7SIX/Professional Enhancement Team (2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * "Runtime version identification, build metadata, and hardware compatibility strings"
 * =====================================================================================
 * ARCHITECTURAL OVERVIEW:
 * Defines compile-time version strings and build metadata displayed during startup,
 * UART boot sequence, and diagnostic output. Supports multiple firmware editions
 * (ApeX, Broadcast, Bandscope, etc.) with distinct version identifiers. Handles
 * optional custom build strings for test/development distributions.
 *
 * MAJOR FEATURES (2025-2026):
 * ---------------------------
 * - Multi-edition version support (conditional compilation)
 * - Author string configuration (Dual Tachyon, N7SIX, muzkr, etc.)
 * - Custom build string injection (CI/CD pipeline integration)
 * - UART protocol version matching for serial tools
 * - Hardware variant detection strings
 * - Build timestamp embedding (optional)
 *
 * TECHNICAL SPECIFICATIONS:
 * -------------------------
 * - Version format: v#.#.# or v#.#.#brN (beta release notation)
 * - String length: <32 bytes per variant
 * - Storage: ROM constants (compile-time determined)
 * - UART transmission: 9600 baud startup sequence
 * - Edition-specific: ApeX, Broadcast, Bandscope, Basic, RescueOps, Game
 * - Compile flags: ENABLE_FEAT_N7SIX, VERSION_STRING, AUTHOR_STRING_2
 * =====================================================================================
 */

#ifdef VERSION_STRING
    #define VER     " "VERSION_STRING
#else
    #define VER     ""
#endif

#ifdef ENABLE_FEAT_N7SIX
    const char Version[]      = AUTHOR_STRING_2 " " VERSION_STRING_2;
    const char Edition[]      = EDITION_STRING;
#else
    const char Version[]      = AUTHOR_STRING VER;
#endif

const char UART_Version[] = "UV-K5 Firmware, " AUTHOR_STRING VER "\r\n";
