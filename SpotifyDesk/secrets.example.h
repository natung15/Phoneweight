// Copy to secrets.h and fill in. secrets.h is gitignored.
#pragma once
#define SPOTIFY_CLIENT_ID     "your-client-id"
#define SPOTIFY_CLIENT_SECRET "your-client-secret"
// Wi-Fi and the Spotify refresh token are entered in the DeskPlayer-Setup portal.
// To skip typing the token, flash once with SPOTIFY_REFRESH_TOKEN defined here; it is
// copied to flash on boot, so you can remove it again afterwards.
// Free key from https://getsongbpm.com/api; leave out to play the GIF at its own speed
// #define GETSONGBPM_API_KEY    "your-getsongbpm-key"
