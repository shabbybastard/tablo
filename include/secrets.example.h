/**
 * Optional local defaults.
 *
 * Copy this file to include/secrets.h (it is gitignored) and fill in only what
 * you need for local debugging. Everything here can also be entered at runtime
 * through the Wi-Fi portal and /config; NVS values win over these defaults.
 *
 * NEVER commit real tokens, passwords or private IP addresses.
 */
#pragma once

/* Wi-Fi: leave empty to start the first-run setup portal. */
#define TABLO_WIFI_SSID ""
#define TABLO_WIFI_PASSWORD ""

/* HTTP Basic Auth for /config, /update and /logs. The placeholder password
 * "change-me" is treated as unset: the device opens the setup portal until a
 * real password (min 8 chars) is configured. */
#define TABLO_ADMIN_USER "admin"
#define TABLO_ADMIN_PASSWORD "change-me"

/* Public OTA manifest published by GitHub Pages. */
#define TABLO_OTA_MANIFEST_URL "https://shabbybastard.github.io/tablo/manifest.json"

/* Optional demo coordinates (no API key required with Open-Meteo). */
#define TABLO_WEATHER_LAT ""
#define TABLO_WEATHER_LON ""

/* ICMP targets. Use hostnames or TEST-NET addresses, never real private IPs
 * in the committed example. */
#define TABLO_PING_GW ""
#define TABLO_PING_VPS ""
#define TABLO_PING_DNS ""

/* Optional Xiaomi air purifier defaults. The token is 32 hex characters and
 * is stored in NVS after saving through the web UI. Leave empty here. */
#define TABLO_PURIFIER_IP ""
#define TABLO_PURIFIER_TOKEN ""
