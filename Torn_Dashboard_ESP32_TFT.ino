#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "secrets.h"

Preferences prefs;

// Active config
String wifiSSID = "";
String wifiPASS = "";
String apiKey   = "";

// ------------------- TFT Setup -------------------
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

bool screenOn = true;
unsigned long lastTouchTime = 0;
int screenWidth = 0;
int screenHeight = 0;

// ------------------- Cooldown Struct -------------------
struct Cooldown {
  int ticktime;
  long endTimestamp;
  int apiValue;
  const char* label;
  int x, y;
};

// ------------------- Cooldowns -------------------
int cooldownY = 270;
int leftX   = 20;
int centerX = 80;
int rightX  = 140;

Cooldown boosterCD  = {0, 0, 0, "Booster",  20, 270};
Cooldown drugCD     = {0, 0, 0, "Drug",     80, 270};
Cooldown medicalCD  = {0, 0, 0, "Medical", 140, 270};

int extraY = cooldownY + 25;
Cooldown travelCD   = {0, 0, 0, "Travel",   20, extraY};
Cooldown hospitalCD = {0, 0, 0, "Hospital", 80, extraY};
Cooldown jailCD     = {0, 0, 0, "Jail",    140, extraY};

// ------------------- Chain -------------------
int chainCurrent = 0;
int chainMax = 0;
int chainTimeout = 0;
int chainTimeoutTick = 0;
unsigned long lastChainUpdate = 0;

int chainCooldown = 0;
int chainCooldownTick = 0;

// ------------------- Organized Crime -------------------
long ocReadyAt = 0;
unsigned long lastOCUpdate = 0;
long ocServerBaseTime = 0;
unsigned long lastOCDraw = 0;

// ------------------- Ranked War -------------------
long rwStartAt = 0;
long rwEndAt = 0;
String opponentName = "";
String rwResult = "";
unsigned long lastRWUpdate = 0;
long rwServerBaseTime = 0;
unsigned long lastRWDraw = 0;
bool rwActive = false;
int rwScoreUs = 0;
int rwScoreEnemy = 0;

// ------------------- Torn Clock -------------------
long serverTime = 0;
unsigned long lastClockDraw = 0;

// ------------------- Settings -------------------
int APIRefreshSecond = 60;

// ------------------- Global Variables -------------------
unsigned long lastApiUpdate = 0;
unsigned long lastApiUpdateMillis = 0;
bool apiError = false;
String lastApiError = "";

String name = "Unknown";
int playerId = 0;
int level = 0;
String statusDesc = "Idle";
String statusCol = "white";

int energyCurrent = 0, energyMax = 1;
int nerveCurrent  = 0, nerveMax  = 1;
int happyCurrent  = 0, happyMax  = 1;
int lifeCurrent   = 0, lifeMax   = 1;

int boosterCooldown = 0;
int drugCooldown    = 0;
int medicalCooldown = 0;

int travelTime = 0;
long hospitalTs = 0;
long jailTs = 0;

int spacing = 25;
int barStartY = 155;
int barHeight = 10;

long moneyOnHand = 0;
int notificationsCount = 0;
int factionId = 0;
int httpCode = 0;

// ------------------- Config -------------------
bool loadConfig() {
#if MANUAL_FLASH
  if (wifiCount > 0) {
    wifiSSID = wifiList[0].ssid;
    wifiPASS = wifiList[0].password;
  } else {
    wifiSSID = "";
    wifiPASS = "";
  }

  apiKey = String(MANUAL_API_KEY);

  Serial0.println("Loaded MANUAL config:");
  Serial0.println("SSID: " + wifiSSID);
  Serial0.println("API: " + apiKey);

  return (wifiSSID.length() > 0 && apiKey.length() > 0);
#else
  prefs.begin("config", true);

  wifiSSID = prefs.getString("ssid", "");
  wifiPASS = prefs.getString("pass", "");
  apiKey   = prefs.getString("api", "");

  prefs.end();

  Serial0.println("Loaded WEB config:");
  Serial0.println("SSID: " + wifiSSID);
  Serial0.println("API: " + apiKey);

  return (wifiSSID.length() > 0 && apiKey.length() > 0);
#endif
}

void saveConfig(const String& ssid, const String& pass, const String& api) {
#if MANUAL_FLASH
  Serial0.println("Manual mode active - config save skipped");
#else
  prefs.begin("config", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("api", api);
  prefs.end();
#endif
}

void waitForConfig() {
#if MANUAL_FLASH
  Serial0.println("Manual mode active - skipping waitForConfig");
  return;
#else
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Waiting Config...");

  String buffer = "";
  unsigned long lastAnnounce = 0;

  while (true) {
    if (millis() - lastAnnounce >= 1000) {
      Serial0.println("WAITING_CONFIG");
      lastAnnounce = millis();
    }

    while (Serial0.available()) {
      char c = Serial0.read();

      if (c == '\r') continue;

      if (c == '\n') {
        buffer.trim();

        DynamicJsonDocument doc(256);
        DeserializationError err = deserializeJson(doc, buffer);

        if (err) {
          Serial0.println("JSON_ERROR");
        } else {
          String newSSID = doc["ssid"] | "";
          String newPASS = doc["pass"] | "";
          String newAPI  = doc["api"]  | "";

          if (newSSID.length() > 0 && newAPI.length() > 0) {
            saveConfig(newSSID, newPASS, newAPI);

            wifiSSID = newSSID;
            wifiPASS = newPASS;
            apiKey   = newAPI;

            Serial0.println("CONFIG_SAVED");

            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN);
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.println("Config Saved");

            delay(1500);
            ESP.restart();
          } else {
            Serial0.println("MISSING_FIELDS");
          }
        }

        buffer = "";
      } else {
        buffer += c;
      }
    }

    delay(10);
  }
#endif
}

// ------------------- Utility Functions -------------------
uint16_t statusColor(String color) {
  color.toLowerCase();
  if (color == "green") return TFT_GREEN;
  if (color == "red") return TFT_RED;
  if (color == "blue") return TFT_BLUE;
  if (color == "yellow") return TFT_YELLOW;
  if (color == "orange") return TFT_ORANGE;
  if (color == "purple") return TFT_PURPLE;
  return TFT_WHITE;
}

String formatMoney(long amount) {
  String s = String(amount);
  int len = s.length();
  for (int i = len - 3; i > 0; i -= 3) {
    s = s.substring(0, i) + "," + s.substring(i);
  }
  return s;
}

// ------------------- Draw Functions -------------------
void drawStatus(String text, int y, uint16_t textColor, int textSize = 2, int padding = 5) {
  sprite.setTextSize(textSize);
  sprite.setTextColor(textColor, TFT_BLACK);

  int maxWidth = screenWidth - 2 * padding;
  int cursorX = padding;
  int cursorY = y;
  int spaceWidth = sprite.textWidth(" ");

  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf(' ', start);
    if (end == -1) end = text.length();
    String word = text.substring(start, end);

    if (cursorX + sprite.textWidth(word) > padding + maxWidth) {
      cursorX = padding;
      cursorY += 8 * textSize + 2;
    }

    sprite.setCursor(cursorX, cursorY);
    sprite.print(word);
    cursorX += sprite.textWidth(word) + spaceWidth;
    start = end + 1;
  }
}

void drawBar(int x, int y, int width, int height, float percent, uint16_t fillColor, uint16_t bgColor, int currentValue, int maxValue, const char* label) {
  if (percent > 1) percent = 1;
  if (percent < 0) percent = 0;

  sprite.fillRect(x, y, width, height, bgColor);
  sprite.fillRect(x, y, (int)(width * percent), height, fillColor);
  sprite.drawRect(x, y, width, height, TFT_WHITE);

  sprite.setTextSize(1);
  sprite.setTextColor(TFT_WHITE);

  char buf[16];
  sprintf(buf, "%d/%d", currentValue, maxValue);

  int textWidth = strlen(buf) * 6;
  int textHeight = 8;
  int textY = y + (height - textHeight) / 2;

  sprite.setCursor(10, textY - 12);
  sprite.print(label);

  sprite.setCursor((screenWidth - textWidth) - 10, textY - 12);
  sprite.print(buf);
}

// ------------------- Cooldown Functions -------------------
void updateCooldown(Cooldown &cd, bool hideWhenZero = false) {
  long currentServerTime = serverTime + (millis() - lastApiUpdateMillis) / 1000.0;
  long remaining = cd.endTimestamp - currentServerTime;
  if (remaining < 0) remaining = 0;

  sprite.fillRect(cd.x, cd.y, 80, 20, TFT_BLACK);

  if (remaining > 0 || !hideWhenZero) {
    int hours = remaining / 3600;
    int minutes = (remaining % 3600) / 60;
    int seconds = remaining % 60;

    char buf[16];

    if (remaining == 0 && !hideWhenZero) strcpy(buf, "READY");
    else sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);

    sprite.setTextSize(1);
    sprite.setTextColor(TFT_WHITE);
    sprite.setCursor(cd.x, cd.y);
    sprite.print(cd.label);

    sprite.setTextColor(remaining == 0 ? TFT_GREEN : TFT_WHITE);
    sprite.setCursor(cd.x, cd.y + 10);
    sprite.print(buf);
  }
}

void updateCooldownFromAPI(Cooldown &cd, int newValue, long serverTime = 0, bool isAbsolute = false) {
  if (isAbsolute) cd.endTimestamp = newValue;
  else cd.endTimestamp = serverTime + newValue;

  cd.apiValue = newValue;
}

// ------------------- WiFi -------------------
void connectWiFi() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(15, 10);
  tft.println("Connecting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

#if MANUAL_FLASH
  bool connected = false;

  for (int i = 0; i < wifiCount; i++) {
    Serial0.print("Trying WiFi: ");
    Serial0.println(wifiList[i].ssid);

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(15, 10);
    tft.println("Connecting WiFi...");
    tft.setTextSize(1);
    tft.setCursor(15, 40);
    tft.print("Trying: ");
    tft.println(wifiList[i].ssid);

    WiFi.disconnect(true);
    delay(300);
    WiFi.begin(wifiList[i].ssid, wifiList[i].password);

    unsigned long startTime = millis();
    while (millis() - startTime < 10000) {
      if (WiFi.status() == WL_CONNECTED) {
        wifiSSID = wifiList[i].ssid;
        wifiPASS = wifiList[i].password;
        connected = true;
        break;
      }
      delay(500);
    }

    if (connected) break;
  }

  if (connected) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(15, 10);
    tft.println("WiFi Connected!");
    tft.setTextSize(1);
    tft.setCursor(15, 35);
    tft.print("SSID: ");
    tft.println(wifiSSID);
    tft.setCursor(15, 60);
    tft.print("IP: ");
    tft.println(WiFi.localIP());
    return;
  }

#else
  if (wifiSSID.length() == 0) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(15, 40);
    tft.println("No SSID saved");
    return;
  }

  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  unsigned long startTime = millis();
  while (millis() - startTime < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
      tft.setCursor(15, 10);
      tft.println("WiFi Connected!");
      tft.setTextSize(1);
      tft.setCursor(15, 35);
      tft.print("SSID: ");
      tft.println(wifiSSID);
      tft.setCursor(15, 60);
      tft.print("IP: ");
      tft.println(WiFi.localIP());
      return;
    }
    delay(500);
  }
#endif

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED);
  tft.setCursor(15, 10);
  tft.println("WiFi Failed!");
}

// ------------------- Setup -------------------
void setup() {
  Serial0.begin(115200);
  delay(1500);

#if MANUAL_FLASH
  Serial0.println("MODE = MANUAL");
#else
  Serial0.println("MODE = WEB");
#endif

  tft.init();
  tft.setRotation(0);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  screenWidth = tft.width();
  screenHeight = tft.height();

  centerX = screenWidth / 2 - 21;
  rightX  = screenWidth - 60;

  boosterCD.x = leftX;
  drugCD.x = centerX;
  medicalCD.x = rightX;
  travelCD.x = leftX;
  hospitalCD.x = centerX;
  jailCD.x = rightX;

  sprite.setColorDepth(8);
  sprite.createSprite(screenWidth, screenHeight);
  sprite.fillScreen(TFT_BLACK);

  if (!loadConfig()) {
#if MANUAL_FLASH
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Manual config");
    tft.setCursor(10, 35);
    tft.println("missing");
    while (true) delay(100);
#else
    waitForConfig();
#endif
  }

  connectWiFi();
}

WiFiClientSecure client;

// ------------------- Main Loop -------------------
void loop() {
  // Disable touch for debug if needed:
  // uint16_t touchX, touchY;
  // if (tft.getTouch(&touchX, &touchY) && millis() - lastTouchTime > 300) {
  //   screenOn = !screenOn;
  //   digitalWrite(TFT_BL, screenOn ? HIGH : LOW);
  //   lastTouchTime = millis();
  // }

  uint16_t touchX, touchY;
  if (tft.getTouch(&touchX, &touchY) && millis() - lastTouchTime > 300) {
    screenOn = !screenOn;
    digitalWrite(TFT_BL, screenOn ? HIGH : LOW);
    lastTouchTime = millis();
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }

  unsigned long now = millis();

  if (now - lastApiUpdate >= (APIRefreshSecond * 1000) || lastApiUpdate == 0) {
    lastApiUpdate = now;

    httpCode = 0;
    HTTPClient http;
    http.begin("https://api.torn.com/user/?selections=basic,bars,travel,cooldowns,notifications,money,profile&key=" + apiKey);
    http.setTimeout(2000);
    httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(4096);
      doc.clear();

      if (deserializeJson(doc, payload)) {
        apiError = true;
        lastApiError = "JSON Parse Error";
      }

      if (!doc.containsKey("error")) {
        apiError = false;
        name = doc["name"] | "Unknown";
        playerId = doc["player_id"] | 0;
        level = doc["level"] | 0;
        statusDesc = doc["status"]["description"] | "Idle";
        statusCol = doc["status"]["color"] | "white";

        energyCurrent = doc["energy"]["current"] | 0;
        energyMax     = doc["energy"]["maximum"] | 1;
        nerveCurrent  = doc["nerve"]["current"] | 0;
        nerveMax      = doc["nerve"]["maximum"] | 1;
        happyCurrent  = doc["happy"]["current"] | 0;
        happyMax      = doc["happy"]["maximum"] | 1;
        lifeCurrent   = doc["life"]["current"] | 0;
        lifeMax       = doc["life"]["maximum"] | 1;

        boosterCooldown = doc["cooldowns"]["booster"] | 0;
        drugCooldown    = doc["cooldowns"]["drug"] | 0;
        medicalCooldown = doc["cooldowns"]["medical"] | 0;

        travelTime = doc["travel"]["time_left"] | 0;

        serverTime = doc["server_time"] | 0;

        hospitalTs = 0;
        jailTs = 0;
        if (doc.containsKey("states")) {
          hospitalTs = doc["states"]["hospital_timestamp"] | 0;
          jailTs     = doc["states"]["jail_timestamp"] | 0;
        }

        if (doc.containsKey("money_onhand")) {
          moneyOnHand = doc["money_onhand"] | 0;
        }

        if (doc.containsKey("faction")) {
          factionId = doc["faction"]["faction_id"] | 0;
        }

        if (doc.containsKey("notifications")) {
          JsonObject notif = doc["notifications"].as<JsonObject>();
          notificationsCount = 0;
          notificationsCount += notif["messages"] | 0;
          notificationsCount += notif["events"] | 0;
          notificationsCount += notif["awards"] | 0;
          notificationsCount += notif["competition"] | 0;
        }

        lastApiUpdateMillis = millis();
      } else {
        apiError = true;
        lastApiError = doc["error"]["error"].as<String>();
      }
    } else {
      apiError = true;
      lastApiError = "User API HTTP failed: " + String(httpCode);
    }
    http.end();

    client.setInsecure();

    httpCode = 0;
    HTTPClient httpChain;
    String url = "https://api.torn.com/v2/faction/chain";
    httpChain.begin(client, url);
    httpChain.addHeader("Authorization", "ApiKey " + apiKey);
    httpChain.addHeader("accept", "application/json");
    httpChain.setTimeout(5000);
    httpCode = httpChain.GET();
    chainCurrent = 0;
    chainMax = 0;
    chainTimeout = 0;

    if (httpCode > 0) {
      String payload = httpChain.getString();
      DynamicJsonDocument doc(2048);
      doc.clear();

      if (!deserializeJson(doc, payload)) {
        if (doc.containsKey("chain")) {
          chainCurrent = doc["chain"]["current"] | 0;
          chainMax     = doc["chain"]["max"] | 0;
          chainTimeout = doc["chain"]["timeout"] | 0;
          chainCooldown = doc["chain"]["cooldown"] | 0;

          if (chainCooldown > 0 && serverTime > 0) {
            chainCooldownTick = max(0L, chainCooldown - serverTime);
          } else {
            chainCooldownTick = 0;
          }

          chainTimeoutTick = chainTimeout;
          lastChainUpdate = millis();
        }
      }
    }
    httpChain.end();

    httpCode = 0;
    HTTPClient httpOC;
    httpOC.begin(client, "https://api.torn.com/v2/user/organizedcrime");
    httpOC.addHeader("Authorization", "ApiKey " + apiKey);
    httpOC.addHeader("accept", "application/json");
    httpOC.setTimeout(5000);
    httpCode = httpOC.GET();

    if (httpCode > 0) {
      String payload = httpOC.getString();
      DynamicJsonDocument doc(2048);
      doc.clear();

      if (!deserializeJson(doc, payload)) {
        ocReadyAt = doc["organizedCrime"]["ready_at"] | 0;
        ocServerBaseTime = serverTime;
        lastOCUpdate = millis();
      }
    }
    httpOC.end();

    httpCode = 0;
    HTTPClient httpRW;
    httpRW.begin(client, "https://api.torn.com/v2/faction/rankedwars?offset=0&limit=1&sort=DESC");
    httpRW.addHeader("Authorization", "ApiKey " + apiKey);
    httpRW.addHeader("accept", "application/json");
    httpRW.setTimeout(5000);
    httpCode = httpRW.GET();

    if (httpCode > 0) {
      String payload = httpRW.getString();
      DynamicJsonDocument doc(4096);
      doc.clear();

      if (!deserializeJson(doc, payload)) {
        if (doc.containsKey("rankedwars") && doc["rankedwars"].size() > 0) {
          JsonObject war = doc["rankedwars"][0];
          rwStartAt = war["start"] | 0;
          rwEndAt = war["end"] | 0;
          rwActive = war["winner"].isNull();

          if (war.containsKey("factions")) {
            JsonArray factions = war["factions"];

            if (factions.size() >= 2) {
              JsonObject faction1 = factions[0];
              JsonObject faction2 = factions[1];

              int id1 = faction1["id"] | 0;
              int id2 = faction2["id"] | 0;
              int score1 = faction1["score"] | 0;
              int score2 = faction2["score"] | 0;

              if (id1 == factionId) {
                rwScoreUs = score1;
                rwScoreEnemy = score2;
              } else {
                rwScoreUs = score2;
                rwScoreEnemy = score1;
              }
            }
          }

          rwServerBaseTime = serverTime;
          lastRWUpdate = millis();
        }
      }
    }
    httpRW.end();

    updateCooldownFromAPI(boosterCD, boosterCooldown, serverTime);
    updateCooldownFromAPI(drugCD, drugCooldown, serverTime);
    updateCooldownFromAPI(medicalCD, medicalCooldown, serverTime);
    updateCooldownFromAPI(travelCD, travelTime, serverTime);
    updateCooldownFromAPI(hospitalCD, hospitalTs, serverTime, true);
    updateCooldownFromAPI(jailCD, jailTs, serverTime, true);

    sprite.fillScreen(TFT_BLACK);

    if (apiError) {
      sprite.setTextColor(TFT_RED, TFT_BLACK);
      sprite.setTextSize(2);
      sprite.setCursor(10, 10);
      sprite.println("API Error");

      sprite.setTextSize(1);
      sprite.setTextColor(TFT_WHITE, TFT_BLACK);
      sprite.setCursor(10, 40);
      sprite.println(lastApiError);
    } else {
      sprite.setTextSize(2);
      sprite.setTextColor(TFT_WHITE);
      sprite.setCursor(15, 10);
      sprite.print(name);

      sprite.setTextSize(1);
      sprite.setTextColor(TFT_WHITE);
      int idWidth = sprite.textWidth(String(playerId));
      sprite.setCursor(screenWidth - idWidth - 10, 15);
      sprite.print(playerId);

      sprite.setCursor(15, 30);
      sprite.printf("Level: %d", level);

      int notifY = 43;
      sprite.setCursor(15, notifY);
      sprite.setTextSize(1);
      if (notificationsCount > 0) {
        int badgeRadius = 6;
        int textW = sprite.textWidth(String(notificationsCount));
        int badgeX = 15 + sprite.textWidth("Notifications: ") + textW / 2;
        int badgeY = notifY + 4;
        sprite.fillCircle(badgeX, badgeY, badgeRadius, TFT_RED);
        sprite.setTextColor(TFT_WHITE);
        sprite.setCursor(badgeX - textW / 2, badgeY - 3);
        sprite.print(notificationsCount);
        sprite.setTextColor(TFT_WHITE);
        sprite.setCursor(15, notifY);
        sprite.print("Notifications:");
      } else {
        sprite.setTextColor(TFT_WHITE);
        sprite.setCursor(15, notifY);
        sprite.printf("Notifications: %d", notificationsCount);
      }

      sprite.setTextColor(TFT_WHITE);
      sprite.setCursor(15, 55);
      sprite.print("Money: ");
      int labelWidth = sprite.textWidth("Money: ");
      sprite.setTextColor(TFT_GREEN);
      sprite.setCursor(10 + labelWidth, 55);
      sprite.print("$" + formatMoney(moneyOnHand));

      drawStatus(statusDesc, 91, statusColor(statusCol), 2, 15);

      int barWidth = screenWidth - 20;
      int spacing = 25;
      uint16_t chainColor = (chainCooldownTick > 0) ? TFT_CYAN : TFT_LIGHTGREY;
      float chainPercent = chainMax > 0 ? (float)chainCurrent / chainMax : 0;

      drawBar(10, barStartY, barWidth, barHeight, (float)energyCurrent / energyMax, TFT_GREEN, TFT_DARKGREY, energyCurrent, energyMax, "Energy");
      drawBar(10, barStartY + spacing, barWidth, barHeight, (float)nerveCurrent / nerveMax, TFT_RED, TFT_DARKGREY, nerveCurrent, nerveMax, "Nerve");
      drawBar(10, barStartY + spacing * 2, barWidth, barHeight, (float)happyCurrent / happyMax, TFT_YELLOW, TFT_DARKGREY, happyCurrent, happyMax, "Happy");
      drawBar(10, barStartY + spacing * 3, barWidth, barHeight, (float)lifeCurrent / lifeMax, TFT_BLUE, TFT_DARKGREY, lifeCurrent, lifeMax, "Life");
      drawBar(10, barStartY + spacing * 4, barWidth, barHeight, chainPercent, chainColor, TFT_DARKGREY, chainCurrent, chainMax, "Chain");
    }
  }

  updateCooldown(boosterCD);
  updateCooldown(drugCD);
  updateCooldown(medicalCD);
  updateCooldown(travelCD);
  updateCooldown(hospitalCD);
  updateCooldown(jailCD);

  if (millis() - lastChainUpdate >= 1000) {
    lastChainUpdate = millis();

    if (chainCooldownTick > 0) chainCooldownTick--;
    else if (chainTimeoutTick > 0) chainTimeoutTick--;

    int activeTimer = (chainCooldownTick > 0) ? chainCooldownTick : chainTimeoutTick;

    int startY = 155;
    int spacing = 25;
    int chainBarY = startY + spacing * 4 - 12;
    int textY = chainBarY + (barHeight - 8) / 2;
    int labelX = 10;
    int valueX = screenWidth - 10 - (String(0) + "/" + String(10)).length() * 6;
    int timerX = labelX + (valueX - labelX) / 2;

    sprite.fillRect(timerX, textY, 60, 10, TFT_BLACK);

    if (activeTimer > 0) {
      char timerBuf[12];

      if (chainCooldownTick > 0) {
        int hours = activeTimer / 3600;
        int minutes = (activeTimer % 3600) / 60;
        int seconds = activeTimer % 60;
        sprite.setTextColor(TFT_CYAN);
        sprintf(timerBuf, "%02d:%02d:%02d", hours, minutes, seconds);
      } else {
        int minutes = activeTimer / 60;
        int seconds = activeTimer % 60;
        sprite.setTextColor(TFT_WHITE);
        sprintf(timerBuf, "%02d:%02d", minutes, seconds);
      }

      sprite.setTextSize(1);
      sprite.setCursor(timerX, textY);
      sprite.print(timerBuf);
    }
  }

  if (millis() - lastOCDraw >= 1000) {
    lastOCDraw = millis();

    long currentServerTime = ocServerBaseTime + (millis() - lastOCUpdate) / 1000.0;
    long remaining = ocReadyAt - currentServerTime;
    if (remaining < 0) remaining = 0;

    long days = remaining / 86400;
    long hours = (remaining % 86400) / 3600;
    long minutes = (remaining % 3600) / 60;
    long seconds = remaining % 60;

    char timeBuf[20];
    sprintf(timeBuf, "%02ld:%02ld:%02ld:%02ld", days, hours, minutes, seconds);

    sprite.fillRect(15, 67, 120, 12, TFT_BLACK);
    sprite.setTextSize(1);
    sprite.setTextColor(TFT_WHITE);
    sprite.setCursor(15, 67);
    sprite.print("OC: ");
    sprite.setTextColor(remaining == 0 ? TFT_GREEN : TFT_WHITE);
    sprite.print(timeBuf);
  }

  if (rwStartAt > 0 && millis() - lastRWDraw >= 1000) {
    lastRWDraw = millis();

    long currentServerTime = serverTime + (millis() - lastApiUpdateMillis) / 1000.0;
    long displayTime = 0;
    bool warRunningNow = false;

    if (currentServerTime >= rwStartAt && (rwEndAt == 0 || currentServerTime <= rwEndAt)) {
      warRunningNow = true;
    }

    if (warRunningNow) displayTime = currentServerTime - rwStartAt;
    else displayTime = max(0L, rwStartAt - currentServerTime);

    long days = displayTime / 86400;
    long hours = (displayTime % 86400) / 3600;
    long minutes = (displayTime % 3600) / 60;
    long seconds = displayTime % 60;

    char rwBuf[20];
    sprintf(rwBuf, "%02ld:%02ld:%02ld:%02ld", days, hours, minutes, seconds);

    sprite.fillRect(15, 79, 120, 12, TFT_BLACK);
    sprite.setTextSize(1);
    sprite.setTextColor(TFT_WHITE);
    sprite.setCursor(15, 79);
    sprite.print("RW: ");

    uint16_t rwColor = TFT_WHITE;
    if (warRunningNow) rwColor = TFT_RED;
    else if (displayTime <= 3600 && displayTime != 0) rwColor = TFT_YELLOW;
    else if (displayTime == 0) rwColor = TFT_GREEN;

    sprite.setTextColor(rwColor);
    sprite.print(rwBuf);

    if (warRunningNow) {
      char scoreBuf[16];
      sprintf(scoreBuf, "%d:%d", rwScoreUs, rwScoreEnemy);

      int scoreWidth = sprite.textWidth(scoreBuf);
      int scoreX = screenWidth - scoreWidth - 10;

      uint16_t scoreColor = TFT_YELLOW;
      if (rwScoreUs > rwScoreEnemy) scoreColor = TFT_GREEN;
      if (rwScoreUs < rwScoreEnemy) scoreColor = TFT_RED;

      sprite.setTextColor(scoreColor);
      sprite.setCursor(scoreX, 79);
      sprite.print(scoreBuf);
    }
  }

  if (serverTime > 0 && millis() - lastClockDraw >= 1000) {
    lastClockDraw = millis();

    long currentServerTime = serverTime + (millis() - lastApiUpdateMillis) / 1000.0;
    time_t rawTime = currentServerTime;
    struct tm *timeinfo = gmtime(&rawTime);

    char timeBuffer[16];
    sprintf(timeBuffer, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    int timerWidth = sprite.textWidth(timeBuffer);
    int timerX = screenWidth - timerWidth - 10;

    sprite.fillRect(timerX, 30, timerWidth, 10, TFT_BLACK);
    sprite.setTextSize(1);
    sprite.setTextColor(TFT_WHITE);
    sprite.setCursor(timerX, 30);
    sprite.print(timeBuffer);
  }

  int apiCountdown = APIRefreshSecond - ((millis() - lastApiUpdate) / 1000.0);
  if (apiCountdown < 0) apiCountdown = 0;
  char buf[8];
  sprintf(buf, "%3ds", apiCountdown);

  sprite.fillRect(screenWidth - sprite.textWidth(buf) - 10, 45, sprite.textWidth(buf), 12, TFT_BLACK);
  sprite.setTextSize(1);
  sprite.setTextColor(TFT_WHITE);
  sprite.setCursor(screenWidth - sprite.textWidth(buf) - 10, 45);
  sprite.print(buf);

  if (apiError) sprite.fillCircle(screenWidth - 8, 8, 4, TFT_RED);
  else sprite.fillCircle(screenWidth - 8, 8, 4, TFT_BLACK);

  sprite.pushSprite(0, 0);

  delay(100);
}