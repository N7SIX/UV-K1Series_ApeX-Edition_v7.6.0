# MDC-1200 Unit ID Quick Entry Guide

## Super Quick Start

**Navigate to MENU_MDC_ID → Press MENU → Type 4 digits (0–9) or arrow keys for A–F → Press MENU to save**

## Common Examples

### Example 1: Enter 0x1234
1. Navigate to MENU_MDC_ID
2. Press MENU
3. Type: 1, 2, 3, 4
4. Press MENU to save
5. Display shows: **0x1234**

### Example 2: Enter 0x4A5F (with hex letters)
1. Navigate to MENU_MDC_ID
2. Press MENU
3. Type 4
4. UP arrow 10× (cycles to A)
5. Type 5
6. UP arrow 15× (cycles to F)
7. Press MENU to save
8. Display shows: **0x4A5F**

### Example 3: Correct a Typo
1. Typed 1, 2, 3, **0** but meant 4
2. Press EXIT (backspace) to remove 0
3. Type 4
4. Press MENU to save
5. Display shows: **0x1234**

## Key Reference

| What You Want | Keys to Press |
|---------------|---------------|
| Enter digit 0–9 | Press the key 0–9 |
| Enter digit A | Type 0, then UP×10 (or DOWN×6) |
| Enter digit B | Type 0, then UP×11 (or DOWN×5) |
| Enter digit C | Type 0, then UP×12 (or DOWN×4) |
| Enter digit D | Type 0, then UP×13 (or DOWN×3) |
| Enter digit E | Type 0, then UP×14 (or DOWN×2) |
| Enter digit F | Type 0, then UP×15 (or DOWN×1) |
| Delete last digit | Press EXIT |
| Increase full value | Press UP arrow (after entry complete) |
| Decrease full value | Press DOWN arrow (after entry complete) |
| Save and confirm | Press MENU |
| Cancel entry | Hold EXIT to exit menu |

## Common MDC Unit IDs

| ID | How to Enter | Use Case |
|----|--------------|-----------| 
| 0x0000 | 0, 0, 0, 0 | Default/Reset |
| 0x0001 | 0, 0, 0, 1 | Unit 1 |
| 0x0100 | 0, 1, 0, 0 | Test pattern |
| 0x1000 | 1, 0, 0, 0 | Base unit |
| 0x1234 | 1, 2, 3, 4 | Example |
| 0xFFFF | F, F, F, F | Maximum value |
| 0xABCD | 0→UP×10, B→UP×11, C→UP×12, D→UP×13 | High letter value |

## Display Guide

As you type, you'll see the value build up:

```
Before entry:    0x1234    (previous value)
After entering:
                 0x____    (empty, ready for input)
After 1 digit:   0x1___
After 2 digits:  0x12__
After 3 digits:  0x123_
After 4 digits:  0x1234    (ready to save)
After MENU:      0x1234    (saved to radio!)
```

## Fastest Entry Tips

1. **Use arrow keys while typing letters** — Don't stop at 0, start typing next digit, then arrow up for A–F
2. **For F, press UP once from E** — Saves time vs. counting to 15
3. **Arrow keys work on last digit only while entering** — So you can fix mistakes without backspacing everything
4. **Hold EXIT to exit completely** — Quick exit if you change your mind

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Value won't save | Make sure you pressed MENU after 4 digits |
| Typo in middle digits | Press EXIT once per mistake, re-enter |
| Radio won't accept A–F | Use UP/DOWN arrows to cycle the digit 0→F |
| Lost my MDC ID | Hold EXIT to cancel, previous value stays |

## Common Radio Models + MDC IDs

Ask your radio group for their standard MDC Unit ID, or set a unique one:

- **Default Radio**: 0x0000 (no MDC)
- **Personal Unit**: 0x0001–0x0099 (simple numbering)
- **Team Group**: 0x1000–0x1FFF (group range)
- **Emergency**: 0xFFFF (emergency call, all units)

---

**Last Updated**: 2024-08  
**Firmware**: v7.6.10B (ApeX Edition)  
**Feature**: Direct Hex Digit Input for MDC-1200 Unit ID
