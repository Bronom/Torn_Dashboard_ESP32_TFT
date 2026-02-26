#pragma once

struct WifiCred {
  const char* ssid;
  const char* password;
};

// -------- WIFI CREDENTIALS --------
WifiCred wifiList[] = {
  {"SSID", "PASSWORD"},
  {"SSID_2", "PASSWORD"},
  {"SSID_3", "PASSWORD"}
};

// -------- TORN API KEY (FULL ACCESS) --------
const char* apiKey = "YOUR_TORN_API_KEY";