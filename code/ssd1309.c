/* ============================================================
 * SSD1309 OLED driver
 * 128 x 56 pixels, I2C, for CFAL12856A0-0151-B
 *
 * The SSD1309 has 8 pages of 8 rows each (= 64 rows total).
 * The CFAL12856A0 exposes only the top 56 rows; the bottom
 * 8 rows are behind the getter and not visible. We simply
 * never write to page 6 (rows 48-55 of controller space) or
 * page 7 (rows 56-63). Keeping HEIGHT=56 handles this.
 *
 * Framebuffer layout: [page][column]
 *   pages 0-6 = rows 0-55  (7 pages x 128 cols = 896 bytes)
 * ============================================================ */

#include "ssd1309.h"
#include "i2c.h"
#include <string.h>

/* Number of pages actually used for 56 visible rows */
#define SSD1309_PAGES   ((SSD1309_HEIGHT + 7) / 8)   /* = 7 */

/* Local framebuffer - one byte per column per page */
static uint8_t framebuf[SSD1309_PAGES][SSD1309_WIDTH];

/* ---- internal helpers ---------------------------------------- */

extern I2C_HandleTypeDef hi2c1;

static void send_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { SSD1309_CTRL_CMD, cmd };
    HAL_I2C_Master_Transmit(&hi2c1, SSD1309_I2C_ADDR,
                             buf, 2, HAL_MAX_DELAY);
}

static void send_cmd2(uint8_t cmd, uint8_t arg)
{
    uint8_t buf[3] = { SSD1309_CTRL_CMD, cmd, arg };
    HAL_I2C_Master_Transmit(&hi2c1, SSD1309_I2C_ADDR,
                             buf, 3, HAL_MAX_DELAY);
}

/* ---- public API --------------------------------------------- */

void SSD1309_Init(void)
{
    /* Reset sequence recommended by SSD1309 datasheet and the
     * Crystalfontz CFAL12856A0 application note.              */
    HAL_Delay(10);

    send_cmd(0xAE);         /* display off                      */

    send_cmd2(0xD5, 0x80);  /* clock divide / osc freq          */
    send_cmd2(0xA8, 0x3F);  /* multiplex ratio: 64 MUX          */
    send_cmd2(0xD3, 0x00);  /* display offset: 0                */
    send_cmd(0x40);         /* display start line = 0           */

    send_cmd2(0x8D, 0x14);  /* charge pump: enable              */

    send_cmd2(0x20, 0x00);  /* horizontal addressing mode       */

    send_cmd(0xA1);         /* segment remap: col 127 = SEG0    */
    send_cmd(0xC8);         /* COM scan direction: remapped     */

    send_cmd2(0xDA, 0x12);  /* COM pin config: alt, no remap    */
    send_cmd2(0x81, 0xCF);  /* contrast                         */
    send_cmd2(0xD9, 0xF1);  /* pre-charge period                */
    send_cmd2(0xDB, 0x40);  /* VCOMH deselect level             */

    send_cmd(0xA4);         /* entire display on: follow RAM    */
    send_cmd(0xA6);         /* normal display (not inverted)    */
    send_cmd(0xAF);         /* display on                       */

    SSD1309_Clear();
    SSD1309_Flush();
}

void SSD1309_Clear(void)
{
    memset(framebuf, 0, sizeof(framebuf));
}

void SSD1309_Flush(void)
{
    /* Set column and page address window to full display */
    send_cmd(0x21);   /* set column address... */
    send_cmd(0);      /* start = 0             */
    send_cmd(SSD1309_WIDTH - 1); /* end = 127  */

    send_cmd(0x22);   /* set page address...   */
    send_cmd(0);      /* start page = 0        */
    send_cmd(SSD1309_PAGES - 1); /* end page   */

    /* Stream all framebuffer data in one I2C transaction.
     * Max HAL transmit buffer is typically fine for 896+1 bytes
     * but we send page by page to stay safe on all HAL configs. */
    for (uint8_t page = 0; page < SSD1309_PAGES; page++)
    {
        /* Prepend the control byte for GDDRAM data */
        uint8_t txbuf[SSD1309_WIDTH + 1];
        txbuf[0] = SSD1309_CTRL_DATA;
        memcpy(&txbuf[1], framebuf[page], SSD1309_WIDTH);
        HAL_I2C_Master_Transmit(&hi2c1, SSD1309_I2C_ADDR,
                                 txbuf, sizeof(txbuf),
                                 HAL_MAX_DELAY);
    }
}

void SSD1309_SetPixel(int16_t x, int16_t y, uint8_t on)
{
    if (x < 0 || x >= SSD1309_WIDTH ||
        y < 0 || y >= SSD1309_HEIGHT) return;

    uint8_t page = (uint8_t)(y / 8);
    uint8_t bit  = (uint8_t)(y % 8);

    if (on)
        framebuf[page][x] |=  (1 << bit);
    else
        framebuf[page][x] &= ~(1 << bit);
}

/* ---- time rendering ----------------------------------------- */

/* Forward declaration of font helpers defined in font.c */
extern const uint8_t font5x7[][5];
void SSD1309_DrawChar(int16_t x, int16_t y,
                      char c, uint8_t scale);
void SSD1309_DrawString(int16_t x, int16_t y,
                        const char *str, uint8_t scale);

/*
 * SSD1309_DrawTime
 *
 * Renders "HH:MM:SS" centred horizontally and vertically.
 *
 * With scale=3 each character is 15 px wide (5*3) and
 * 21 px tall (7*3), with 3 px gap between chars.
 *
 * "HH:MM:SS" = 8 chars + 2 colons = 8 chars total in the
 * string. Width = 8*(15+3) - 3 = 141 px  -- slightly too
 * wide for 128 px at scale 3.
 *
 * So we use scale=3 for HH:MM and put :SS smaller (scale=2)
 * below, giving a clean watch layout:
 *
 *   [        HH:MM        ]   y = 4,  scale 3, 21 px tall
 *   [          :SS        ]   y = 32, scale 2, 14 px tall
 *
 * Total height = 4 + 21 + 4 + 14 = 43, centred in 56 rows.
 */
void SSD1309_DrawTime(uint8_t hours, uint8_t minutes,
                      uint8_t seconds)
{
    char hhmm[6], ss[4];

    /* Format strings */
    hhmm[0] = '0' + (hours   / 10);
    hhmm[1] = '0' + (hours   % 10);
    hhmm[2] = ':';
    hhmm[3] = '0' + (minutes / 10);
    hhmm[4] = '0' + (minutes % 10);
    hhmm[5] = '\0';

    ss[0] = ':';
    ss[1] = '0' + (seconds / 10);
    ss[2] = '0' + (seconds % 10);
    ss[3] = '\0';

    /* Scale 3: char cell = (5*3 + 3) = 18 px wide, 5 chars */
    /* "HH:MM" = 5 chars => 5*18 - 3 = 87 px wide           */
    uint8_t scale_hhmm = 3;
    uint8_t char_w_hhmm = 5 * scale_hhmm + scale_hhmm; /* 18 */
    int16_t total_w_hhmm = 5 * char_w_hhmm - scale_hhmm; /* 87 */
    int16_t x_hhmm = (SSD1309_WIDTH - total_w_hhmm) / 2;
    int16_t y_hhmm = 4;

    /* Scale 2: char cell = (5*2 + 2) = 12 px wide, 3 chars */
    /* ":SS" = 3 chars => 3*12 - 2 = 34 px wide             */
    uint8_t scale_ss = 2;
    uint8_t char_w_ss = 5 * scale_ss + scale_ss;  /* 12 */
    int16_t total_w_ss = 3 * char_w_ss - scale_ss; /* 34 */
    int16_t x_ss = (SSD1309_WIDTH - total_w_ss) / 2;
    int16_t y_ss = y_hhmm + 7 * scale_hhmm + 4;  /* 4 + 21 + 4 = 29 */

    SSD1309_DrawString(x_hhmm, y_hhmm, hhmm, scale_hhmm);
    SSD1309_DrawString(x_ss,   y_ss,   ss,   scale_ss);
}

/* Draw a single character from the 5x7 font at scale */
void SSD1309_DrawChar(int16_t x, int16_t y,
                      char c, uint8_t scale)
{
    if (c < 0x20 || c > 0x7E) return;

    const uint8_t *glyph = font5x7[c - 0x20];

    for (uint8_t col = 0; col < 5; col++)
    {
        uint8_t column_data = glyph[col];
        for (uint8_t row = 0; row < 7; row++)
        {
            uint8_t on = (column_data >> row) & 0x01;
            if (on)
            {
                /* Expand each pixel by scale factor */
                for (uint8_t sy = 0; sy < scale; sy++)
                    for (uint8_t sx = 0; sx < scale; sx++)
                        SSD1309_SetPixel(
                            x + col * scale + sx,
                            y + row * scale + sy,
                            1);
            }
        }
    }
}

/* Draw a null-terminated string at scale */
void SSD1309_DrawString(int16_t x, int16_t y,
                        const char *str, uint8_t scale)
{
    int16_t cursor = x;
    /* Inter-character gap = 1 source pixel * scale */
    uint8_t advance = 5 * scale + scale;

    while (*str)
    {
        SSD1309_DrawChar(cursor, y, *str, scale);
        cursor += advance;
        str++;
    }
}
