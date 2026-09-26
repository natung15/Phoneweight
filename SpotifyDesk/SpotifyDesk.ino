#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "secrets.h"

// Layout for 320x480 portrait: text on top, art along the bottom
const int SCREEN_W = 320, SCREEN_H = 480;
const int ART_SIZE = 300, ART_X = (SCREEN_W - ART_SIZE) / 2, ART_Y = SCREEN_H - ART_SIZE - 10;
const int TEXT_X = 10, TEXT_Y = 8, TEXT_W = SCREEN_W - 2 * TEXT_X;
const int WAVE_H = 18, WAVE_Y = ART_Y - 6 - WAVE_H;
const int BAR_W = 6, BAR_GAP = 4, BAR_COUNT = (TEXT_W + BAR_GAP) / (BAR_W + BAR_GAP);
const uint32_t WAVE_FRAME_MS = 80;
const uint32_t POLL_MS = 5000;

TFT_eSPI tft;
// Spotify polling runs as its own task on core 0 so the wave keeps moving
// during slow HTTPS requests; this lock keeps the two from drawing at once
SemaphoreHandle_t tftLock;

String accessToken;
uint32_t tokenFetchedAt = 0, tokenTtlMs = 0;
String currentArtUrl, lastInfo;
volatile bool isPlaying = false, showWave = false;
uint32_t lastFrame = 0;
uint8_t barH[BAR_COUNT];  // height each bar is currently drawn at

const uint16_t SPOTIFY_GREEN = 0x1DCA;  // #1DB954
uint16_t panelBg = TFT_BLACK;           // darkened average colour of the current art
uint32_t sumR, sumG, sumB, sumN;

// TJpg_Decoder hands over decoded blocks; push them straight to the screen
bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  // Sample the art so the background can be tinted to match
  for (uint32_t i = 0; i < (uint32_t)w * h; i += 7) {
    uint16_t c = (bitmap[i] >> 8) | (bitmap[i] << 8);  // undo setSwapBytes
    sumR += c >> 11;
    sumG += (c >> 5) & 0x3F;
    sumB += c & 0x1F;
    sumN++;
  }
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // full power browns out weak USB supplies
  Serial.print("Connecting to Wi-Fi");
  uint32_t attemptStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
    if (millis() - attemptStart > 10000) {  // the router may refuse; start over
      Serial.print(" retrying");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      attemptStart = millis();
    }
  }
  Serial.printf("\nConnected, IP %s\n", WiFi.localIP().toString().c_str());
}

bool refreshAccessToken() {
  WiFiClientSecure client;
  client.setInsecure();  // starter only: skips TLS certificate checks
  HTTPClient http;
  http.begin(client, "https://accounts.spotify.com/api/token");
  http.setAuthorization(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(String("grant_type=refresh_token&refresh_token=") + SPOTIFY_REFRESH_TOKEN);
  String body = http.getString();
  http.end();
  if (code != 200) {
    Serial.printf("Token refresh failed: %d %s\n", code, body.c_str());
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  accessToken = doc["access_token"].as<String>();
  tokenTtlMs = (doc["expires_in"] | 3600) * 1000UL - 60000;  // refresh a minute early
  tokenFetchedAt = millis();
  Serial.println("Got access token");
  return true;
}

bool tokenValid() {
  return accessToken.length() && millis() - tokenFetchedAt < tokenTtlMs;
}

// Word-wrap s into at most maxLines lines of maxW pixels in the current font,
// ending with "..." when it doesn't all fit. Returns the number of lines.
int wrapText(const String& s, int maxW, int maxLines, String* lines) {
  int n = 0;
  String rest = s;
  rest.trim();
  while (rest.length() && n < maxLines) {
    int end = rest.length();
    while (end > 1 && tft.textWidth(rest.substring(0, end)) > maxW) end--;
    if (end < (int)rest.length()) {  // break at a space when there is one
      int sp = rest.lastIndexOf(' ', end);
      if (sp > 0) end = sp;
    }
    lines[n++] = rest.substring(0, end);
    rest = rest.substring(end);
    rest.trim();
  }
  if (rest.length() && n) {
    String& last = lines[n - 1];
    while (last.length() && tft.textWidth(last + "...") > maxW) last.remove(last.length() - 1);
    last.trim();
    last += "...";
  }
  return n;
}

// Fill everything except the art itself with the tinted background
void fillBackground() {
  int artRight = ART_X + ART_SIZE, artBottom = ART_Y + ART_SIZE;
  tft.fillRect(0, 0, tft.width(), ART_Y, panelBg);
  tft.fillRect(0, artBottom, tft.width(), tft.height() - artBottom, panelBg);
  tft.fillRect(0, ART_Y, ART_X, ART_SIZE, panelBg);
  tft.fillRect(artRight, ART_Y, tft.width() - artRight, ART_SIZE, panelBg);
}

void drawInfo(const String& title, const String& artist) {
  fillBackground();
  tft.setTextDatum(TL_DATUM);
  String lines[5];

  // Label row: "NOW PLAYING" on the left, play state on the right
  tft.setTextFont(2);
  tft.setTextColor(TFT_LIGHTGREY);  // one colour = transparent background
  tft.drawString("NOW PLAYING", TEXT_X, TEXT_Y);
  const char* state = isPlaying ? "Playing" : "Paused";
  int right = TEXT_X + TEXT_W;
  tft.fillCircle(right - tft.textWidth(state) - 9, TEXT_Y + 8, 4, isPlaying ? SPOTIFY_GREEN : TFT_DARKGREY);
  tft.setTextColor(isPlaying ? TFT_WHITE : TFT_LIGHTGREY);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(state, right, TEXT_Y);
  tft.setTextDatum(TL_DATUM);

  // Title (up to 3 lines), then artists in whatever space is left above the art
  int y = TEXT_Y + 20;
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(TFT_WHITE);
  int n = wrapText(title, TEXT_W, 3, lines);
  for (int i = 0; i < n; i++, y += tft.fontHeight()) tft.drawString(lines[i], TEXT_X, y);

  y += 4;
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_LIGHTGREY);
  int artistLines = constrain((WAVE_Y - 4 - y) / tft.fontHeight(), 1, 5);
  n = wrapText(artist, TEXT_W, artistLines, lines);
  for (int i = 0; i < n; i++, y += tft.fontHeight()) tft.drawString(lines[i], TEXT_X, y);

  memset(barH, 0, sizeof(barH));  // the background fill wiped the wave; redraw it all
  drawWave();
}

// Decorative wave under the artists. Spotify doesn't expose live audio levels,
// so the bars are generated: moving while a track plays, flat when paused.
// Only the pixels that changed since the last frame are drawn.
void drawWave() {
  int x0 = TEXT_X + (TEXT_W - (BAR_COUNT * (BAR_W + BAR_GAP) - BAR_GAP)) / 2;
  int mid = WAVE_Y + WAVE_H / 2;
  float t = millis() / 1000.0f;
  uint16_t color = isPlaying ? SPOTIFY_GREEN : TFT_DARKGREY;
  for (int i = 0; i < BAR_COUNT; i++) {
    int h = 2;
    if (isPlaying) {
      float env = 0.35f + 0.65f * sinf(PI * (i + 0.5f) / BAR_COUNT);  // taller in the middle
      float v = 0.55f + 0.3f * sinf(i * 0.45f + t * 6.0f) + 0.15f * sinf(i * 1.3f - t * 9.1f);
      h = max(2, (int)(v * env * WAVE_H)) & ~1;  // even heights keep bars centred
    }
    int old = barH[i];
    if (h == old) continue;
    int x = x0 + i * (BAR_W + BAR_GAP);
    if (h > old) {
      tft.fillRect(x, mid - h / 2, BAR_W, h, color);
    } else {  // shrink: clear the strips above and below the new height
      int d = (old - h) / 2;
      tft.fillRect(x, mid - old / 2, BAR_W, d, panelBg);
      tft.fillRect(x, mid + h / 2, BAR_W, d, panelBg);
    }
    barH[i] = h;
  }
}

bool drawArt(const String& url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  int code = http.GET();
  int len = http.getSize();
  if (code != 200 || len <= 0 || len > 150000) {
    Serial.printf("Art download failed: HTTP %d, %d bytes\n", code, len);
    http.end();
    return false;
  }
  uint8_t* buf = (uint8_t*)(psramFound() ? ps_malloc(len) : malloc(len));
  if (!buf) {
    Serial.println("Out of memory for art");
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  int got = 0;
  uint32_t start = millis();
  while (got < len && (http.connected() || stream->available()) && millis() - start < 10000) {
    size_t avail = stream->available();
    if (avail) got += stream->readBytes(buf + got, min((size_t)(len - got), avail));
    else delay(1);
  }
  http.end();

  bool ok = got == len;
  if (ok) {
    uint16_t w, h;
    TJpgDec.getJpgSize(&w, &h, buf, len);
    uint8_t scale = 1;
    while (w / scale > ART_SIZE && scale < 8) scale *= 2;
    TJpgDec.setJpgScale(scale);
    xSemaphoreTake(tftLock, portMAX_DELAY);
    tft.fillRect(ART_X, ART_Y, ART_SIZE, ART_SIZE, TFT_BLACK);
    sumR = sumG = sumB = sumN = 0;
    TJpgDec.drawJpg(ART_X, ART_Y, buf, len);
    if (sumN) {  // darken the average to ~40% so white text stays readable
      uint16_t r = sumR / sumN * 2 / 5, g = sumG / sumN * 2 / 5, b = sumB / sumN * 2 / 5;
      panelBg = (r << 11) | (g << 5) | b;
    }
    xSemaphoreGive(tftLock);
    Serial.printf("Drew %ux%u art (scale 1/%u), free heap %u\n", w, h, scale, ESP.getFreeHeap());
  } else {
    Serial.printf("Art download incomplete: %d/%d bytes\n", got, len);
  }
  free(buf);
  return ok;
}

void fetchNowPlaying() {
  if (!tokenValid() && !refreshAccessToken()) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.spotify.com/v1/me/player/currently-playing");
  http.addHeader("Authorization", "Bearer " + accessToken);
  int code = http.GET();

  if (code == 204) {  // nothing playing
    http.end();
    Serial.println("Nothing playing");
    return;
  }
  if (code == 401) {  // token rejected; get a fresh one next poll
    accessToken = "";
    http.end();
    return;
  }
  if (code != 200) {
    Serial.printf("currently-playing failed: %d\n", code);
    http.end();
    return;
  }
  String body = http.getString();
  http.end();

  // Keep only the fields we use; the full response is several KB
  JsonDocument filter;
  filter["is_playing"] = true;
  filter["item"]["name"] = true;
  filter["item"]["artists"][0]["name"] = true;
  filter["item"]["album"]["images"][0]["url"] = true;
  filter["item"]["album"]["images"][0]["width"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("JSON error: %s\n", err.c_str());
    return;
  }
  if (doc["item"].isNull()) {
    Serial.println("No track info (ad or podcast?)");
    return;
  }

  isPlaying = doc["is_playing"] | false;
  String title = doc["item"]["name"] | "";
  String artist;
  for (JsonObject a : doc["item"]["artists"].as<JsonArray>()) {
    if (artist.length()) artist += ", ";
    artist += a["name"] | "";
  }

  // Spotify usually lists 640, 300 and 64 px images; take the 300 px one
  JsonArray images = doc["item"]["album"]["images"];
  String artUrl;
  for (JsonObject img : images) {
    if (img["width"] == ART_SIZE) {
      artUrl = img["url"].as<String>();
      break;
    }
  }
  if (artUrl.isEmpty() && images.size()) artUrl = images[0]["url"].as<String>();

  // Art first: it sets the background colour the text panel is drawn on
  bool newArt = artUrl.length() && artUrl != currentArtUrl && drawArt(artUrl);
  if (newArt) currentArtUrl = artUrl;

  String info = title + "|" + artist + "|" + isPlaying;
  if (newArt || info != lastInfo) {
    xSemaphoreTake(tftLock, portMAX_DELAY);
    drawInfo(title, artist);
    xSemaphoreGive(tftLock);
    showWave = true;
    lastInfo = info;
    Serial.printf("%s - %s (%s)\n", title.c_str(), artist.c_str(), isPlaying ? "playing" : "paused");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nSpotifyDesk, PSRAM: %u bytes\n", ESP.getPsramSize());
  tft.init();
  tft.setRotation(2);  // portrait, 320x480; use 0 if it's upside down
  tft.fillScreen(TFT_BLACK);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tftOutput);
  tftLock = xSemaphoreCreateMutex();
  connectWiFi();
  refreshAccessToken();
  xTaskCreatePinnedToCore(spotifyTask, "spotify", 16384, nullptr, 1, nullptr, 0);
}

void spotifyTask(void*) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) connectWiFi();
    fetchNowPlaying();
    vTaskDelay(pdMS_TO_TICKS(POLL_MS));
  }
}

// loop() only animates the wave; all network work happens in spotifyTask
void loop() {
  if (showWave && isPlaying && millis() - lastFrame >= WAVE_FRAME_MS) {
    lastFrame = millis();
    xSemaphoreTake(tftLock, portMAX_DELAY);
    drawWave();
    xSemaphoreGive(tftLock);
  }
  delay(10);
}
