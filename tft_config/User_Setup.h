// Copy over ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
// (library updates overwrite that file, so this is the source of truth)

#define ILI9488_DRIVER        // confirmed working (ST7796 was a wiring red herring)

#define TFT_MISO 19           // used by touch only; leave the display's SDO unconnected
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TOUCH_CS 21

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  16000000
#define SPI_TOUCH_FREQUENCY  2500000
