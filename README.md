# Phoneweight: ESP32 Spotify desk display

A small desk display that shows what's playing on Spotify. It's an ESP32 with a 3.5" touchscreen showing the album art, song title, all artists, and an animated wave. Tap the screen (or press BOOT) to switch to a second page where a GIF dances in time with the song's BPM.

Wi-Fi is set up from your phone, so it doesn't need to be compiled into the firmware. The board remembers up to 5 networks and joins whichever one is in range.

BPM data provided by [ReccoBeats](https://reccobeats.com) and [GetSongBPM](https://getsongbpm.com).

## Contents

- [Hardware](#hardware)
- [1. Install the tools](#1-install-the-tools)
- [2. Create a Spotify app](#2-create-a-spotify-app)
- [3. Get a refresh token](#3-get-a-refresh-token)
- [4. Fill in secrets.h and upload](#4-fill-in-secretsh-and-upload)
- [5. Connect it to Wi-Fi from your phone](#5-connect-it-to-wi-fi-from-your-phone)
- [Using the display](#using-the-display)
- [Where to update things](#where-to-update-things)
- [Using your own GIF](#using-your-own-gif)
- [Troubleshooting](#troubleshooting)

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

## 1. Install the tools

On a Mac:

```sh
brew install arduino-cli
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install ArduinoJson TJpg_Decoder TFT_eSPI AnimatedGIF WiFiManager
cp tft_config/User_Setup.h ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
cp SpotifyDesk/secrets.example.h SpotifyDesk/secrets.h
```

`secrets.h` holds your keys and is gitignored, so it never gets pushed.

## 2. Create a Spotify app

The display reads your playback through the Spotify Web API, which needs an app registered on your account.

1. Go to the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) and log in.
2. Click **Create app**. Any name and description works.
3. Under **Redirect URIs**, add exactly `http://127.0.0.1:8888/callback`.
4. Under **Which API/SDKs are you planning to use?**, tick **Web API**, then save.
5. Open the app's **Settings** and copy the **Client ID** and **Client secret**.

## 3. Get a refresh token

The refresh token gives the display permission to see what one Spotify account is playing. You do this once per account. You can do it on your computer or a phone, but you need a computer for step 3.

1. Open this link in a browser, with `YOUR_CLIENT_ID` replaced by your Client ID:

   ```
   https://accounts.spotify.com/authorize?client_id=YOUR_CLIENT_ID&response_type=code&redirect_uri=http%3A%2F%2F127.0.0.1%3A8888%2Fcallback&scope=user-read-currently-playing
   ```

2. Log in and tap **Agree**. The browser then goes to a `127.0.0.1` page that **fails to load. That's expected.** Copy the full address from the address bar. It looks like `http://127.0.0.1:8888/callback?code=AQB...`. The part after `code=` is the code you need in the next step.

3. Within a few minutes (the code expires quickly and only works once), run this on your computer:

   ```sh
   curl -X POST https://accounts.spotify.com/api/token \
     -u YOUR_CLIENT_ID:YOUR_CLIENT_SECRET \
     -d grant_type=authorization_code \
     -d code=THE_CODE \
     -d redirect_uri=http://127.0.0.1:8888/callback
   ```

4. The reply includes `"refresh_token":"..."`. Copy that value. You'll paste it into the display in step 5.

## 4. Fill in secrets.h and upload

Edit `SpotifyDesk/secrets.h`:

```c
#define SPOTIFY_CLIENT_ID     "your-client-id"
#define SPOTIFY_CLIENT_SECRET "your-client-secret"
// Optional: a free key from https://getsongbpm.com/api, used when ReccoBeats doesn't know a song
// #define GETSONGBPM_API_KEY "your-getsongbpm-key"
```

Plug in the ESP32 and upload:

```sh
arduino-cli compile --upload -p /dev/cu.usbserial-0001 \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app SpotifyDesk
```

Run `arduino-cli board list` if your port has a different name. If the upload hangs on "Connecting...", hold BOOT and tap EN.

## 5. Connect it to Wi-Fi from your phone

1. After the upload, the display says **"Connect to DeskPlayer-Setup on your phone"**.
2. On your phone, go to **Settings → Wi-Fi** and join **DeskPlayer-Setup**. It has no password.
3. A setup page pops up. If it doesn't, open a browser and go to **192.168.4.1**.
4. Tap **Configure WiFi**, pick your network and enter its password. The network must be 2.4 GHz, because the ESP32 can't use 5 GHz.
5. Paste your refresh token into **Spotify refresh token**. To move it from your computer to your phone, AirDrop or email it to yourself, or use Universal Clipboard.
6. Tap **Save**. The phone page stays on "Saving credentials". That's normal, so watch the **display** instead: it shows **Connected** with an IP address.

Play something on Spotify and it appears within a few seconds.

If nobody finishes setup within 3 minutes, the board restarts and tries again.

## Using the display

| What | How |
|---|---|
| Switch between now-playing and the GIF page | Tap the screen, or press **BOOT** |
| Add another Wi-Fi network | Press **EN**, then **hold BOOT for 1 second** while the screen says "Hold BOOT now to add a Wi-Fi network", then follow step 5 |
| Restart | Press **EN** |

- **Saved networks:** the board keeps up to 5 Wi-Fi networks and joins whichever one is in range. Adding a sixth drops the oldest.
- **Adding a network:** your saved networks and Spotify token are kept. Leave the token field blank unless you want to change accounts.
- **Avoid:** holding BOOT *while* pressing EN or plugging in. That puts the board into flashing mode, and the screen freezes until you press EN again.

## Where to update things

| To change... | Update it here |
|---|---|
| **Wi-Fi network** | On the display: hold BOOT at startup and add the network from your phone (see above). No re-upload needed. |
| **Which Spotify account it shows** | Get a refresh token for that account ([step 3](#3-get-a-refresh-token)), then hold BOOT at startup and paste it into **Spotify refresh token** on the setup page. No re-upload needed. |
| **Who is allowed to use your Spotify app** | [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) → your app → **User Management**. See below. |
| **Spotify Client ID or secret** | `SpotifyDesk/secrets.h`, then upload again ([step 4](#4-fill-in-secretsh-and-upload)). |
| **GetSongBPM key** | `SpotifyDesk/secrets.h`, then upload again. |
| **Dancing GIF** | See [Using your own GIF](#using-your-own-gif), then upload again. |
| **Screen orientation** | `tft.setRotation(2)` in `SpotifyDesk/SpotifyDesk.ino`. Use `0` if it's upside down. |
| **Wiring or display driver** | `tft_config/User_Setup.h`, then copy it into the TFT_eSPI library folder again ([step 1](#1-install-the-tools)). |

### Letting someone else use it with their own Spotify account

A new Spotify app starts in **development mode**, where only accounts you've approved can use it. For a friend's display to show *their* music:

1. In the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard), open your app → **User Management**.
2. Add their name and **the email address on their Spotify account**.
3. They open the step 3 link (with your Client ID) and tap **Agree**. Then they send you the `127.0.0.1` address it lands on.
4. You run the `curl` command with that code and send them back the refresh token. You do this step because it needs your Client secret.
5. They paste the token into the setup page from step 5.

If you skip step 2, they can log in, but Spotify blocks the app from reading their playback.

Development mode allows only a small number of users, and making an app public ("extended quota") is limited to registered businesses with a large user base. For a few friends and family, development mode works fine. Check your app's dashboard for Spotify's current limits.

## Using your own GIF

```sh
python3 tools/gif2h.py your.gif --beats 4
```

`--beats` is how many beats one loop of the GIF lasts: a single bounce is 1, and a left-right-left-right step is 4. The display stretches the loop to match the current song's tempo, and fills the page behind the GIF with the GIF's corner colour. Upload again afterwards.

## Troubleshooting

- **Stuck on "Connecting to Wi-Fi..." for over a minute:** the saved networks can't be reached. Wait for the setup screen, or hold BOOT at startup to add a network.
- **Says "Connected" but no music appears:** make sure something is actually playing. If it still doesn't show, the refresh token may be wrong or the account isn't in **User Management**. Add a fresh token through the setup page.
- **DeskPlayer-Setup doesn't show on your phone:** press EN and wait for the setup screen. The hotspot only runs while that screen is showing.
- **Board keeps restarting on a weak USB port:** use a better cable or a powered hub. The firmware already lowers Wi-Fi transmit power to reduce brownouts.
