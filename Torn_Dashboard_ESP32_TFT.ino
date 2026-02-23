#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

//  -- Wifi ---
struct WifiCred {
  const char* ssid;
  const char* password;
};

WifiCred wifiList[] = {
  {"YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD"}
};

const char* apiKey = "YOUR_TORN_API_KEY";

const int wifiCount = sizeof(wifiList) / sizeof(wifiList[0]);
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
int cooldownY = 270;
int leftX   = 10;
int centerX = screenWidth / 2 - 30;
int rightX  = screenWidth - 70;

Cooldown boosterCD = {0, 0, "Booster", leftX, cooldownY, 0};
Cooldown drugCD    = {0, 0, "Drug", centerX, cooldownY, 0};
Cooldown medicalCD = {0, 0, "Medical", rightX, cooldownY, 0};

// Extra timers: Travel, Hospital, Jail
int extraY = cooldownY + 25;
int extraLeftX   = 10;
int extraCenterX = screenWidth / 2 - 30;
int extraRightX  = screenWidth - 70;

Cooldown travelCD   = {0, 0, "Travel", extraLeftX, extraY, 0};
Cooldown hospitalCD = {0, 0, "Hospital", extraCenterX, extraY, 0};
Cooldown jailCD     = {0, 0, "Jail", extraRightX, extraY, 0};

// ------------------- Chain timer -------------------
int chainTimeoutTick = 0;
unsigned long lastChainUpdate = 0;

// ------------------- Organized Crime timer -------------------
long ocReadyAt = 0;                 // ready_at timestamp
long ocServerBaseTime = 0;          // Torn server_time at fetch
unsigned long ocMillisBase = 0;     // millis() at fetch
unsigned long lastOCDraw = 0;       // for 1 sec refresh

// ------------------- Functions -------------------
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

// Draw bar (Energy, Nerve, Happy, Life, Chain)
void drawBar(int x, int y, int width, int height, float percent, uint16_t fillColor, uint16_t bgColor, int currentValue, int maxValue, const char* label) {
  int spacing = 12;
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
  int textY = y + (height - textHeight)/2;

  tft.setCursor(10, textY - spacing);
  tft.print(label);

  tft.setCursor((screenWidth - textWidth) - 10, textY - spacing);
  tft.print(buf);
}

// ------------------- Cooldown Functions -------------------
void updateCooldown(Cooldown &cd, bool hideWhenZero = false) {
    unsigned long now = millis();
    if (now - cd.lastUpdate >= 1000) {
        if (cd.ticktime > 0) cd.ticktime--;
        cd.lastUpdate = now;

        // Clear area under label (20px high row)
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

            // Draw label
            tft.setCursor(cd.x, cd.y);
            tft.print(cd.label);

            // Draw timer under label
            tft.setCursor(cd.x, cd.y + 10); // 10px below label
            tft.print(buf);
        }
    }
}

void updateCooldownFromAPI(Cooldown &cd, int newValue, long serverTime = 0, bool isAbsolute = false) {
  if (isAbsolute) {
    long remaining = max(0L, newValue - serverTime);
    cd.ticktime = remaining;
  } else {
    cd.ticktime = max(0, newValue);
  }
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

  for (int i = 0; i < wifiCount; i++) {
    tft.fillRect(0, 40, screenWidth, 45, TFT_BLACK);
    int padding = 15;
    tft.setCursor(padding, 40);
    tft.setTextSize(2);
    tft.print("Trying:");
    tft.setCursor(padding, 65);
    tft.setTextSize(2);
    tft.print(wifiList[i].ssid);

    WiFi.begin(wifiList[i].ssid, wifiList[i].password);

    unsigned long startTime = millis();
    const unsigned long timeout = 10000;
    while (millis() - startTime < timeout) {
      if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(padding, 10);
        tft.setTextSize(2);
        tft.println("WiFi Connected!");
        tft.setCursor(padding, 40);
        tft.setTextSize(2);
        tft.printf("SSID: %s", wifiList[i].ssid);
        tft.setCursor(padding, 65);
        tft.setTextSize(2);
        tft.printf("IP: %s", WiFi.localIP().toString().c_str());
        return;
      }
      delay(500);
    }
    WiFi.disconnect(false);
    delay(500);
  }
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED);
  tft.setCursor(15, 10);
  tft.setTextSize(2);
  tft.println("No WiFi Found!");
}

String formatMoney(long amount) {
    String s = String(amount);
    int len = s.length();
    for (int i = len - 3; i > 0; i -= 3) {
        s = s.substring(0, i) + "," + s.substring(i);
    }
    return s;
}

// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
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
  static unsigned long lastApiUpdateMillis = 0; // ESP millis at last API fetch
  static long lastServerTime = 0;               // Torn server time at last API fetch

  if (now - lastApiUpdate >= 60000 || lastApiUpdate == 0) {
    lastApiUpdate = now;

    // -------- Player API --------
    HTTPClient http;
    String url = "https://api.torn.com/user/?selections=basic,bars,travel,cooldowns,notifications,money,profile&key=" + String(apiKey);
    http.begin(url);
    int httpCode = http.GET();

    int energyCurrent = 0, energyMax = 1;
    int nerveCurrent = 0, nerveMax = 1;
    int happyCurrent = 0, happyMax = 1;
    int lifeCurrent = 0, lifeMax = 1;
    int boosterCooldown = 0, drugCooldown = 0, medicalCooldown = 0;
    int travelTime = 0;
    long serverTime = 0, hospitalTs = 0, jailTs = 0;
    String name = "Unknown";
    int playerId = 0, level = 0;
    String statusDesc = "Idle", statusCol = "white";
    int notificationsCount = 0;
    long moneyOnHand = 0;
    long organizedCrimeReadyAt = 0;

    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(4096);
      if (deserializeJson(doc, payload)) { http.end(); return; }
      if (!doc.containsKey("error")) {
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
        hospitalTs = doc["states"]["hospital_timestamp"] | 0;
        jailTs     = doc["states"]["jail_timestamp"] | 0;

        if (doc.containsKey("money_onhand")) {
          moneyOnHand = doc["money_onhand"] | 0;
        }

        // -------- Notifications --------
        if (doc.containsKey("notifications")) {
          JsonObject notif = doc["notifications"].as<JsonObject>();
          notificationsCount += notif["messages"] | 0;
          notificationsCount += notif["events"] | 0;
          notificationsCount += notif["awards"] | 0;
          notificationsCount += notif["competition"] | 0;
        }

        lastServerTime = serverTime;        // seconds from Torn API
        lastApiUpdateMillis = millis();     // ESP millis when we got the server time
      }
    }
    http.end();

    // -------- Chain API --------
    int chainCurrent = 0, chainMax = 1, chainTimeout = 0;
    HTTPClient chainHttp;
    String chainUrl = "https://api.torn.com/faction/8606?selections=chain&key=" + String(apiKey);
    chainHttp.begin(chainUrl);
    int chainCode = chainHttp.GET();
    if (chainCode > 0) {
      String chainPayload = chainHttp.getString();
      DynamicJsonDocument chainDoc(1024);
      if (!deserializeJson(chainDoc, chainPayload)) {
        if (chainDoc.containsKey("chain")) {
          chainCurrent = chainDoc["chain"]["current"] | 0;
          chainMax     = chainDoc["chain"]["max"] | 10;
          chainTimeout = chainDoc["chain"]["timeout"] | 0;
        }
      }
    }
    chainHttp.end();

    // -------- V2 Organized Crime API --------
    HTTPClient ocHttp;
    String ocUrl = "https://api.torn.com/v2/user/organizedcrime";
    ocHttp.begin(ocUrl);
    ocHttp.addHeader("accept", "application/json");
    ocHttp.addHeader("Authorization", "ApiKey " + String(apiKey));

    int ocCode = ocHttp.GET();
    if (ocCode > 0) {
        String ocPayload = ocHttp.getString();
        DynamicJsonDocument ocDoc(2048);

        if (!deserializeJson(ocDoc, ocPayload)) {

            // Adjust path if API structure differs
            if (ocDoc.containsKey("organizedCrime")) {
                JsonObject ocObj = ocDoc["organizedCrime"];

                if (ocObj.containsKey("ready_at")) {
                    ocReadyAt = ocObj["ready_at"] | 0;

                    // Sync with Torn server time
                    ocServerBaseTime = serverTime;
                    ocMillisBase = millis();
                }
            }
        }
    }
    ocHttp.end();

    // -------- Set chain countdown for smooth update --------
    chainTimeoutTick = chainTimeout;
    lastChainUpdate = millis();

    // -------- Clear screen & draw info --------
    tft.fillScreen(TFT_BLACK);

    // Player name
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(15, 10);
    tft.print(name);

    // Player ID
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);

    String idStr = String(playerId);                 // convert number to string
    int textWidth = tft.textWidth(idStr);           // get width in pixels
    int padding = 10;                               // padding from right edge
    int cursorX = screenWidth - textWidth - padding; // calculate starting X

    tft.setCursor(cursorX, 15);
    tft.print(idStr);

    // Level
    tft.setCursor(15, 30);
    tft.printf("Level: %d", level);

    // Notifications count
    int notifY = 45;
    tft.setCursor(15, notifY);
    tft.setTextSize(1);

    if (notificationsCount > 0) {
        // Draw red badge
        int badgeRadius = 6;
        int textWidth = tft.textWidth(String(notificationsCount));
        int badgeX = 15 + tft.textWidth("Notifications: ") + textWidth/2;
        int badgeY = notifY + 4;
        tft.fillCircle(badgeX, badgeY, badgeRadius, TFT_RED);

        // Draw number inside badge
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(badgeX - textWidth/2, badgeY - 3);
        tft.print(notificationsCount);
        
        // Draw the label only, no white number
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(15, notifY);
        tft.print("Notifications:");
    } else {
        // No badge, just print the label + count normally
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(15, notifY);
        tft.printf("Notifications: %d", notificationsCount);
    }

    // Draw label in white
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(15, 60);
    tft.print("Money: ");

    // Draw value in green, right after label
    int labelWidth = tft.textWidth("Money: ");
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(10 + labelWidth, 60);
    tft.print("$" + formatMoney(moneyOnHand));
        
    // Status
    drawStatus(statusDesc, 90, statusColor(statusCol), 2, 15);

    // -------- Bars --------
    int barWidth = screenWidth - 20;
    int barHeight = 10;
    int spacing = 25;
    int startY = 155;

    drawBar(10, startY, barWidth, barHeight, (float)energyCurrent/energyMax, TFT_GREEN, TFT_DARKGREY, energyCurrent, energyMax, "Energy");
    drawBar(10, startY + spacing, barWidth, barHeight, (float)nerveCurrent/nerveMax, TFT_RED, TFT_DARKGREY, nerveCurrent, nerveMax, "Nerve");
    drawBar(10, startY + spacing*2, barWidth, barHeight, (float)happyCurrent/happyMax, TFT_YELLOW, TFT_DARKGREY, happyCurrent, happyMax, "Happy");
    drawBar(10, startY + spacing*3, barWidth, barHeight, (float)lifeCurrent/lifeMax, TFT_BLUE, TFT_DARKGREY, lifeCurrent, lifeMax, "Life");
    drawBar(10, startY + spacing*4, barWidth, barHeight, (float)chainCurrent/chainMax, TFT_LIGHTGREY, TFT_DARKGREY, chainCurrent, chainMax, "Chain");

    // Update cooldowns
    updateCooldownFromAPI(boosterCD, boosterCooldown);
    updateCooldownFromAPI(drugCD, drugCooldown);
    updateCooldownFromAPI(medicalCD, medicalCooldown);
    updateCooldownFromAPI(travelCD, travelTime);
    updateCooldownFromAPI(hospitalCD, hospitalTs, serverTime, true);
    updateCooldownFromAPI(jailCD, jailTs, serverTime, true);
  }

  // -------- Update cooldowns every second --------
  updateCooldown(boosterCD);
  updateCooldown(drugCD);
  updateCooldown(medicalCD);
  updateCooldown(travelCD);
  updateCooldown(hospitalCD);
  updateCooldown(jailCD);

  // -------- Update chain countdown every second --------
  unsigned long nowMillis = millis();
  if (chainTimeoutTick > 0 && nowMillis - lastChainUpdate >= 1000) {
    chainTimeoutTick--;
    lastChainUpdate = nowMillis;

    int startY = 155;
    int spacing = 25;
    int barHeight = 10;
    int chainBarY = startY + spacing*4 - 12;
    int textY = chainBarY + (barHeight - 8)/2;
    int labelX = 10;
    int valueX = screenWidth - 10 - (String(0) + "/" + String(10)).length()*6;
    int timerX = labelX + (valueX - labelX)/2;

    int minutes = chainTimeoutTick / 60;
    int seconds = chainTimeoutTick % 60;
    char timerBuf[8];
    sprintf(timerBuf, "%02d:%02d", minutes, seconds);

    tft.fillRect(timerX, textY, 36, 8, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(timerX, textY);
    tft.print(timerBuf);
  }

  // -------- Update Organized Crime countdown every second --------
  if (ocReadyAt > 0 && millis() - lastOCDraw >= 1000) {
    lastOCDraw = millis();

    long currentServerTime = ocServerBaseTime +
        (millis() - ocMillisBase) / 1000;

    long remaining = ocReadyAt - currentServerTime;
    if (remaining < 0) remaining = 0;

    long days    = remaining / 86400;
    long hours   = (remaining % 86400) / 3600;
    long minutes = (remaining % 3600) / 60;
    long seconds = remaining % 60;

    char timeBuf[20];
    sprintf(timeBuf, "%02ld:%02ld:%02ld:%02ld", days, hours, minutes, seconds);

    // Clear area under money
    tft.fillRect(15, 75, 180, 12, TFT_BLACK);

    tft.setTextSize(1);
    tft.setTextColor(remaining == 0 ? TFT_GREEN : TFT_WHITE);
    tft.setCursor(15, 75);
    tft.print("OC Ready in: ");
    tft.print(timeBuf);
  }
  
  // -------- Update Torn clock every second (smooth) --------
  static unsigned long lastClockDraw = 0;
  if (lastServerTime > 0 && millis() - lastClockDraw >= 1000) {
      lastClockDraw = millis();

      long currentServerTime = lastServerTime +
          (millis() - lastApiUpdateMillis) / 1000;

      time_t rawTime = currentServerTime;
      struct tm * timeinfo = gmtime(&rawTime);

      char timeBuffer[16];
      sprintf(timeBuffer, "%02d:%02d:%02d",
              timeinfo->tm_hour,
              timeinfo->tm_min,
              timeinfo->tm_sec);

      // Right-align the timer
      int timerWidth = tft.textWidth(timeBuffer);
      int padding = 10;
      int timerX = screenWidth - timerWidth - padding;

      // Clear previous timer
      tft.fillRect(timerX, 30, timerWidth, 10, TFT_BLACK);

      tft.setTextSize(1);
      tft.setTextColor(TFT_LIGHTGREY);
      tft.setCursor(timerX, 30);
      tft.print(timeBuffer);
  }

  // -------- API countdown top-right --------
  int apiCountdown = 60 - ((millis() - lastApiUpdate) / 1000);
  if (apiCountdown < 0) apiCountdown = 0;
  char buf[8];
  sprintf(buf, "%2ds", apiCountdown);

  // Clear previous text
  tft.fillRect(screenWidth - tft.textWidth(buf) - 10, 45, tft.textWidth(buf), 12, TFT_BLACK);

  // Draw new countdown
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setCursor(screenWidth - tft.textWidth(buf) - 10, 45);
  tft.print(buf);

  delay(100);
}