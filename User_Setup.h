#define ILI9341_DRIVER

#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
#define TFT_BL 	 32   // Backlight pin

#define TOUCH_CS 21 // optional

#define SPI_TOUCH_FREQUENCY 2500000
#define SPI_FREQUENCY       25000000

#define LOAD_GLCD    // 16 pixel font
#define LOAD_FONT2   // 24 pixel font
#define LOAD_FONT4   // 48 pixel font
#define LOAD_FONT6   // 48 pixel font
#define LOAD_FONT7   // 7-segment 48 pixel
#define LOAD_FONT8   // Large 75 pixel font
#define LOAD_GFXFF   // FreeFonts