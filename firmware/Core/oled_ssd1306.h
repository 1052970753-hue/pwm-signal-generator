#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#include "system_config.h"

#define OLED_CMD  0x00
#define OLED_DATA 0x40

extern uint8_t OLED_Buffer[128 * 64 / 8];

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Refresh(void);
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawChar(uint8_t x, uint8_t y, char c);
void OLED_DrawString(uint8_t x, uint8_t y, const char *str);
void OLED_DrawHLine(uint8_t x1, uint8_t x2, uint8_t y);
void OLED_DrawVLine(uint8_t x, uint8_t y1, uint8_t y2);
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t filled);

#endif
