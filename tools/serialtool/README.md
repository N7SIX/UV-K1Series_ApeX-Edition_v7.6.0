# UV-K Serial Flasher

This package is a host-side serial flasher and configuration tool for UV-K radios.
It is a clean, repo-native alternative to UVTools2, with its own packet framing and bootloader protocol.

## What it supports
- Firmware flashing via bootloader protocol
- EEPROM/config/calibration data dump and restore
- USB CDC / UART transport using the radio's built-in serial protocol

## Protocol basics
- Packet start: `0xAB 0xCD`
- Packet end: `0xDC 0xBA`
- Messages are obfuscated by XOR with a 16-byte table
- CRC is CRC-CCITT-style with polynomial `0x1021`

## Usage

Install dependencies:
```bash
python3 -m pip install pyserial
```

Run the tool:
```bash
python -m serialtool.cli flash --port /dev/ttyUSB0 --bl-ver 1.01 firmware.bin
```

Other commands:
```bash
python -m serialtool.cli dump --port /dev/ttyUSB0 --all backup.bin
python -m serialtool.cli restore --port /dev/ttyUSB0 --config backup.bin
```

## Notes
- This repo already includes the host-side programmer in `tools/serialtool`.
- The firmware-side protocol is implemented in `App/app/uart.c`.
- You can extend this package to support UV-K5, K6 v1, K1 Series, and K5 v3 by adding device detection and bootloader-specific options.
