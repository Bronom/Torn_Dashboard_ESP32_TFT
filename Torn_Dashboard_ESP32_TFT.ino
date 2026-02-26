#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>

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

int APIRefreshSecond = 60;

TFT_eSPI tft = TFT_eSPI(); // TFT instance
TFT_eSprite sprite = TFT_eSprite(&tft); // create sprite linked to tft

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
int leftX   = 20;
int centerX = screenWidth / 2 - 30;
int rightX  = screenWidth - 70;

Cooldown boosterCD = {0, 0, "Booster", leftX, cooldownY, 0};
Cooldown drugCD    = {0, 0, "Drug", centerX, cooldownY, 0};
Cooldown medicalCD = {0, 0, "Medical", rightX, cooldownY, 0};

// Extra timers: Travel, Hospital, Jail
int extraY = cooldownY + 25;
int extraLeftX   = 20;
int extraCenterX = screenWidth / 2 - 30;
int extraRightX  = screenWidth - 70;

Cooldown travelCD   = {0, 0, "Travel", extraLeftX, extraY, 0};
Cooldown hospitalCD = {0, 0, "Hospital", extraCenterX, extraY, 0};
Cooldown jailCD     = {0, 0, "Jail", extraRightX, extraY, 0};

long travelServerBaseTime = 0;
unsigned long travelMillisBase = 0;
unsigned long lastTravelDraw = 0;

// ------------------- Chain timer -------------------
int chainTimeoutTick = 0;
unsigned long lastChainUpdate = 0;
unsigned long chainServerBaseTime = 0;
unsigned long chainMillisBase = 0;

int chainCurrent = 0;
int chainMax = 0;
int chainTimeout = 0;

// ------------------- Organized Crime timer -------------------
long ocReadyAt = 0;
long ocServerBaseTime = 0;
unsigned long ocMillisBase = 0;
unsigned long lastOCDraw = 0;       // for 1 sec refresh

// ------------------- Next Ranked War -------------------
long rwStartAt = 0;
long rwEndAt = 0;
String opponentName = "";
String rwResult = "";
long rwServerBaseTime = 0;
unsigned long rwMillisBase = 0;
unsigned long lastRWDraw = 0;

// ------------------- Torn clock -------------------
unsigned long lastClockDraw = 0;
long serverTime = 0;

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

// ------------------- Draw Status -------------------
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

        int wordWidth = sprite.textWidth(word);

        if (cursorX + wordWidth > padding + maxWidth) {
            cursorX = padding;
            cursorY += 8 * textSize + 2;
        }

        sprite.setCursor(cursorX, cursorY);
        sprite.print(word);
        cursorX += wordWidth + spaceWidth;
        start = end + 1;
    }
}

// Draw bar (Energy, Nerve, Happy, Life, Chain)
void drawBar(int x, int y, int width, int height, float percent, uint16_t fillColor, uint16_t bgColor, int currentValue, int maxValue, const char* label) {
    int spacing = 12;
    sprite.fillRect(x, y, width, height, bgColor);
    if (percent > 1) percent = 1;
    int fillWidth = (int)(width * percent);
    sprite.fillRect(x, y, fillWidth, height, fillColor);
    sprite.drawRect(x, y, width, height, TFT_WHITE);

    sprite.setTextSize(1);
    sprite.setTextColor(TFT_WHITE);

    char buf[16];
    sprintf(buf, "%d/%d", currentValue, maxValue);

    int textWidth = strlen(buf) * 6;
    int textHeight = 8;
    int textY = y + (height - textHeight)/2;

    sprite.setCursor(10, textY - spacing);
    sprite.print(label);

    sprite.setCursor((screenWidth - textWidth) - 10, textY - spacing);
    sprite.print(buf);
}

// ------------------- Cooldown Functions -------------------
void updateCooldown(Cooldown &cd, bool hideWhenZero = false) {
    unsigned long now = millis();
    if (now - cd.lastUpdate >= 1000) {
        if (cd.ticktime > 0) cd.ticktime--;
        cd.lastUpdate = now;

        // Clear the previous timer in the sprite
        sprite.fillRect(cd.x, cd.y, 80, 20, TFT_BLACK); 

        if (cd.ticktime > 0 || !hideWhenZero) {
            int hours = cd.ticktime / 3600;
            int minutes = (cd.ticktime % 3600) / 60;
            int seconds = cd.ticktime % 60;

            char buf[16];
            if (cd.ticktime == 0 && !hideWhenZero) strcpy(buf, "READY");
            else sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);

            sprite.setTextSize(1);
            sprite.setTextColor((cd.ticktime == 0 && !hideWhenZero) ? TFT_GREEN : TFT_WHITE);

            // Draw label
            sprite.setCursor(cd.x, cd.y);
            sprite.print(cd.label);

            // Draw timer under label
            sprite.setCursor(cd.x, cd.y + 10); // 10px below label
            sprite.print(buf);
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

  sprite.setColorDepth(8);
  sprite.createSprite(screenWidth, screenHeight); // full-screen buffer
  sprite.fillScreen(TFT_BLACK); // clear buffer

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
    
    // ---------------- Global layout variables ----------------
    int startY = 155;   // starting Y position for bars
    int spacing = 25;   // vertical spacing between bars
    int barHeight = 10; // height of bars

    unsigned long now = millis();
    static unsigned long lastApiUpdate = 0;
    static unsigned long lastApiUpdateMillis = 0;

    int energyCurrent = 0, energyMax = 1;
    int nerveCurrent = 0, nerveMax = 1;
    int happyCurrent = 0, happyMax = 1;
    int lifeCurrent = 0, lifeMax = 1;
    int boosterCooldown = 0, drugCooldown = 0, medicalCooldown = 0;
    int travelTime = 0;
    long hospitalTs = 0, jailTs = 0;
    String name = "Unknown";
    int playerId = 0, level = 0;
    String statusDesc = "Idle", statusCol = "white";
    int notificationsCount = 0;
    long moneyOnHand = 0;


    // -------- API fetch every 60s --------
    if (now - lastApiUpdate >= (APIRefreshSecond * 1000) || lastApiUpdate == 0) {
        lastApiUpdate = now;

        WiFiClientSecure client;
        client.setInsecure();  // skip cert validation

        // --- Get server time ---
        HTTPClient httpTime;
        httpTime.begin(client, "https://api.torn.com/v2/user/timestamp");
        httpTime.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpTime.addHeader("accept", "application/json");

        int code = httpTime.GET();

        if (code > 0) {
            String payload = httpTime.getString();
            DynamicJsonDocument doc(1024);
            if (!deserializeJson(doc, payload)) {
                serverTime = doc["timestamp"] | 0;

                lastApiUpdateMillis = millis();
            }
        }
        httpTime.end();

        // ------------------- PLAYER BASIC -------------------
        HTTPClient httpPlayer;
        httpPlayer.begin(client, "https://api.torn.com/v2/user/basic");
        httpPlayer.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpPlayer.addHeader("accept", "application/json");

        code = httpPlayer.GET();
        if (code > 0) {
            String payload = httpPlayer.getString();
            //Serial.println(payload); // debug

            DynamicJsonDocument doc(40960);
            DeserializationError err = deserializeJson(doc, payload);
            if (!err && doc.containsKey("profile")) {
                JsonObject profile = doc["profile"];

                name = profile["name"] | "Unknown";
                playerId = profile["id"] | 0;
                level = profile["level"] | 0;

                if (profile.containsKey("status")) {
                    JsonObject status = profile["status"];
                    statusDesc = status["description"] | "Idle";
                    statusCol  = status["color"] | "white";
                }
            }
        }
        httpPlayer.end();


        // ------------------- BARS -------------------
        HTTPClient httpBars;
        httpBars.begin(client, "https://api.torn.com/v2/user/bars");
        httpBars.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpBars.addHeader("accept", "application/json");

        code = httpBars.GET();
        if (code > 0) {
            String payload = httpBars.getString();
            DynamicJsonDocument doc(8192);
            if (!deserializeJson(doc, payload)) {
                JsonObject bars = doc["bars"];
                energyCurrent = bars["energy"]["current"] | 0;
                energyMax     = bars["energy"]["maximum"] | 1;

                nerveCurrent = bars["nerve"]["current"] | 0;
                nerveMax     = bars["nerve"]["maximum"] | 1;

                happyCurrent = bars["happy"]["current"] | 0;
                happyMax     = bars["happy"]["maximum"] | 1;

                lifeCurrent = bars["life"]["current"] | 0;
                lifeMax     = bars["life"]["maximum"] | 1;
            }
        }
        httpBars.end();


        // ------------------- MONEY -------------------
        HTTPClient httpMoney;
        httpMoney.begin(client, "https://api.torn.com/v2/user/money");
        httpMoney.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpMoney.addHeader("accept", "application/json");

        code = httpMoney.GET();
        if (code > 0) {
            String payload = httpMoney.getString();
            DynamicJsonDocument doc(2048);
            if (!deserializeJson(doc, payload)) {
                moneyOnHand = doc["money"]["wallet"] | 0;
            }
        }
        httpMoney.end();


        // ------------------- NOTIFICATIONS -------------------
        HTTPClient httpNotif;
        httpNotif.begin(client, "https://api.torn.com/v2/user/notifications");
        httpNotif.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpNotif.addHeader("accept", "application/json");

        code = httpNotif.GET();
        if (code > 0) {
            String payload = httpNotif.getString();
            DynamicJsonDocument doc(4096);
            if (!deserializeJson(doc, payload)) {
                notificationsCount = doc["notifications"]["count"] | 0;
            }
        }
        httpNotif.end();


        // ------------------- COOLDOWNS -------------------
        HTTPClient httpCD;
        httpCD.begin(client, "https://api.torn.com/v2/user/cooldowns");
        httpCD.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpCD.addHeader("accept", "application/json");

        int codeCD = httpCD.GET();
        if (codeCD > 0) {
            String payloadCD = httpCD.getString();
            DynamicJsonDocument cdDoc(8192);
            if (!deserializeJson(cdDoc, payloadCD)) {
                JsonObject cd = cdDoc["cooldowns"];

                boosterCooldown = cd["booster"] | 0;
                drugCooldown    = cd["drug"] | 0;
                medicalCooldown = cd["medical"] | 0;
            }
        }
        httpCD.end();


        // ------------------- TRAVEL -------------------
        HTTPClient travelHttp;
        travelHttp.begin(client, "https://api.torn.com/v2/user/travel");
        travelHttp.addHeader("Authorization", "ApiKey " + String(apiKey));
        travelHttp.addHeader("accept", "application/json");

        int travelCode = travelHttp.GET();
        if (travelCode > 0) {
            String travelPayload = travelHttp.getString();
            DynamicJsonDocument travelDoc(4096);

            if (!deserializeJson(travelDoc, travelPayload)) {
                JsonObject travel = travelDoc["travel"];

                travelTime = travel["time_left"] | 0;
            }
        }
        travelHttp.end();

        // ------------------- Profile / Status ----------------------
        HTTPClient statusHttp;
        statusHttp.begin(client, "https://api.torn.com/v2/user/profile");
        statusHttp.addHeader("Authorization", "ApiKey " + String(apiKey));
        statusHttp.addHeader("accept", "application/json");

        int statusCode = statusHttp.GET();
        if (statusCode > 0) {
            String payload = statusHttp.getString();

            DynamicJsonDocument doc(16384);
            DeserializationError err = deserializeJson(doc, payload);
            if (!err) {
                JsonObject profile = doc["profile"];
                JsonObject status  = profile["status"];

                String state       = status["state"] | "";
                long untilTs       = status["until"] | 0;

                // Update your cooldowns
                if (state == "Hospital") hospitalTs = untilTs;
                else if (state == "Jail") jailTs = untilTs;
            }
        }
        statusHttp.end();

        // ------------------- CHAIN -------------------
        HTTPClient httpChain;
        String url = "https://api.torn.com/v2/faction/chain";
        httpChain.begin(client, url);
        httpChain.addHeader("Authorization", "ApiKey " + String(apiKey));
        httpChain.addHeader("accept", "application/json");

        code = httpChain.GET();
        chainCurrent = 0;
        chainMax = 0;
        chainTimeout = 0;

        if (code > 0) {
            String payload = httpChain.getString();
            //Serial.println(payload); // debug
            DynamicJsonDocument doc(2048);
            if (!deserializeJson(doc, payload)) {
                if (doc.containsKey("chain")) {
                    chainCurrent = doc["chain"]["current"] | 0;
                    chainMax     = doc["chain"]["max"] | 0;
                    chainTimeout = doc["chain"]["timeout"] | 0;

                    chainTimeoutTick = chainTimeout;
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
            // Serial.println(payload); // debug
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
        int notifY = 42;
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
        sprite.setCursor(15, 54);
        sprite.print("Money: ");
        int labelWidth = sprite.textWidth("Money: ");
        sprite.setTextColor(TFT_GREEN);
        sprite.setCursor(10 + labelWidth, 54);
        sprite.print("$" + formatMoney(moneyOnHand));

        // Status
        drawStatus(statusDesc, 92, statusColor(statusCol), 2, 15);

        // Bars
        int barWidth = screenWidth - 20;
        int barHeight = 10;
        int spacing = 25;
        int startY = 155;
        drawBar(10, startY, barWidth, barHeight, (float)energyCurrent/energyMax, TFT_GREEN, TFT_DARKGREY, energyCurrent, energyMax, "Energy");
        drawBar(10, startY + spacing, barWidth, barHeight, (float)nerveCurrent/nerveMax, TFT_RED, TFT_DARKGREY, nerveCurrent, nerveMax, "Nerve");
        drawBar(10, startY + spacing*2, barWidth, barHeight, (float)happyCurrent/happyMax, TFT_YELLOW, TFT_DARKGREY, happyCurrent, happyMax, "Happy");
        drawBar(10, startY + spacing*3, barWidth, barHeight, (float)lifeCurrent/lifeMax, TFT_BLUE, TFT_DARKGREY, lifeCurrent, lifeMax, "Life");
        drawBar(10, startY + spacing*4, barWidth, barHeight, 0, TFT_LIGHTGREY, TFT_DARKGREY, chainCurrent, chainMax, "Chain");
    }

    // -------- Update cooldowns every second (with flashing) --------
    updateCooldown(boosterCD);
    updateCooldown(drugCD);
    updateCooldown(medicalCD);
    updateCooldown(travelCD);
    updateCooldown(hospitalCD);
    updateCooldown(jailCD);

    // -------- Update chain countdown --------
    if (chainTimeoutTick > 0 && millis() - lastChainUpdate >= 1000) {
        chainTimeoutTick--;
        lastChainUpdate = millis();

        int startY = 155 + spacing*4 - 12;
        int textY = startY + (10 - 8)/2;
        int timerX = 10 + (screenWidth - 20)/2 - 18;

        int minutes = chainTimeoutTick / 60;
        int seconds = chainTimeoutTick % 60;
        char timerBuf[8];
        sprintf(timerBuf, "%02d:%02d", minutes, seconds);

        sprite.fillRect(timerX, textY, 36, 8, TFT_BLACK);
        sprite.setTextSize(1);
        sprite.setTextColor(TFT_WHITE);
        sprite.setCursor(timerX, textY);
        sprite.print(timerBuf);
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
      sprite.fillRect(15, 66, 180, 12, TFT_BLACK);

      sprite.setTextSize(1);
      sprite.setTextColor(remaining == 0 ? TFT_GREEN : TFT_WHITE);
      sprite.setCursor(15, 66);
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
        sprite.fillRect(15, 78, 120, 12, TFT_BLACK);

        sprite.setTextSize(1);
        sprite.setTextColor(remaining == 0 ? TFT_GREEN : TFT_WHITE);
        sprite.setCursor(15, 78);
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
        sprite.setTextColor(TFT_LIGHTGREY);
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
    sprite.setTextColor(TFT_LIGHTGREY);
    sprite.setCursor(screenWidth - sprite.textWidth(buf) - 10, 45);
    sprite.print(buf);

    // -------- Push everything to TFT --------
    sprite.pushSprite(0, 0);

    delay(100);
}