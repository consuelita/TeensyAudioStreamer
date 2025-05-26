# Teensy USB Audio WAV Player

A simple WAV file player for Teensy 4.0/4.1 that streams audio via USB.

## Features

- 🎵 Plays WAV files from SD card in sequence
- 🔄 Automatic looping through all files
- 🎛️ Serial control interface with commands
- 📊 Real-time audio processor and memory usage monitoring
- 🔧 Automatic error recovery and file rescanning
- 💾 Memory-optimized for stable operation
- 🔌 USB audio output (works as USB audio device)

## Hardware Requirements

- **Teensy 4.0 or 4.1** microcontroller
- **MicroSD card** (FAT32 formatted recommended)
- **WAV files** stored in the root directory of the SD card

### Connections

- SD card uses the built-in SD card slot on Teensy 4.1, or connect to SPI pins on Teensy 4.0
- USB connection to computer/Android device for audio output
- Serial connection (optional, for control and monitoring)

## Software Requirements

- **Arduino IDE** with Teensy support
- **Teensyduino** add-on installed
- **Audio Library** (included with Teensyduino)

### Required Libraries
- `Audio.h` - Teensy Audio Library
- `Wire.h` - I2C communication
- `SPI.h` - SPI communication
- `SerialFlash.h` - Flash memory support

## Installation

1. Install Arduino IDE and Teensyduino from [PJRC website](https://www.pjrc.com/teensy/teensyduino.html)
2. Download or clone this repository
3. Open `teensy_wav_player.ino` in Arduino IDE
4. Select your Teensy board (Tools → Board → Teensy 4.0 or 4.1)
5. Set USB Type to "Audio" (Tools → USB Type → Audio)
6. Upload the sketch to your Teensy

## Usage

### Setup
1. Format your microSD card as FAT32
2. Copy your WAV files to the **root directory** of the SD card
3. Insert the SD card into your Teensy
4. Power up the Teensy and connect via USB

### Supported Audio Formats
- **WAV files** (.wav extension)
- Located in the root directory of the SD card
- Various sample rates and bit depths supported by Teensy Audio Library

### Serial Commands

Connect to the Teensy's serial port (115200 baud) to use these commands:

| Command | Action |
|---------|--------|
| `n` or `N` | Skip to next track |
| `r` or `R` | Restart current track |
| `s` or `S` | Stop playback |
| `p` or `P` | Play/Resume |
| `f` or `F` | Rescan SD card for files |
| `?`, `h`, or `H` | Show help menu |

### Status Information

The device automatically prints status information every 2 seconds, including:
- Currently playing file
- Track position (X of Y)
- Playback status
- Processor usage percentage
- Memory usage
- Available commands

## Configuration

You can modify these parameters in the code:

```cpp
#define SDCARD_CS_PIN 10
#define MAX_FILES 20              // Maximum WAV files to scan
#define MAX_CONSECUTIVE_FAILURES 5 // Max failures before rescan
const unsigned long STATUS_INTERVAL = 2000; // Status print interval (ms)
AudioMemory(25);                  // Audio memory blocks
```

## Troubleshooting

### SD Card Issues
- Ensure that SDCARD_CS_PIN is set to the correct pin for your specific hardware situation
- Ensure SD card is properly formatted (FAT32 recommended)
- Check that WAV files are in the root directory
- Verify SD card connections are secure
- Try a different SD card if problems persist

### Audio Issues
- Verify USB Type is set to "Audio" in Arduino IDE
- Check that your device recognizes the Teensy as a USB audio device
- Ensure WAV files are not corrupted
- Try reducing `AudioMemory()` if experiencing stability issues

### Serial Output Corruption
- This version uses memory-optimized code to prevent corruption
- If issues persist, try reducing `MAX_FILES` or `AudioMemory()`

### No Files Found
- Ensure WAV files have `.wav` extension (case insensitive)
- Check that files are in the root directory, not in folders
- Use the 'f' command to manually rescan for files

## Technical Details

- **Error Recovery**: Automatic retry mechanisms for SD card and playback failures
- **Audio Buffer**: x audio memory blocks (configurable)
- **File Limit**: Supports up to x WAV files (configurable)
- **Stability**: No recursive calls to prevent stack overflow

## License

This project is open source. Feel free to modify and distribute according to your needs.

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

## Acknowledgments

- Built using the excellent [Teensy Audio Library](https://www.pjrc.com/teensy/td_libs_Audio.html)
- Designed for [PJRC Teensy](https://www.pjrc.com/teensy/) microcontrollers
