/* ============================================================
 * Watch firmware - STM32WB50CGU5
 * Display : CFAL12856A0 (SSD1309, 128x56) on I2C1
 * RTC     : RV3028-C7 on I2C1
 * I2C1    : PB8 = SCL (pin 46), PB9 = SDA (pin 47)
 *
 * Reads HH:MM:SS from the RV3028 every second and
 * renders it centred on the OLED using a large 5x7 font
 * scaled 3x for readability on a watch face.
 * ============================================================ */

#include "main.h"
#include "i2c.h"
#include "ssd1309.h"
#include "rv3028.h"
#include "font.h"

/* Update interval - check RTC every 500 ms so display
 * never lags more than half a second behind real time  */
#define UPDATE_INTERVAL_MS  500

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* I2C1 on PB8/PB9, 400 kHz fast mode */
    MX_I2C1_Init();

    /* Bring up display */
    SSD1309_Init();
    SSD1309_Clear();

    /* Optional: set initial time if the RTC has never been
     * programmed. Comment out after first flash.          */
    /* RV3028_SetTime(12, 0, 0); */

    uint32_t last_tick = 0;

    while (1)
    {
        uint32_t now = HAL_GetTick();
        if ((now - last_tick) >= UPDATE_INTERVAL_MS)
        {
            last_tick = now;

            uint8_t hours, minutes, seconds;
            RV3028_GetTime(&hours, &minutes, &seconds);

            SSD1309_Clear();
            SSD1309_DrawTime(hours, minutes, seconds);
            SSD1309_Flush();
        }

        /* Sleep until next systick to save power */
        __WFI();
    }
}
