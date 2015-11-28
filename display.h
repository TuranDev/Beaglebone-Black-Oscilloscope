#include <stdint.h>

#define CS 9,11 // P9_11 - Chip Select Low active
#define RST 9,12 // P9_12 - Reset when L initialisation executed
#define A0 9,13 // P9_13 - Register Select
#define SCLK 9,15 // P9_15 - Serial Clock
#define SID 9,23 // P9_23 - Serial Input

extern int pagemap[8];
extern uint8_t displayBuffer[1024];
extern uint8_t menuBuffer[3][1024];
extern uint8_t oscilliscopeBuffer[1024];
extern uint8_t signalGenBuffer[1024];
extern uint8_t aboutBuffer[1024];

void displayInitialise();
void sendData(uint8_t data);
void sendCommand(uint8_t command);
void updateDisplay();
void clearDisplay();
void setPixel(uint8_t x, uint8_t y, uint8_t state);
void drawLine(float x1, float y1,float x2, float y2);
void drawCross(uint8_t x, uint8_t y,uint8_t state);
void drawNumber(uint8_t x, uint8_t y, int number, int digits);
void clearColoumn(uint8_t x);
