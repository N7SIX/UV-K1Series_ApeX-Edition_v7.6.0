# UI Simulator & Test Harness: K5Viewer

K5Viewer is the official UI simulator and test harness for the Quansheng UV-K1/K5 N7SIX firmware. It allows developers and testers to view the radio's 128×64 LCD framebuffer in real time on their computer, using a standard USB-to-Serial cable.

## Features
- Live display of the radio's screen (128×64 monochrome)
- Delta frame updates for efficient bandwidth
- Screenshot capture (PNG)
- Multiple color themes and inverted mode
- FPS display, window resizing, and pixel rendering options

## Usage
1. **Connect** your radio (running N7SIX firmware) to your PC with a Baofeng/Kenwood-style USB-to-Serial cable.
2. **Install dependencies:**
   ```bash
   pip install pyserial pygame
   ```
3. **Run the viewer:**
   ```bash
   python tools/k5viewer/k5viewer.py --port COMx  # Replace COMx with your serial port
   ```

See `tools/k5viewer/README.md` for full details and advanced options.

## Developer Notes
- K5Viewer is ideal for UI development, regression testing, and documentation screenshots.
- It does not provide remote control—only display mirroring and screenshot capture.
- For integration with CI or automated UI tests, consider scripting K5Viewer with Python.

---
**Location:** `tools/k5viewer/`
**Maintainer:** N7SIX
