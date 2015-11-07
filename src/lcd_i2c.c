/*
 * lcd_i2c.c
 *
 *  Created on: 24.07.2015
 *      Author: Stefan
 */
/*
 *
PC8574T  LCD 1602
P0       RS
P1       RW
P2       E
P3       Backlight
P4       D4
P5       D5
P6       D6
P7       D7
 */

#include "pigpio.h"
#include <stdio.h>
#include <stdarg.h>

#include "../inc/lcd_i2c.h"

int handle_i2c; //handler for i2cOpen

#define CLEAR ('\f')    // clear display
#define LINETWO ('\n')     // go to LINE 2
#define DEGREE_C ('\t')     // degree C

#define DATA      0x01
#define COMMAND   0x00
#define BACKLIGHT_ON    0x08
#define BACKLIGHT_OFF   0x00
int iDataFlag;
int iBacklightFlag;
unsigned char i2c_data = 0; //for dim the lcd backlight
unsigned char back_light_duty_cycle = 100;
unsigned int lcd_backlight_on_tick = 0;

/*********************************************************************
LcdI2cOneNible - send a single nibble to the LCD
*/
void LcdI2cOneNible(unsigned char lcd_nible);
void LcdI2cOneNible(unsigned char lcd_nible)
{
   lcd_nible &= 0x0F;
   lcd_nible <<= 4;
   lcd_nible |= iDataFlag;
   lcd_nible |= iBacklightFlag;
   i2c_data = lcd_nible;
   i2cWriteByte(handle_i2c, i2c_data);
   i2c_data |= 0x04;
   i2cWriteByte(handle_i2c, i2c_data); // assert E
   usleep(200);
   i2c_data &= 0xFB;
   i2cWriteByte(handle_i2c, i2c_data); // remove E
   usleep(200);
}

/*********************************************************************
LcdI2cByte - send two nibbles to the LCD, upper nibble first
*/
void LcdI2cByte (unsigned char lcd_byte);
void LcdI2cByte (unsigned char lcd_byte)
{
   unsigned char i;

   i = lcd_byte;      // temp store
   lcd_byte >>= 4;    // put upper nibble into lower 4 bits
   LcdI2cOneNible ( lcd_byte );  // send upper nibble
   usleep (100);  // make sure that LCD finishes this
   LcdI2cOneNible ( i );  // send lower nibble
   usleep (100);  // make sure that LCD finishes this
}

/*********************************************************************
LcdI2c_Init - Initialize the LCD for the following operating parameters:
   4 bit mode, 2 lines, 5x7
   turn on display and cursor: non-blinking
   incr address and shift cursor with each character
   This function should be called just once at system powerup.
   Running after powerup can cause LCD to scramble characters.

*/

void LcdI2c_Init ()
{
   i2cWriteByte(handle_i2c, 0);   // write 0
   iDataFlag = COMMAND;       // show command mode
   //iBacklightFlag = BACKLIGHT_OFF;

   //8 bit mod just to resets all previous LCD settings
   LcdI2cOneNible (3);          // 8 bit mode
   LcdI2cOneNible (3);          // 8 bit mode
   LcdI2cOneNible (8);          // 2 lines mode; 5x7 dots

   LcdI2cOneNible (2);          // 4 bit mode
   LcdI2cOneNible (2);          // 4 bit mode
   LcdI2cOneNible (8);          // 2 lines mode; 5x7 dots

   LcdI2cByte ( 0b00001100 );     // turn on display and cursor off: non-blinking
   LcdI2cByte ( 0b00000001 );     // clears display and cursor to home
   LcdI2cByte ( 0b00000011 );     // increment address and shift cursor with each character
}

/*********************************************************************
LcdI2cClear - clears the display

*/
void LcdI2cClear ( void );
void LcdI2cClear ( void )
{
   iDataFlag = COMMAND;    // set up for command
   //iBacklightFlag = BACKLIGHT_ON;
   LcdI2cByte ( 0x01 );    // clear the display
   usleep (3000);       // insure at least 2 msec
}


/*********************************************************************
LcdI2cLine2 - moves cursor to line 2.  Change 0xC0 to match your LCD's
line 2 starting address.

*/
void LcdI2cLine2 ( void );
void LcdI2cLine2 ( void )
{
   iDataFlag = COMMAND;    // set up for command
   //iBacklightFlag = BACKLIGHT_ON;
   LcdI2cByte ( 0xC0 );    // set RAM address to Line 2
                        //note: this value may vary for different displays
   usleep (3000);       // insure at least 2 msec
}

/*********************************************************************
LcdI2cCursor - move the cursor to c.

*/

void LcdI2cCursor (char c)    // cursor to c; 0-79 is range
{
   iDataFlag = COMMAND;
   // change the following line for LCDs with other than 20 chars/line
   if     (c > 59) LcdI2cByte((c-60) +84  + 0x80);
   else if(c > 39) LcdI2cByte((c-40) +20  + 0x80);
   else if(c > 19) LcdI2cByte((c-20) +64  + 0x80);
   else   LcdI2cByte(c + 0x80);
}

/*********************************************************************
LcdI2cDisplay - display a line of text on the LCD
   argument: address of null terminated text string
   \f clears the LCD
   \n moves cursor to LINE 2
*/
void LcdI2cDisplay ( char *szp );
void LcdI2cDisplay ( char *szp )
{
   //iBacklightFlag = BACKLIGHT_ON;
   while ( *szp )
   {
      if (isprint(*szp))
      {
         iDataFlag = DATA;
         LcdI2cByte (*szp++);
      }
      else
      {
         switch (*szp++)
         {
            case DEGREE_C: iDataFlag = DATA; LcdI2cByte(0xDF); break;
            case CLEAR: LcdI2cClear(); break;
            case LINETWO: LcdI2cLine2(); break;
         }
      }
   }
}

/*******************************************************************
lprintf - same as printf, but destination is the LCD

*/

void lprintf(const char *format, ...)
{
   char buf[128];
   va_list argptr;
   va_start(argptr, format);
   vsprintf(buf, format, argptr);
   va_end(argptr);
   LcdI2cDisplay(buf);
}

void LcdI2cBackLightDim (void)
{
   while (1)
   {
      if (back_light_duty_cycle >= 100) {iBacklightFlag = 0x08; sleep(2);}
      else if (back_light_duty_cycle == 0 ) {iBacklightFlag = 0x00; sleep(2);}
      else
      {
         iBacklightFlag = 0x08;
         i2c_data |= 0x08;
         i2cWriteByte(handle_i2c, i2c_data);
         usleep(100 * back_light_duty_cycle); //10 ms back light period
         iBacklightFlag = 0x00;
         i2c_data &= 0xF7;
         i2cWriteByte(handle_i2c, i2c_data);
         usleep(100 * (100 - back_light_duty_cycle));
      }
   }
}

void LcdI2cBackLightOn(int gpio, int level, unsigned int tick)
{
   if ((level == 0) && (tick > lcd_backlight_on_tick + LCD_BACKLIGHT_ON_TIME))
   {
      lcd_backlight_on_tick = tick;
      iBacklightFlag = 0x08;
      i2c_data |= 0x08;
      i2cWriteByte(handle_i2c, i2c_data);
      printf("gpio %d became %d at %ud\n", gpio, level, tick);
      WriteLog("%s:%d - LCD ON button pressed \n", __FILE__, __LINE__);
   }
}

void LcdI2cBackLightOff (void)
{
   iBacklightFlag = 0x00;
   i2c_data &= 0xF7;
   i2cWriteByte(handle_i2c, i2c_data);
}
