# Arduino MIDI Player

A portable MIDI-like player for Arduino that reads musical note events from CSV files stored on an SD card and plays them through multiple buzzers, with a TFT/OLED display interface and controls for playback, speed, and transposition.

## Features

* Browse and select CSV files from an SD card
* Play, pause, stop, rewind, and fast-forward playback
* Adjust playback speed (tempo) and transpose pitch in real-time
* Buffered seeking to avoid frequent file parsing
* Visual feedback on TFT/OLED display with playback menu and file list
* Logging of user actions and events to SD card
* Control via physical buttons and serial commands

## Hardware Requirements

* Arduino board with sufficient I/O pins and Timers (e.g., Arduino Mega)
* SD card module
* TFT/OLED display compatible with Adafruit\_ST7735 (SPI interface)
* 5 buzzers connected to digital pins (in this case 28–32)
* 3 push buttons for UP, OK, and DOWN controls
* Connecting wires, breadboard or prototyping board

## Software Requirements

* PlatformIO on Visual Studio Code
* Arduino core libraries
* [SD library](https://www.arduino.cc/en/Reference/SD)
* [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
* [Adafruit ST7735 Library](https://github.com/adafruit/Adafruit-ST7735-Library)
* [Tone Library](https://github.com/bhagman/Tone)

## File Structure

```
|-- README.md
|-- include
|   |-- logger.h
|   |-- oled_gui.h
|   |-- player.h
|   `-- sd_card.h
|-- lib
|   |-- Adafruit_BusIO
|   |-- Adafruit_GFX
|   |-- Adafruit_ST7735
|   |-- IRremote
|   |-- SD
|   |-- TimerFreeTone
|   `-- Tone
|-- midi_csv_generator
|   `-- main.py
|-- platformio.ini
|-- src
|   |-- logger.cpp
|   |-- main.cpp
|   |-- oled_gui.cpp
|   |-- player.cpp
|   `-- sd_card.cpp
```

## Usage

1. Insert an SD card containing `.csv` files with note event data into the SD module (root directory).
2. Power on the Arduino. The TFT/OLED will display a "PLAYLIST" menu.
3. Use the **DOWN** and **UP** buttons to navigate the file list and **OK** to select a file.
4. In playback mode, navigate options:

   * `||`: Play/Pause
   * `/D`: Stop
   * `>>`: Fast-forward
   * `<<`: Rewind
   * `S+` / `S-`: Increase/decrease speed
   * `T+` / `T-`: Transpose up/down semitones
5. Alternatively, send serial commands at 9600 baud:

   * `p`: Play/Pause
   * `s`: Stop
   * `z` / `x`: Rewind 5s / Forward 5s
   * `w` / `q`: Tempo + / -
   * `]` / `[` : Transpose + / -

## CSV Format

Each `.csv` file should contain a header followed by lines with the format:

```
index,frequency,startTime,endTime,buzzerIndex
```

* `index`: index of note, ignored in the program
* `frequency`: Tone frequency in Hz
* `startTime`, `endTime`: Time in milliseconds when the note starts and ends
* `buzzerIndex`: Integer \[1–5] indicating which buzzer to use

CSV can be also generated from MIDI file using Python script attached to the repository.

## MIDI-to-CSV Conversion

A helper Python script is provided to generate the required CSV files from standard MIDI files.

- **Location:** `midi_csv_generator/main.py`
- **Requirements:**
  - Python 3.x
  - `mido` library (`pip install mido python-rtmidi`)
- **Usage:**
  ```bash
  python midi_csv_generator/main.py input_file.mid 
  
## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
