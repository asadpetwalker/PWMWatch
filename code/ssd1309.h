#ifndef SSD1309_H
#define SSD1309_H

#include <stdint.h>

/* I2C address - SA0 pin LOW = 0x3C, HIGH = 0x3D.
 * The CFAL12856A0 reference design ties SA0 low,
 * giving address 0x3C. Change to 0x3D if yours differs. */
#define SSD1309_I2C_ADDR    (0x3C << 1)  /* HAL uses 8-bit addr */

/* Display physical dimensions */
#define SSD1309_WIDTH       128
#define SSD1309_HEIGHT      56

/* SSD1309 I2C control bytes */
#define SSD1309_CTRL_CMD    0x00  /* Co=0, D/C#=0 -> command */
#define SSD1309_CTRL_DATA   0x40  /* Co=0, D/C#=1 -> data    */

void SSD1309_Init(void);
void SSD1309_Clear(void);
void SSD1309_Flush(void);
void SSD1309_DrawTime(uint8_t hours, uint8_t minutes, uint8_t seconds);

/* Low-level pixel write into the local framebuffer */
void SSD1309_SetPixel(int16_t x, int16_t y, uint8_t on);

#endif /* SSD1309_H */
