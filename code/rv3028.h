#ifndef RV3028_H
#define RV3028_H

#include <stdint.h>

/* Fixed I2C address - cannot be changed on RV3028-C7 */
#define RV3028_I2C_ADDR     (0x52 << 1)  /* HAL 8-bit format */

/* Time register addresses (all values in BCD) */
#define RV3028_REG_SECONDS  0x00
#define RV3028_REG_MINUTES  0x01
#define RV3028_REG_HOURS    0x02

/* Read current time from RTC.
 * Returns decoded decimal values (not BCD). */
void RV3028_GetTime(uint8_t *hours, uint8_t *minutes,
                    uint8_t *seconds);

/* Set the time. Pass decimal values.
 * Call once after first flash to initialise the RTC. */
void RV3028_SetTime(uint8_t hours, uint8_t minutes,
                    uint8_t seconds);

#endif /* RV3028_H */
