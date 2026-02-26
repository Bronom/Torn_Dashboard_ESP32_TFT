#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>
#include "secrets.h"

// ------------------- TFT Setup -------------------
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

bool screenOn = true;
unsigned long lastTouchTime = 0;
int screenWidth = tft.width();
int screenHeight = tft.height();

// ------------------- Cooldown Struct -------------------
struct Cooldown {
  int ticktime;          
  int apiValue;          
  const char* label;
  int x, y;
  unsigned long lastUpdate;
};

// ------------------- Cooldowns -------------------
int cooldownY = 270;
int leftX   = 20;
int centerX = screenWidth / 2 - 21;
int rightX  = screenWidth - 60;

Cooldown boosterCD = {0, 0, "Booster", leftX, cooldownY, 0};
Cooldown drugCD    = {0, 0, "Drug", centerX, cooldownY, 0};
Cooldown medicalCD = {0, 0, "Medical", rightX, cooldownY, 0};

int extraY = cooldownY + 25;
Cooldown travelCD   = {0, 0, "Travel", leftX, extraY, 0};
Cooldown hospitalCD = {0, 0, "Hospital", centerX, extraY, 0};
Cooldown jailCD     = {0, 0, "Jail", rightX, extraY, 0};

// ------------------- Chain -------------------
int chainCurrent = 0;
int chainMax = 0;
int chainTimeout = 0;
int chainTimeoutTick = 0;
unsigned long lastChainUpdate = 0;

int chainCooldown = 0;          // cooldown from API
int chainCooldownTick = 0;      // local ticking value
unsigned long lastChainCooldownUpdate = 0;

// ------------------- Organized Crime -------------------
long ocReadyAt = 0;
unsigned long ocMillisBase = 0;
long ocServerBaseTime = 0;
unsigned long lastOCDraw = 0;

// ------------------- Ranked War -------------------
long rwStartAt = 0;
long rwEndAt = 0;
String opponentName = "";
String rwResult = "";
unsigned long rwMillisBase = 0;
long rwServerBaseTime = 0;
unsigned long lastRWDraw = 0;

// ------------------- Torn Clock -------------------
long serverTime = 0;
unsigned long lastClockDraw = 0;

// ------------------- Settings -------------------
int APIRefreshSecond = 60;

// ------------------- Global Variables -------------------
unsigned long lastApiUpdate = 0;
unsigned long lastApiUpdateMillis = 0;

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
    sprite.fillRect(x, y, width, height, bgColor);
    sprite.fillRect(x, y, (int)(width * percent), height, fillColor);
    sprite.drawRect(x, y, width, height, TFT_WHITE);

    sprite.setTextSize(1);
    sprite.setTextColor(TFT_WHITE);

    char buf[16];
    sprintf(buf, "%d/%d", currentValue, maxValue);

    int textWidth = strlen(buf) * 6;
    int textHeight = 8;
    int textY = y + (height - textHeight)/2;

    sprite.setCursor(10, textY - 12);
    sprite.print(label);

    sprite.setCursor((screenWidth - textWidth) - 10, textY - 12);
    sprite.print(buf);
}

// ------------------- Cooldown Functions -------------------
void updateCooldown(Cooldown &cd, bool hideWhenZero = false) {
    unsigned long now = millis();
    if (now - cd.lastUpdate >= 1000) {
        if (cd.ticktime > 0) cd.ticktime--;
        cd.lastUpdate = now;

        sprite.fillRect(cd.x, cd.y, 80, 20, TFT_BLACK);

        if (cd.ticktime > 0 || !hideWhenZero) {
            int hours = cd.ticktime / 3600;
            int minutes = (cd.ticktime % 3600) / 60;
            int seconds = cd.ticktime % 60;

            char buf[16];
            if (cd.ticktime == 0 && !hideWhenZero) strcpy(buf, "READY");
            else sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);

            sprite.setTextSize(1);

            // ALWAYS WHITE for the label
            sprite.setTextColor(TFT_WHITE);
            sprite.setCursor(cd.x, cd.y);
            sprite.print(cd.label);

            // Green only for the timer/value if you want
            sprite.setTextColor(cd.ticktime == 0 && !hideWhenZero ? TFT_GREEN : TFT_WHITE);
            sprite.setCursor(cd.x, cd.y + 10);
            sprite.print(buf);
        }
    }
}

void updateCooldownFromAPI(Cooldown &cd, int newValue, long serverTime = 0, bool isAbsolute = false) {
  cd.ticktime = isAbsolute ? max(0L, newValue - serverTime) : max(0, newValue);
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
    tft.setCursor(15, 40);
    tft.setTextSize(2);
    tft.print("Trying: ");
    tft.println(wifiList[i].ssid);

    WiFi.begin(wifiList[i].ssid, wifiList[i].password);
    unsigned long startTime = millis();
    while (millis() - startTime < 10000) {
      if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(15, 10);
        tft.println("WiFi Connected!");
        tft.setCursor(15, 35);
        tft.printf("SSID: %s\n", wifiList[i].ssid);
        tft.setCursor(15, 60);
        tft.printf("IP: %s\n", WiFi.localIP().toString().c_str());
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
  tft.println("No WiFi Found!");
}

// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  sprite.setColorDepth(8);
  sprite.createSprite(screenWidth, screenHeight);
  sprite.fillScreen(TFT_BLACK);

  connectWiFi();
}

// ------------------- Main Loop -------------------
void loop() {
  // Touch screen toggle
  uint16_t touchX, touchY;
  if (tft.getTouch(&touchX, &touchY) && millis() - lastTouchTime > 300) {
      screenOn = !screenOn;
      digitalWrite(TFT_BL, screenOn ? HIGH : LOW);
      lastTouchTime = millis();
  }

  if (WiFi.status() != WL_CONNECTED) { WiFi.reconnect(); delay(1000); return; }

  static unsigned long lastApiUpdate = 0;
  unsigned long now = millis();

    // -------- API fetch --------
    if (now - lastApiUpdate >= (APIRefreshSecond * 1000) || lastApiUpdate == 0) {
        lastApiUpdate = now;

        // ------------------- PLAYER BASIC -------------------
        // -------- Player API --------
        HTTPClient http;
        http.begin("https://api.torn.com/user/?selections=basic,bars,travel,cooldowns,notifications,money,profile&key=" + String(apiKey));
        int httpCode = http.GET();

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

            lastApiUpdateMillis = millis();     // ESP millis when we got the server time
        }
        }
        http.end();


        WiFiClientSecure client;
        client.setInsecure();  // skip cert validation

        // ------------------- CHAIN -------------------
        HTTPClient httpChain;
        String url = "https://api.torn.com/v2/faction/chain";
        httpChain.begin(client, url);
        httpChain.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpChain.addHeader("accept", "application/json");

        int code = httpChain.GET();
        chainCurrent = 0;
        chainMax = 0;
        chainTimeout = 0;

        if (code > 0) {
            String payload = httpChain.getString();

            DynamicJsonDocument doc(2048);
            if (!deserializeJson(doc, payload)) {
                if (doc.containsKey("chain")) {
                    chainCurrent = doc["chain"]["current"] | 0;
                    chainMax     = doc["chain"]["max"] | 0;
                    chainTimeout = doc["chain"]["timeout"] | 0;

                    chainCooldown = doc["chain"]["cooldown"] | 0;

                    // Convert absolute timestamp to remaining seconds
                    if (chainCooldown > 0 && serverTime > 0) {
                        chainCooldownTick = max(0L, chainCooldown - serverTime);
                    } else {
                        chainCooldownTick = 0;
                    }
                    lastChainCooldownUpdate = millis();

                    chainTimeoutTick = chainTimeout;
                    lastChainUpdate = millis();
                }
            }
        }
        httpChain.end();

        // ------------------- ORGANIZED CRIME -------------------
        HTTPClient httpOC;
        httpOC.begin(client, "https://api.torn.com/v2/user/organizedcrime");
        httpOC.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpOC.addHeader("accept", "application/json");

        int ocCode = httpOC.GET();
        if (ocCode > 0) {
            String payload = httpOC.getString();
            DynamicJsonDocument doc(2048);

            if (!deserializeJson(doc, payload)) {
                ocReadyAt = doc["organizedCrime"]["ready_at"] | 0; // Unix timestamp

                // Compute remaining seconds
                ocServerBaseTime = serverTime;
                ocMillisBase = millis();
            }
        }
        httpOC.end();

        // ------------------- Ranked War -------------------
        HTTPClient httpRW;
        httpRW.begin(client, "https://api.torn.com/v2/faction/rankedwars?offset=0&limit=1&sort=DESC");
        httpRW.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpRW.addHeader("accept", "application/json");

        code = httpRW.GET();

        if (code > 0) {
            String payload = httpRW.getString();

            DynamicJsonDocument doc(4096);
            if (!deserializeJson(doc, payload)) {
                if (doc.containsKey("rankedwars") && doc["rankedwars"].size() > 0) {
                    JsonObject war = doc["rankedwars"][0];
                    rwStartAt = war["start"] | 0;
                    rwEndAt = war["end"] | 0;

                    rwResult = String((const char*)war["result"]);
                    if (war.containsKey("opponent")) {
                        opponentName = String((const char*)war["opponent"]["name"]);
                    }

                    rwServerBaseTime = serverTime;
                    rwMillisBase = millis();
                }
            }
        }
        httpRW.end();

        // -------- Update cooldowns from API --------
        updateCooldownFromAPI(boosterCD, boosterCooldown);
        updateCooldownFromAPI(drugCD, drugCooldown);
        updateCooldownFromAPI(medicalCD, medicalCooldown);
        updateCooldownFromAPI(travelCD, travelTime);
        updateCooldownFromAPI(hospitalCD, hospitalTs, serverTime, true);
        updateCooldownFromAPI(jailCD, jailTs, serverTime, true);

        // -------- Clear sprite and redraw everything --------
        sprite.fillScreen(TFT_BLACK);

        // Player info
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

        // Notifications
        int notifY = 43;
        sprite.setCursor(15, notifY);
        sprite.setTextSize(1);
        if (notificationsCount > 0) {
            int badgeRadius = 6;
            int textW = sprite.textWidth(String(notificationsCount));
            int badgeX = 15 + sprite.textWidth("Notifications: ") + textW/2;
            int badgeY = notifY + 4;
            sprite.fillCircle(badgeX, badgeY, badgeRadius, TFT_RED);
            sprite.setTextColor(TFT_WHITE);
            sprite.setCursor(badgeX - textW/2, badgeY - 3);
            sprite.print(notificationsCount);
            sprite.setTextColor(TFT_WHITE);
            sprite.setCursor(15, notifY);
            sprite.print("Notifications:");
        } else {
            sprite.setTextColor(TFT_WHITE);
            sprite.setCursor(15, notifY);
            sprite.printf("Notifications: %d", notificationsCount);
        }

        // Money
        sprite.setTextColor(TFT_WHITE);
        sprite.setCursor(15, 55);
        sprite.print("Money: ");
        int labelWidth = sprite.textWidth("Money: ");
        sprite.setTextColor(TFT_GREEN);
        sprite.setCursor(10 + labelWidth, 55);
        sprite.print("$" + formatMoney(moneyOnHand));

        // Status
        drawStatus(statusDesc, 91, statusColor(statusCol), 2, 15);

        // Bars
        int barWidth = screenWidth - 20;
        int barHeight = 10;
        int spacing = 25;

        // Set chain bar color based on cooldown
        uint16_t chainColor = (chainCooldownTick > 0) ? TFT_CYAN : TFT_LIGHTGREY;

        drawBar(10, barStartY, barWidth, barHeight, (float)energyCurrent/energyMax, TFT_GREEN, TFT_DARKGREY, energyCurrent, energyMax, "Energy");
        drawBar(10, barStartY + spacing, barWidth, barHeight, (float)nerveCurrent/nerveMax, TFT_RED, TFT_DARKGREY, nerveCurrent, nerveMax, "Nerve");
        drawBar(10, barStartY + spacing*2, barWidth, barHeight, (float)happyCurrent/happyMax, TFT_YELLOW, TFT_DARKGREY, happyCurrent, happyMax, "Happy");
        drawBar(10, barStartY + spacing*3, barWidth, barHeight, (float)lifeCurrent/lifeMax, TFT_BLUE, TFT_DARKGREY, lifeCurrent, lifeMax, "Life");
        drawBar(10, barStartY + spacing*4, barWidth, barHeight, (float)chainCurrent/chainMax, chainColor, TFT_DARKGREY, chainCurrent, chainMax, "Chain");
    }

    // Update cooldowns every second
    updateCooldown(boosterCD);
    updateCooldown(drugCD);
    updateCooldown(medicalCD);
    updateCooldown(travelCD);
    updateCooldown(hospitalCD);
    updateCooldown(jailCD);

    // -------- Smart Chain Countdown --------
    // -------- Update chain countdown every second --------
    if (millis() - lastChainUpdate >= 1000) {
        lastChainUpdate = millis();

        // Tick down cooldown first
        if (chainCooldownTick > 0) {
            chainCooldownTick--;
        } 
        // Then tick timeout if cooldown finished
        else if (chainTimeoutTick > 0) {
            chainTimeoutTick--;
        }

        int activeTimer = (chainCooldownTick > 0) ? chainCooldownTick : chainTimeoutTick;

        // Clear timer area
        int startY = 155;
        int spacing = 25;
        int barHeight = 10;
        int chainBarY = startY + spacing*4 - 12;
        int textY = chainBarY + (barHeight - 8)/2;
        int labelX = 10;
        int valueX = screenWidth - 10 - (String(0) + "/" + String(10)).length()*6;
        int timerX = labelX + (valueX - labelX)/2;

        sprite.fillRect(timerX, textY, 60, 10, TFT_BLACK);

        if (activeTimer > 0) {
            char timerBuf[12];

            if (chainCooldownTick > 0) {
                // Format HH:MM:SS for cooldown
                int hours   = activeTimer / 3600;
                int minutes = (activeTimer % 3600) / 60;
                int seconds = activeTimer % 60;
                sprite.setTextColor(TFT_CYAN);
                sprintf(timerBuf, "%02d:%02d:%02d", hours, minutes, seconds);
            } else {
                // Format MM:SS for timeout
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
      sprite.fillRect(15, 67, 180, 12, TFT_BLACK);

      sprite.setTextSize(1);
      sprite.setTextColor(remaining == 0 ? TFT_GREEN : TFT_WHITE);
      sprite.setCursor(15, 67);
      sprite.print("OC: ");
      sprite.print(timeBuf);
    }

    // -------- Update Ranked War countdown every second --------
    if (rwStartAt > 0 && millis() - lastRWDraw >= 1000) {
        lastRWDraw = millis();

        long currentServerTime = serverTime + (millis() - lastApiUpdate) / 1000;
        long remaining = max(0L, rwStartAt - currentServerTime);

        if (remaining < 0) remaining = 0;

        long days    = remaining / 86400;
        long hours   = (remaining % 86400) / 3600;
        long minutes = (remaining % 3600) / 60;
        long seconds = remaining % 60;

        char rwBuf[16];
        sprintf(rwBuf, "%02ld:%02ld:%02ld:%02ld", days, hours, minutes, seconds);

        // Clear an area under OC
        sprite.fillRect(15, 79, 120, 12, TFT_BLACK);

        sprite.setTextSize(1);
        sprite.setTextColor(remaining == 0 ? TFT_GREEN : TFT_WHITE);
        sprite.setCursor(15, 79);
        sprite.print("RW: ");
        sprite.print(rwBuf);
    }

    // -------- Torn clock --------
    if (serverTime > 0 && millis() - lastClockDraw >= 1000) {
        lastClockDraw = millis();

        long currentServerTime = serverTime + (millis() - lastApiUpdateMillis)/1000;

        time_t rawTime = currentServerTime;
        struct tm * timeinfo = gmtime(&rawTime);

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

    // -------- API countdown top-right --------
    int apiCountdown = APIRefreshSecond - ((millis() - lastApiUpdate)/1000);
    if (apiCountdown < 0) apiCountdown = 0;
    char buf[8];
    sprintf(buf, "%3ds", apiCountdown);

    sprite.fillRect(screenWidth - sprite.textWidth(buf) - 10, 45, sprite.textWidth(buf), 12, TFT_BLACK);
    sprite.setTextSize(1);
    sprite.setTextColor(TFT_WHITE);
    sprite.setCursor(screenWidth - sprite.textWidth(buf) - 10, 45);
    sprite.print(buf);

    // -------- Push everything to TFT --------
    sprite.pushSprite(0, 0);

    delay(100);
}