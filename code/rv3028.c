/* ============================================================
 * RV3028-C7 Real-Time Clock driver
 *
 * The RV3028 stores time as BCD (Binary Coded Decimal).
 * For example, 23:59:47 is stored as 0x23, 0x59, 0x47.
 * We decode to plain decimal for the rest of the firmware.
 *
 * The RV3028 defaults to 24-hour mode on power-up, which
 * is what we want. No extra configuration needed for basic
 * timekeeping.
 * ============================================================ */

#include "rv3028.h"
#include "i2c.h"

extern I2C_HandleTypeDef hi2c1;

/* BCD to decimal conversion */
static inline uint8_t bcd2dec(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10) + (bcd & 0x0F));
}

/* Decimal to BCD conversion */
static inline uint8_t dec2bcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10) << 4) | (dec % 10));
}

static uint8_t RV3028_ReadReg(uint8_t reg)
{
    uint8_t val = 0;
    HAL_I2C_Master_Transmit(&hi2c1, RV3028_I2C_ADDR,
                             &reg, 1, HAL_MAX_DELAY);
    HAL_I2C_Master_Receive(&hi2c1, RV3028_I2C_ADDR,
                            &val, 1, HAL_MAX_DELAY);
    return val;
}

static void RV3028_WriteReg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    HAL_I2C_Master_Transmit(&hi2c1, RV3028_I2C_ADDR,
                             buf, 2, HAL_MAX_DELAY);
}

void RV3028_GetTime(uint8_t *hours, uint8_t *minutes,
                    uint8_t *seconds)
{
    /* Read all three time registers in one burst.
     * The RV3028 auto-increments its register pointer. */
    uint8_t reg = RV3028_REG_SECONDS;
    uint8_t raw[3] = {0};

    HAL_I2C_Master_Transmit(&hi2c1, RV3028_I2C_ADDR,
                             &reg, 1, HAL_MAX_DELAY);
    HAL_I2C_Master_Receive(&hi2c1, RV3028_I2C_ADDR,
                            raw, 3, HAL_MAX_DELAY);

    *seconds = bcd2dec(raw[0] & 0x7F); /* bit 7 is unused */
    *minutes = bcd2dec(raw[1] & 0x7F);
    *hours   = bcd2dec(raw[2] & 0x3F); /* bits 7:6 unused in 24h */
}

void RV3028_SetTime(uint8_t hours, uint8_t minutes,
                    uint8_t seconds)
{
    RV3028_WriteReg(RV3028_REG_SECONDS, dec2bcd(seconds));
    RV3028_WriteReg(RV3028_REG_MINUTES, dec2bcd(minutes));
    RV3028_WriteReg(RV3028_REG_HOURS,   dec2bcd(hours));
}
