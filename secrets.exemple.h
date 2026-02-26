#pragma once

struct WifiCred {
  const char* ssid;
  const char* password;
};

WifiCred wifiList[] = {
  {"SSID", "PASSWORD"},
  {"SSID_2", "PASSWORD"},
  {"SSID_3", "PASSWORD"}
};

const char* apiKey = "YOUR_TORN_API_KEY";

const int wifiCount = sizeof(wifiList) / sizeof(wifiList[0]);
