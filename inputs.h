#define CS 9,11 // P9_11 - Chip Select Low active
#define RST 9,12 // P9_12 - Reset when L initialisation executed
#define A0 9,13 // P9_13 - Register Select
#define SCLK 9,15 // P9_15 - Serial Clock
#define SID 9,23 // P9_23 - Serial Input
#define scrollButton 8,11 // P8_11 - Menu Scroll Button
#define selectButton 8,12 // P8_12 - Menu Select Button
#define button1 8,13 // P8_13 - Multi Use Button 1
#define button2 8,14 // P8_14 - Multi Use Button 2

void inputInitialise();
void *inputCheck(void *data);
void inputCleanup();
