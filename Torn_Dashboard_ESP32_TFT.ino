#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* apiKey = "YOUR_TORN_API_KEY";

TFT_eSPI tft = TFT_eSPI(); // TFT instance

bool screenOn = true;
unsigned long lastTouchTime = 0;

int screenWidth = tft.width();
int screenHeight = tft.height();

// ------------------- Cooldown Struct -------------------
struct Cooldown {
  int ticktime;          // current countdown
  int apiValue;          // last value from API
  const char* label;
  int x, y;
  unsigned long lastUpdate;
};

// ------------------- Setup cooldown X/Y -------------------
// Main cooldowns
int cooldownY = 250;
int leftX   = 10;
int centerX = screenWidth / 2 - 30;
int rightX  = screenWidth - 70;

Cooldown boosterCD = {0, 0, "Booster", leftX, cooldownY, 0};
Cooldown drugCD    = {0, 0, "Drug", centerX, cooldownY, 0};
Cooldown medicalCD = {0, 0, "Medical", rightX, cooldownY, 0};

// Extra timers: Travel, Hospital, Jail
int extraY = cooldownY + 30;
int extraLeftX   = 10;
int extraCenterX = screenWidth / 2 - 30;
int extraRightX  = screenWidth - 70;

Cooldown travelCD   = {0, 0, "Travel", extraLeftX, extraY, 0};
Cooldown hospitalCD = {0, 0, "Hospital", extraCenterX, extraY, 0};
Cooldown jailCD     = {0, 0, "Jail", extraRightX, extraY, 0};

// ------------------- Functions -------------------

// Map Torn status colors to TFT colors
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

// Draw status text with word wrapping
void drawStatus(String text, int y, uint16_t textColor, int textSize = 2, int padding = 5) {
  tft.setTextSize(textSize);
  tft.setTextColor(textColor, TFT_BLACK);
  int maxWidth = screenWidth - 2 * padding;

  int cursorX = padding;
  int cursorY = y;
  int spaceWidth = tft.textWidth(" ");

  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf(' ', start);
    if (end == -1) end = text.length();
    String word = text.substring(start, end);

    int wordWidth = tft.textWidth(word);

    if (cursorX + wordWidth > padding + maxWidth) {
      cursorX = padding;
      cursorY += 8 * textSize + 2;
    }

    tft.setCursor(cursorX, cursorY);
    tft.print(word);

    cursorX += wordWidth + spaceWidth;
    start = end + 1;
  }
}

// Draw bar (Energy, Nerve, Happy, Life)
void drawBar(int x, int y, int width, int height, float percent, uint16_t fillColor, uint16_t bgColor, int currentValue, int maxValue, const char* label) {
  tft.fillRect(x, y, width, height, bgColor);
  if (percent > 1) percent = 1;
  int fillWidth = (int)(width * percent);
  tft.fillRect(x, y, fillWidth, height, fillColor);
  tft.drawRect(x, y, width, height, TFT_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);

  char buf[16];
  sprintf(buf, "%d/%d", currentValue, maxValue);

  int textWidth = strlen(buf) * 6;
  int textHeight = 8;
  int textY = y + (height - textHeight) / 2;

  tft.setCursor(10, textY - 15);
  tft.print(label);

  tft.setCursor((screenWidth - textWidth) - 10, textY - 15);
  tft.print(buf);
}

// ------------------- Cooldown Functions -------------------
// Draw cooldowns differently depending on type
void updateCooldown(Cooldown &cd, bool hideWhenZero = false) {
    unsigned long now = millis();
    if (now - cd.lastUpdate >= 1000) {
        if (cd.ticktime > 0) cd.ticktime--;
        cd.lastUpdate = now;

        // Clear previous area
        tft.fillRect(cd.x, cd.y, 80, 20, TFT_BLACK);

        if (cd.ticktime > 0 || !hideWhenZero) {
            int hours = cd.ticktime / 3600;
            int minutes = (cd.ticktime % 3600) / 60;
            int seconds = cd.ticktime % 60;

            char buf[16];
            if (cd.ticktime == 0 && !hideWhenZero) strcpy(buf, "READY"); 
            else sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);

            tft.setTextSize(1);
            tft.setTextColor((cd.ticktime == 0 && !hideWhenZero) ? TFT_GREEN : TFT_WHITE);
            tft.setCursor(cd.x, cd.y);
            tft.print(cd.label);

            tft.setCursor(cd.x, cd.y + 12);
            tft.print(buf);
        }
    }
}

void updateCooldownFromAPI(Cooldown &cd, int newAPIValue, long serverTime = 0, bool isAbsoluteTimestamp = false) {
  if (isAbsoluteTimestamp) {
      // Calculate countdown from absolute timestamp
      long remaining = max(0L, newAPIValue - serverTime);
      cd.ticktime = remaining;
  } else {
      // Normal countdown
      cd.apiValue = newAPIValue;
      if (cd.ticktime < cd.apiValue) cd.ticktime = cd.apiValue;
  }
}
// ------------------- WiFi -------------------
void connectWiFi() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.println("Connecting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  const unsigned long timeout = 20000; // 20 sec

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
    Serial.print(".");
    delay(500);
  }

  tft.fillScreen(TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setCursor(10, 10);
    tft.println("WiFi Connected!");
    tft.setCursor(10, 40);
    tft.printf("IP: %s", WiFi.localIP().toString().c_str());
  } else {
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_RED);
    tft.println("WiFi Failed");
  }
}

// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);   // Backlight ON
  tft.fillScreen(TFT_BLACK);
  connectWiFi();
}

// ------------------- Loop -------------------
void loop() {
  uint16_t touchX, touchY;
  if (tft.getTouch(&touchX, &touchY)) {
    if (millis() - lastTouchTime > 300) {
      screenOn = !screenOn;
      lastTouchTime = millis();
      digitalWrite(TFT_BL, screenOn ? HIGH : LOW);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }

  static unsigned long lastApiUpdate = 0;
  unsigned long now = millis();

  if (now - lastApiUpdate >= 60000 || lastApiUpdate == 0) {
    lastApiUpdate = now;

    HTTPClient http;
    String url = "https://api.torn.com/user/?selections=basic,bars,travel,cooldowns,profile&key=" + String(apiKey);
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();

      DynamicJsonDocument doc(4096);
      if (deserializeJson(doc, payload)) {
        Serial.println("JSON parse error");
        http.end();
        return;
      }

      if (doc.containsKey("error")) {
        Serial.println("Torn API Error");
        http.end();
        return;
      }

      // -------- Player info --------
      String name = doc["name"] | "Unknown";
      int playerId = doc["player_id"] | 0;
      int level = doc["level"] | 0;
      String statusDesc = doc["status"]["description"] | "Idle";
      String statusCol = doc["status"]["color"] | "white";

      int energyCurrent = doc["energy"]["current"] | 0;
      int energyMax     = doc["energy"]["maximum"] | 1;
      int nerveCurrent  = doc["nerve"]["current"] | 0;
      int nerveMax      = doc["nerve"]["maximum"] | 1;
      int happyCurrent  = doc["happy"]["current"] | 0;
      int happyMax      = doc["happy"]["maximum"] | 1;
      int lifeCurrent   = doc["life"]["current"] | 0;
      int lifeMax       = doc["life"]["maximum"] | 1;

      int boosterCooldown = doc["cooldowns"]["booster"] | 0;
      int drugCooldown    = doc["cooldowns"]["drug"] | 0;
      int medicalCooldown = doc["cooldowns"]["medical"] | 0;

      int travelTime = doc["travel"]["time_left"] | 0;

      // Absolute timestamps for Hospital/Jail
      long serverTime = doc["server_time"] | 0;
      long hospitalTs = doc["states"]["hospital_timestamp"] | 0;
      long jailTs     = doc["states"]["jail_timestamp"] | 0;

      // -------- Clear and redraw --------
      tft.fillScreen(TFT_BLACK);

      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(15, 10);
      tft.print(name);

      tft.setTextSize(1);
      tft.setCursor(screenWidth - 50, 15);
      tft.printf("%d", playerId);

      tft.setCursor(15, 30);
      tft.printf("Level: %d", level);

      drawStatus(statusDesc, 50, statusColor(statusCol), 2, 15);

      // -------- Bars --------
      int barWidth = screenWidth - 20;
      int barHeight = 15;
      int startY = 120;

      drawBar(10, startY, barWidth, barHeight, (float)energyCurrent/energyMax, TFT_GREEN, TFT_DARKGREY, energyCurrent, energyMax, "Energy");
      drawBar(10, startY + 35, barWidth, barHeight, (float)nerveCurrent/nerveMax, TFT_RED, TFT_DARKGREY, nerveCurrent, nerveMax, "Nerve");
      drawBar(10, startY + 70, barWidth, barHeight, (float)happyCurrent/happyMax, TFT_YELLOW, TFT_DARKGREY, happyCurrent, happyMax, "Happy");
      drawBar(10, startY + 105, barWidth, barHeight, (float)lifeCurrent/lifeMax, TFT_BLUE, TFT_DARKGREY, lifeCurrent, lifeMax, "Life");

      // -------- Update cooldowns from API --------
      updateCooldownFromAPI(boosterCD, boosterCooldown, serverTime, true);  // absolute timestamp
      updateCooldownFromAPI(drugCD, drugCooldown, serverTime, true);  // absolute timestamp
      updateCooldownFromAPI(medicalCD, medicalCooldown, serverTime, true);  // absolute timestamp

      updateCooldownFromAPI(travelCD, travelTime, serverTime, true); // absolute timestamp
      updateCooldownFromAPI(hospitalCD, hospitalTs, serverTime, true); // absolute timestamp
      updateCooldownFromAPI(jailCD, jailTs, serverTime, true);         // absolute timestamp

    } else {
      Serial.print("HTTP request failed: ");
      Serial.println(httpCode);
    }
    http.end();
  }

  // -------- Update all timers --------
  updateCooldown(boosterCD);
  updateCooldown(drugCD);
  updateCooldown(medicalCD);
  updateCooldown(travelCD);
  updateCooldown(hospitalCD); // now correctly counts down
  updateCooldown(jailCD);

  // -------- API countdown top-right --------
  int apiCountdown = 60 - ((millis() - lastApiUpdate) / 1000);
  if (apiCountdown < 0) apiCountdown = 0;
  char buf[8];
  sprintf(buf, "%2ds", apiCountdown);
  tft.fillRect(screenWidth - 35, 30, 30, 12, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(screenWidth - 35, 30);
  tft.print(buf);

  delay(100);
}