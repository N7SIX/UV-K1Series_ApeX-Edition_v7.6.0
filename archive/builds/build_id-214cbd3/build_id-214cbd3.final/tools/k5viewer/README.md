# Quansheng K5Viewer

K5Viewer is a lightweight viewer that displays the live screen output of the Quansheng K5 transceiver (running N7SIX firmware), transmitted via UART to your computer through a Baofeng/Kenwood-style USB-to-Serial cable.

https://github.com/user-attachments/assets/578c8605-9820-4a2c-90e4-5a93b2be7d2b

> [!WARNING]
> K5Viewer IS NOT A REMOTE CONTROL TOOL.

It simply displays the live screen of the Quansheng K5 on your computer and allows you to take screenshots.
It can be useful for illustrating documentation, creating tutorials, or demonstrating how K5Viewer works — whether by screen sharing or using a video projector.
It's also a handy tool for development and debugging, especially for display-related features.

## 🚀 Features

- Realtime display of 128×64 monochrome screen via serial connection (UART)
- Delta frame updates to minimize bandwidth usage
- Capture screen snapshots in PNG format
- Switch background color (gray, blue, or orange)
- Toggle inverted video mode
- Toggle LCD pixel rendering mode
- Resize the window (zoom in/out)
- Display current FPS in the window title

## 🛠️ Requirements

- Python **3.6+**
- pip (Python package installer)

### 📦 Install dependencies:

If necessary, use this command to install dependencies: 

```bash
pip install pyserial pygame
```

## ▶️ How to Run

1. Connect your **Quansheng K5** (running **N7SIX firmware** v4.2) to your computer using a Baofeng/Kenwood-style USB-to-Serial cable.

2. Run the `k5viewer` with the appropriate serial port:

   ```bash
   ./k5viewer.py -port /dev/ttyUSB0             # Linux
   ./k5viewer.py -port /dev/cu.usbserial-xxxx   # macOS
   ./k5viewer.py -port COM3                     # Windows
   ```
 > [!NOTE]   
 > If no -port is provided, the script defaults to /dev/ttyUSB0.

Alternatively, you can manually edit the script and change the `DEFAULT_PORT` variable near the top of the file:

   ```python
	DEFAULT_PORT = '/dev/ttyUSB0'              # Linux
	# or
	DEFAULT_PORT = '/dev/cu.usbserial-xxxx'    # macOS
	# or
	DEFAULT_PORT = 'COM3'                      # Windows
   ```

You can also list available serial ports to help you choose:

   ```bash
	./k5viewer.py --list-ports
   ```

## 🎮 Controls

| Key       | Action                          |
|-----------|---------------------------------|
| `Q`       | Quit the viewer                 |
| `G`       | Set background to **gray**      |
| `B`       | Set background to **blue**      |
| `O`       | Set background to **orange**    |
| `W`       | Set background to **white**     |
| `I`       | Toggle **video inversion**      |
| `P`       | Toggle **LCD pixel mode**       |
| `SPACE`   | Save a screenshot as PNG        |
| `UP`      | Increase window size            |
| `DOWN`    | Decrease window size            |


Screenshots are saved as `screenshot_YYYYMMDD_HHMMSS.png` in the same directory.

## 📬 Contact

If you encounter issues or have suggestions, feel free to open an issue or submit a pull request. Enjoy building with your Quansheng K5! 📡
