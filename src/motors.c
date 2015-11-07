/*
 * motors.c
 *
 *  Created on: 23.02.2015
 *      Author: Stefan
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "pigpio.h"

#include "../inc/spi_adc.h"
#include "../inc/log_data.h"
#include "../inc/motors.h"

#define ticks_in_10ms 10000
#define ZERRO_POINT_OFFSET 700

unsigned int zero_tick = 0;
unsigned int prev_zero_tick = 0;
unsigned int current_zero_tick = 0;

unsigned int undetected_zero_crossing_count;
unsigned int zero_cros_diff_time = 0;
unsigned int zero_cros_diff_time_mean = 9910;
unsigned int zero_cros_diff_count = 1;
char ac_present = 0;

unsigned int pwm_commands_count = 0;
unsigned int zero_crossing_count = 0;

unsigned int pump1_pwm = 0;
unsigned int pump2_pwm = 0;

//unsigned int servo1_pos = 0;
//unsigned int servo2_pos = 0;

servo_state_t servos[3];

void set_servo_pos(char sevo_nr, unsigned char pos) //position 0-100 0= 500; 100 = 2500
{
   int mics;
   int current_time;

   gpioTime(PI_TIME_ABSOLUTE , &current_time, &mics);

   servos[sevo_nr].time = current_time;
   servos[sevo_nr].pos = pos;
   servos[sevo_nr].on = 1;

   if (pos == 255)  // -1 = turn off the servo
   {
      gpioServo(servos[sevo_nr].pin, 0);
      servos[sevo_nr].on = 0;
   }
   else
   {
      gpioServo(servos[sevo_nr].pin, 500 + 20 * pos);
      servos[sevo_nr].on = 1;
   }
}
int get_last_AC_zero_crossing()
{
   //static int last_adc_raw_value = 0;
   //static int min_value = 0;
   int current_adc_raw_value;
   static unsigned int count = 0;
   static int raw_adc_buff[30];
   static unsigned int i = 0;
   static unsigned int tick1 = 0;
   static unsigned int tick2 = 0;
   int diffTick;
   char j;
   //char ac_present;
   int sum = 0;

   //current_adc_raw_value = get_adc_raw(AC_TRANSFORMATOR_SENSOR);
   raw_adc_buff[i % 30] = current_adc_raw_value;
   tick2 = gpioTick();

   for (j = 0; j < 30; ++j)
   {
      sum = sum + raw_adc_buff[j];
      //printf(" %.4d", raw_adc_buff[j]);
   }
   //printf("\r");
   if ((sum / 30) < 400)
   {
      ac_present = 0; //no AC
      i++;
   }
   else ac_present = 1;

   if (ac_present)
   {
      diffTick = tick2 - tick1;
      if ((current_adc_raw_value < 4) &&
          ((diffTick > 21000) ||
           ((diffTick > 9900) && (diffTick < 10100)) ||
           ((diffTick > 19500) && (diffTick < 20500))
          )
         )
      {
         tick1 = gpioTick();
         //zero_tick = tick1;
         //gpioWrite(PUMP_1_GPIO, 1); // Set PUMP_1_GPIO high.
         if (diffTick < 10500)
         {
            zero_cros_diff_time += diffTick;
            zero_cros_diff_count ++;
         }
         //gpioSetPWMfrequency(18, 100); // Set PUMP_1_GPIO to 100Hz.
         //gpioPWM(18, 50);
         //log_to_csv(current_adc_raw_value);
         //printf(".");
         //count = i;
         i++;
         undetected_zero_crossing_count = 1;
         return 0;
      }
      /*
      if ((current_adc_raw_value == 0)) //&& (i > count + 12))
      {
         log_to_csv(current_adc_raw_value);
         printf(".");
         count = i;
         i++;
         //gpioPWM(18, 50);
         return 0;
      }*/
/*
      else if ((raw_adc_buff[(i-2) % 30] > raw_adc_buff[(i-1) % 30]) &&
               (current_adc_raw_value > raw_adc_buff[(i-1) % 30]) &&
               (current_adc_raw_value < 500) &&
               //(i > count + 20)
               (diffTick > 8000)
               )
      {
//         //log_to_csv(current_adc_raw_value);
//         count = i;
//         if (current_adc_raw_value < 20) gpioPWM(18, 50);
         tick1 = gpioTick();
         zero_tick = tick1;
         gpioWrite(PUMP_1_GPIO, 1); // Set PUMP_1_GPIO high.
         //gpioSetPWMfrequency(18, 100); // Set PUMP_1_GPIO to 100Hz.
         //gpioPWM(18, 50);
         //log_to_csv(current_adc_raw_value);
         printf(".");
      }
      else {}
*/
      i++;
   }
   return 1;
}

/*function to execute when the timer elapsed */
void start_pwm_pump2_timer();
void start_pwm_pump2_timer()
{
   //gpioTrigger(PUMP_2_GPIO, 100, 0);
   gpioWrite(PUMP_2_GPIO, 0);
   //gpioSetTimerFunc(2 + (zero_crossing_count % 2), 10 + (pump2_pwm/10), NULL);
}

void zero_crossing(int gpio, int level, unsigned int tick)
{
   //static unsigned int adc_chan = 0;
   //unsigned int temp;
   switch (level)
   {
      case 1:
         ac_present = 1;
         current_zero_tick = tick;
         if (current_zero_tick > (prev_zero_tick + 9900))
         {
            gpioWrite(PUMP_1_GPIO, 1); // Set PUMP_1_GPIO high.
            gpioWrite(PUMP_2_GPIO, 1); // Set PUMP_2_GPIO high.
            prev_zero_tick = current_zero_tick;
            zero_tick = current_zero_tick;
            zero_crossing_count ++;
            //printf("gpio %d became %d at %ud\n", gpio, level, tick);
            //printf("%u\n",tick);

#if (THREAD_MOTOR_CONTROL == 0)
            //pump 2
            if (pump2_pwm > 90) gpioWrite(PUMP_2_GPIO, 0);
            else if (pump2_pwm < 10) gpioWrite(PUMP_2_GPIO, 1);
            else
            {
               gpioPWM(PUMP_2_GPIO, 0);
               gpioSetPWMfrequency(PUMP_2_GPIO, 250); //Dummy frequency
               gpioSetPWMfrequency(PUMP_2_GPIO, 100);
               gpioPWM(PUMP_2_GPIO, 100 - pump2_pwm);
               //gpioSetTimerFunc(0 + (zero_crossing_count % 2), 2000 - (pump2_pwm/10), start_pwm_pump2_timer);
            }
#endif
         }

         break;
      case PI_TIMEOUT:
         ac_present = 0;
         printf("ac_present = 0\n");

         if (pump1_pwm > 0)
         {
            gpioWrite(PUMP_1_GPIO, 0);
         }
         else
         {
            gpioWrite(PUMP_1_GPIO, 1);
         }

         if (pump2_pwm > 0)
         {
            gpioWrite(PUMP_2_GPIO, 0);
         }
         else
         {
            gpioWrite(PUMP_2_GPIO, 1);
         }
         break;
      default:
         break;
   }
}

void start_pwm_pump(void)
{
   unsigned int current_tick;
   int diffTick;
   unsigned int requested_pwm1;
   unsigned int requested_pwm2;

   //pump_pwm = 2;

   while (1)
   {
      /*pwm_commands_count ++;
      if (pwm_commands_count > 20000) // ~ 2 second
      {
         pwm_commands_count = 0;
         if (zero_crossing_count > 75) // ~ .75 seconds
         {
            ac_present = 1;
            printf("ac_present = 1\n");
         }
         else
         {
            ac_present = 0;
            printf("ac_present = 0\n");
         }
         zero_crossing_count = 0;
      }
*/
      if (ac_present == 1)
      {
         current_tick = gpioTick();
         diffTick = current_tick - zero_tick;

         //pump 1
         if (pump1_pwm > 90) gpioWrite(PUMP_1_GPIO, 0);
         else if (pump1_pwm < 10) gpioWrite(PUMP_1_GPIO, 1);
         else
         {
            requested_pwm1 = ticks_in_10ms + ZERRO_POINT_OFFSET - (pump1_pwm * 100);
            if ((diffTick >= requested_pwm1) && (diffTick <= (requested_pwm1 + 1000)))
            {
               gpioWrite(PUMP_1_GPIO, 0);
            }
            else
            {
               gpioWrite(PUMP_1_GPIO, 1);
            }
         }

         //pump 2
         if (pump2_pwm > 90) gpioWrite(PUMP_2_GPIO, 0);
         else if (pump2_pwm < 10) gpioWrite(PUMP_2_GPIO, 1);
         else
         {
            requested_pwm2 = ticks_in_10ms + ZERRO_POINT_OFFSET - (pump2_pwm * 100);
            if ((diffTick >= requested_pwm2) && (diffTick <= (requested_pwm2 + 1000)))
            {
               gpioWrite(PUMP_2_GPIO, 0);
            }
            else
            {
               gpioWrite(PUMP_2_GPIO, 1);
            }
         }
      } // ac_present = 1
      /*else
      {
         if (pump1_pwm > 0)
         {
            gpioWrite(PUMP_1_GPIO, 0);
         }
         else
         {
            gpioWrite(PUMP_1_GPIO, 1);
         }

         if (pump2_pwm > 0)
         {
            gpioWrite(PUMP_2_GPIO, 0);
         }
         else
         {
            gpioWrite(PUMP_2_GPIO, 1);
         }
      }*/
      usleep(50);
   }//while
}

void console_print(void)
{
   unsigned char x = 0;
   if (gpioInitialise())
   {
      WriteLog("%s:%d - Init OK\n", __FILE__, __LINE__);
   }
   else WriteLog("%s:%d - Init NOT OK\n", __FILE__, __LINE__);
   while (1)
   {
      if (zero_cros_diff_count != 0)
      {
//         if (zero_cros_diff_count == 1)
//            zero_cros_diff_time_mean = 9990;
//         else
            zero_cros_diff_time_mean = zero_cros_diff_time/zero_cros_diff_count;
      }
      else
      {
         zero_cros_diff_time_mean = 9990;
      }
      printf("ac_present: %d; mean: %d; count: %d; pump_pwm: %d\n",ac_present, zero_cros_diff_time_mean, zero_cros_diff_count, pump1_pwm);
      zero_cros_diff_time = zero_cros_diff_time_mean;
      zero_cros_diff_count = 1;
//      x++;
//      if ((x % 5) == 0)
//      {
//         pump_pwm++;
//         pump_pwm = pump_pwm % 10;
//      }
      sleep(5);
   }
   gpioTerminate();
   pthread_exit(NULL);
}

void console_read_char(void)
{
   char temp1;
   char temp2;
//   if (gpioInitialise())
//   {
//      WriteLog("%s:%d - Init OK\n", __FILE__, __LINE__);
//   }
//   else WriteLog("%s:%d - Init NOT OK\n", __FILE__, __LINE__);
   printf("insert pump PWM [0 .. 10] (%d)\n",pump2_pwm);
   while (1)
   {
      temp1 = getchar() - 48;
      if (temp1 <=9 && temp1 >= 0)
      {
         printf("insert pump PWM [0 .. 10] (%d)\n",temp1);
         pump2_pwm = temp1 * 10;
      }
      temp2 = getchar();
      sleep(1);
   }
//   gpioTerminate();
//   pthread_exit(NULL);
}

