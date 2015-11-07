/*
 * lcd_i2c.h
 *
 *  Created on: 24.07.2015
 *      Author: Stefan
 */

#ifndef LCD_I2C_H_
#define LCD_I2C_H_

#define LCD_BACKLIGHT_ON_TIME    180000000 // 3 minute

void lprintf(const char *format, ...);
void LcdI2cCursor (char c);
void LcdI2c_Init ();
void LcdI2cBackLightDim (void);
void LcdI2cBackLightOff (void);
void LcdI2cBackLightOn(int gpio, int level, unsigned int tick);

extern unsigned int lcd_backlight_on_tick;

#endif /* LCD_I2C_H_ */
