#pragma once

// 1 = manual flash mode
// 0 = web UI / saved config mode
#define MANUAL_FLASH 0

#if MANUAL_FLASH

  struct WifiCred {
    const char* ssid;
    const char* password;
  };

  // -------- WIFI CREDENTIALS --------
  // Try these networks in order
  static WifiCred wifiList[] = {
    {"SSID",   "PASSWORD"},
    {"SSID_2", "PASSWORD"},
    {"SSID_3", "PASSWORD"}
  };

  static const int wifiCount = sizeof(wifiList) / sizeof(wifiList[0]);

  // -------- TORN API KEY (FULL ACCESS) --------
  static const char* MANUAL_API_KEY = "YOUR_TORN_API_KEY";

#endif