# Torn_Dashboard_ESP32_TFT
 Torn Dashboard - ESP32 WROOM DA + TFT 2.8 Inch LCD Touch with ILI9341 Driver


Pinout : 

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
