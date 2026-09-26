# Phoneweight: ESP32 Spotify desk display

An ESP32 with a 3.5" touchscreen that shows what's playing on Spotify: album art, title, all artists, and an animated wave. Tap the screen to switch to a second page that plays a GIF synced to the song's BPM.

BPM data provided by [GetSongBPM](https://getsongbpm.com).

## Hardware

- ESP32 DevKit (ESP32-WROOM-32, CP2102 USB)
- 3.5" 480x320 SPI TFT, ILI9488 with XPT2046 touch (board marked KMRTM35018-SPI)

| Display pin | ESP32 |
|---|---|
| VCC, LED | 3V3 |
| GND | GND |
| CS | GPIO 15 |
| RESET | GPIO 4 |
| DC/RS | GPIO 2 |
| SDI (MOSI) | GPIO 23 |
| SCK | GPIO 18 |
| SDO (MISO) | not connected |
| T_CLK | GPIO 18 |
| T_DIN | GPIO 23 |
| T_DO | GPIO 19 |
| T_CS | GPIO 21 |

## Setup

```sh
brew install arduino-cli
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install ArduinoJson TJpg_Decoder TFT_eSPI AnimatedGIF
cp tft_config/User_Setup.h ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
cp SpotifyDesk/secrets.example.h SpotifyDesk/secrets.h   # then fill it in
```

`secrets.h` needs your Wi-Fi details, a Spotify app's client ID and secret, a refresh token with the `user-read-currently-playing` scope (redirect URI `http://127.0.0.1:8888/callback`), and optionally a [GetSongBPM API key](https://getsongbpm.com/api).

## Build and upload

```sh
arduino-cli compile --upload -p /dev/cu.usbserial-0001 \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app SpotifyDesk
arduino-cli monitor -p /dev/cu.usbserial-0001 -c baudrate=115200
```

If the upload hangs on "Connecting...", hold BOOT and tap EN.

## Using your own GIF

```sh
python3 tools/gif2h.py your.gif --beats 4
```

`--beats` is how many beats one loop of the GIF lasts. The sketch stretches the loop to match the current song's tempo.
