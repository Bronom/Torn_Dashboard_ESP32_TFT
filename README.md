# Torn_Dashboard_ESP32_TFT
 Torn Dashboard - ESP32 WROOM DA + TFT 2.8 Inch LCD Touch with ILI9341 Driver

*** Wifi must be 2.4Ghz ***

# Part List : 
2.8 Inch SPI TFT LCD Touch ILI9341

ESP32-32D

ESP32-32D Breaker Board (Recommended)

Electrical Wire

# Pinout : 
| Function    | ESP32 Pin | Notes                                             |
| ----------- | --------- | ------------------------------------------------- |
| **MOSI**    | 23        | SPI data to TFT                                   |
| **MISO**    | 19        | SPI data from TFT (used only if reading from TFT) |
| **SCLK**    | 18        | SPI clock                                         |
| **CS**      | 5         | TFT chip select                                   |
| **DC / A0** | 2         | Data/Command select                               |
| **RST**     | 4         | Reset                                             |
| **BL**      | 32        | Backlight control (can use PWM if needed)         |

| Function      | ESP32 Pin | Notes                                |
| ------------- | --------- | ------------------------------------ |
| **CS**        | 21        | Chip select for touch controller     |
| **MOSI**      | 23        | Same SPI MOSI (shared)               |
| **MISO**      | 19        | Same SPI MISO (shared)               |
| **SCLK**      | 18        | Same SPI clock (shared)              |
| **IRQ / PEN** | 22        | Some libraries support touch IRQ pin |


# Setup : 
To Setup go to this link : https://torndb.bronom.com/

# Library : 
TFT_eSPI by Bodmer

XPT2046_Touchscreen by Paul Stoffregen

Wifi by Arduino

HttpClient by Adrian McEwen

ArduinoJson by Benoit Blanchon

# Board Manager :
esp32 by Espressif Systems

# secrets.h : 
Credential for Wifi and API KEY
