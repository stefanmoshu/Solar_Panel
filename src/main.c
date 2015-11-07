/*
 * main.c
 *
 *  Created on: 31.01.2015
 *      Author: Stefan
 */


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

//for creating folder
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <time.h>

#include "pigpio.h"
#include "../inc/log_data.h"
#include "../inc/spi_adc.h"
#include "../inc/motors.h"
#include "../inc/lcd_i2c.h"

#include "../inc/http.h"
#if (WATCHDOG_ACTIVE == 1)
#include <fcntl.h>
#include <linux/watchdog.h>
#endif

// Mutexes
pthread_mutex_t userCommandMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t autonomyCommandMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sensorDataMutex = PTHREAD_MUTEX_INITIALIZER;

// Mutex-controlled Variables
char* userCommand;
char* autonomyCommand;

unsigned char close_program_now = 0;
unsigned char web_motors_control = 0;
unsigned char g_log_counter = 0; // when counter reach 10 log to SD card
unsigned char pump1_diff_temp = INIT_PUMP1_DIFF_TEMP;

float current_temp_diff_pump1;  //diferenta de temperatura din secunda curenta
int pump_off_time = 0; // timpul de la oprire la pornirea pompei

//timp_min_pompa_1 = 120 ; in secunde - timp minim de functionare a pompei dupa ce a pornit
//timp_max_pompa_1 = 3600 ; in secunde - timp maxim de functionare a pompei

#define WATCHDOG_ACTIVE          0
#define DIFF_TEMP_LOGIC_ACTIVE   0

#define CHANGE_DIFF_TEMP_TO_HI_TIME    180  // timpul de crestere al PUMP1.ref_hi la care se modifica diferenta de temperatura in sus
#define CHANGE_DIFF_TEMP_TO_LOW_TIME   210  // se va lua in considerare cat sta oprita pompa
#define PUMP1_DIFF_TEMP          pump1_diff_temp //diferenta de temperatura dintre cei 2 senzori ce declanseaz pornirea pompei
#define PUMP1_HIS_DIFF_TEMP      8  //(histeresis) diferenta de temperatura dintre cei 2 senzori ce declanseaz oprirea pompei dupa ce a pornit
#define PUMP1_WAIT_TIME          90 //seconds
#define ANTIFREEZE_MAX_HI_TEMP   90 //grade celsius temperatura maxima la care poate ajunge antigelul.

#define PUMP2_DIFF_TEMP          20 //diferenta de temperatura dintre cei 2 senzori ce declanseaz pornirea pompei
#define PUMP2_HIS_DIFF_TEMP      8  //(histeresis) diferenta de temperatura dintre cei 2 senzori ce declanseaz oprirea pompei dupa ce a pornit

#define PUMP2_ON_TEMP            55
#define PUMP2_OFF_TEMP           45
#define PUMP2_WAIT_TIME          100 //seconds

#define PUMP_ON                  1
#define PUMP_OFF                 0
#define SERVO_ON                 1
#define SERVO_OFF                0

#define CONPARATOR_INPUT         4
#define BUTTON_INPUT             7

#define SERVO_TEMP_REF_HI        47
#define SERVO_TEMP_REF_LOW       45
#define SERVO_WAIT_TIME          60 //seconds

#define LOG_TO_CSV_FILE          0

typedef struct temp_diff_circuit_tag
{
   float ref_hi;        //temp senzorului conectat la sursa de incalzire
   float ref_low;       //temp senzorului conectat la rezervor
   char pomp_state; // on sau off
   int pump_time;    // timpul cand pompa si-a schimbat starea
}temp_diff_circuit_t;

typedef struct single_temp_sensor_circuit_tag
{
   float on_temp;        //temp la care sa porneasca pompa
   float off_temp;       //temp la care sa se opreasca pompa
   float sensor_temp;
   char pomp_state; // on sau off
   int pump_time;    // timpul cand pompa si-a schimbat starea
}single_temp_sensor_circuit_t;

typedef struct servo_circuit_tag
{
   float sensor_temp;
   char servo_state; // on sau off
   int servo_time;    // timpul cand pompa si-a schimbat starea
}servo_circuit_t;

temp_diff_circuit_t PUMP1;
temp_diff_circuit_t PUMP2;
servo_circuit_t SERVO;

void handle_servos (void)
{
   int mics;
   int current_time;
   char i;

   gpioTime(PI_TIME_ABSOLUTE , &current_time, &mics);
   SERVO.sensor_temp = TEMP_ADC_CH(SERVO_TEMP_REF_SENSOR);
   //DATA_LOG.ADC3 = SERVO.sensor_temp;

   if (    (SERVO.sensor_temp >= SERVO_TEMP_REF_HI)
        && (SERVO.servo_state == SERVO_OFF)
        && (current_time > SERVO.servo_time + SERVO_WAIT_TIME)
      )
   {
      DATA_LOG.SERVO1 = 100;
      DATA_LOG.SERVO2 = 100;
      DATA_LOG.SERVO3 = 100;
      set_servo_pos(0, 100);
      set_servo_pos(1, 100);
      gpioWrite(RELAY_1, 1); // Set RELAY_1 high.
      SERVO.servo_state = SERVO_ON;
      g_log_counter = 10; // log data to sd card
      gpioTime(PI_TIME_ABSOLUTE , &SERVO.servo_time, &mics);
   }

   if (    (SERVO.sensor_temp <= SERVO_TEMP_REF_LOW)
        && (SERVO.servo_state == SERVO_ON)
        && (current_time > SERVO.servo_time + SERVO_WAIT_TIME)
      )
   {
      DATA_LOG.SERVO1 = 0;
      DATA_LOG.SERVO2 = 0;
      DATA_LOG.SERVO3 = 0;
      set_servo_pos(0, 0);
      set_servo_pos(1, 0);
      gpioWrite(RELAY_1, 0); // Set RELAY_1 low.
      SERVO.servo_state = SERVO_OFF;
      g_log_counter = 10; // log data to sd card
      gpioTime(PI_TIME_ABSOLUTE , &SERVO.servo_time, &mics);
   }

   for (i = 0; i < 2; ++i)
   {
      if ((1 == servos[i].on) && (current_time > servos[i].time + 4)) set_servo_pos(i, 255);  //stop the servo after 4 seconds
   }

}
void change_diff_temp_circuit_1(void)
{
   if (pump_off_time > 0)
   {
      if (pump_off_time < CHANGE_DIFF_TEMP_TO_HI_TIME)
      {
         pump1_diff_temp +=2;
         pump_off_time = 0;
      }
      else if ((pump_off_time > CHANGE_DIFF_TEMP_TO_LOW_TIME) && (pump1_diff_temp > INIT_PUMP1_DIFF_TEMP))
      {
         pump1_diff_temp -=2;
         pump_off_time = 0;
      }
      else pump_off_time = 0;
   }
}
void handle_temp_diff_circuit_1(void)
{
   int mics;
   int current_time;

   gpioTime(PI_TIME_ABSOLUTE , &current_time, &mics);
   PUMP1.ref_hi = TEMP_ADC_CH(REF_HI_PUMP1);
   PUMP1.ref_low = TEMP_ADC_CH(REF_LOW_PUMP1);
   current_temp_diff_pump1 = PUMP1.ref_hi - PUMP1.ref_low;
   //DATA_LOG.ADC1 = PUMP1.ref_hi;
   //DATA_LOG.ADC3 = PUMP1.ref_low;
   if (     (((PUMP1.ref_hi - PUMP1.ref_low) >= PUMP1_DIFF_TEMP) || (PUMP1.ref_hi >= ANTIFREEZE_MAX_HI_TEMP)) //activate pump when the defined diff temp is reached
         && (PUMP1.pomp_state == PUMP_OFF)
#if (DIFF_TEMP_LOGIC_ACTIVE == 1)
         && (current_time > PUMP1.pump_time + PUMP1_WAIT_TIME)
#endif
       )
   {
      pump1_pwm = 100;
      PUMP1.pomp_state = PUMP_ON;
      g_log_counter = 10; // log data to sd card
      pump_off_time = current_time - PUMP1.pump_time;
      gpioTime(PI_TIME_ABSOLUTE , &PUMP1.pump_time, &mics);
   }

#if (DIFF_TEMP_LOGIC_ACTIVE == 1)
   if (   (PUMP1.pomp_state == PUMP_ON) //change pump1_pwm according with diff temp => higher diff -> higher pwm
       && (current_time > PUMP1.pump_time + 2) //2 seconds full power to stabilize the motor
       )
   {
#define PUMP1_PWM_MAX      100
#define PUMP1_PWM_MIN      60
#define DIF_TEMP_P1_MAX    PUMP1_DIFF_TEMP //la 20 de grade diferenta e pwm maxim
#define DIF_TEMP_P1_MIN    (PUMP1_DIFF_TEMP - PUMP1_HIS_DIFF_TEMP)  //la 8 de grade diferenta e pwm minim

      if (current_temp_dif > DIF_TEMP_P1_MAX) current_temp_dif = DIF_TEMP_P1_MAX;
      if (current_temp_dif < DIF_TEMP_P1_MIN) current_temp_dif = DIF_TEMP_P1_MIN;
      //pwm = pwm_max - ((pwm_max - pwm_min) * (dif_max- curr_dif) / (dif_max - dif_min))
      pump1_pwm = PUMP1_PWM_MAX - (unsigned int)((PUMP1_PWM_MAX - PUMP1_PWM_MIN) * (DIF_TEMP_P1_MAX - current_temp_dif) / (DIF_TEMP_P1_MAX - DIF_TEMP_P1_MIN));
   }
#endif
   if (     (PUMP1.pomp_state == PUMP_ON)
         && (current_time > PUMP1.pump_time + PUMP1_WAIT_TIME)
#if (DIFF_TEMP_LOGIC_ACTIVE == 1)
         && ((PUMP1.ref_hi - PUMP1.ref_low) <= PUMP1_DIFF_TEMP - PUMP1_HIS_DIFF_TEMP)
#endif
       )
   {
      pump1_pwm = 0;
      PUMP1.pomp_state = PUMP_OFF;
      g_log_counter = 10; // log data to sd card
      DATA_LOG.P1_ON_TIME += (unsigned int)(current_time - PUMP1.pump_time);
      gpioTime(PI_TIME_ABSOLUTE , &PUMP1.pump_time, &mics);
   }
   DATA_LOG.PUMP1 = pump1_pwm;
}

void handle_temp_diff_circuit_2(void)
{
   int mics;
   int current_time;
   float current_temp_dif;

   gpioTime(PI_TIME_ABSOLUTE , &current_time, &mics);
   PUMP2.ref_hi = TEMP_ADC_CH(REF_HI_PUMP2);
   PUMP2.ref_low = TEMP_ADC_CH(REF_LOW_PUMP2);
   current_temp_dif = PUMP2.ref_hi - PUMP2.ref_low;
   //DATA_LOG.ADC5 = PUMP2.ref_hi;
   //DATA_LOG.ADC3 = PUMP2.ref_low;
   if (     ((PUMP2.ref_hi - PUMP2.ref_low) >= PUMP2_DIFF_TEMP) //activate pump when the defined diff temp is reached
         && (PUMP2.pomp_state == PUMP_OFF)
         && (current_time > PUMP2.pump_time + PUMP2_WAIT_TIME)
       )
   {
      pump2_pwm = 100;
      PUMP2.pomp_state = PUMP_ON;
      g_log_counter = 10; // log data to sd card
      gpioTime(PI_TIME_ABSOLUTE , &PUMP2.pump_time, &mics);
   }

   if (   (PUMP2.pomp_state == PUMP_ON) //change PUMP2_pwm according with diff temp => higher diff -> higher pwm
       && (current_time > PUMP2.pump_time + 2) //2 seconds full power to stabilize the motor
       )
   {
#define PUMP2_PWM_MAX      100
#define PUMP2_PWM_MIN      60
#define DIF_TEMP_P2_MAX    20 //la 20 de grade diferenta e pwm maxim
#define DIF_TEMP_P2_MIN    8  //la 8 de grade diferenta e pwm minim

      if (current_temp_dif > DIF_TEMP_P2_MAX) current_temp_dif = DIF_TEMP_P2_MAX;
      if (current_temp_dif < DIF_TEMP_P2_MIN) current_temp_dif = DIF_TEMP_P2_MIN;
      //pwm = pwm_max - ((pwm_max - pwm_min) * (dif_max- curr_dif) / (dif_max - dif_min))
      pump2_pwm = PUMP2_PWM_MAX - (unsigned int)((PUMP2_PWM_MAX - PUMP2_PWM_MIN) * (DIF_TEMP_P2_MAX - current_temp_dif) / (DIF_TEMP_P2_MAX - DIF_TEMP_P2_MIN));
   }
   if (     ((PUMP2.ref_hi - PUMP2.ref_low) <= PUMP2_DIFF_TEMP - PUMP2_HIS_DIFF_TEMP)
         && (PUMP2.pomp_state == PUMP_ON)
         && (current_time > PUMP2.pump_time + PUMP2_WAIT_TIME)
       )
   {
      pump2_pwm = 0;
      PUMP2.pomp_state = PUMP_OFF;
      g_log_counter = 10; // log data to sd card
      gpioTime(PI_TIME_ABSOLUTE , &PUMP2.pump_time, &mics);
   }
   DATA_LOG.PUMP2 = pump2_pwm;
}

#if 0
void handle_single_temp_sensor_circuit(void)
{
   int mics;
   int current_time;

   gpioTime(PI_TIME_ABSOLUTE , &current_time, &mics);
   PUMP2.sensor_temp = TEMP_ADC_CH(PUMP_2_TEMP_SENSOR);
   DATA_LOG.ADC3 = PUMP2.sensor_temp;

   if (    (PUMP2.sensor_temp >= PUMP2_ON_TEMP)
        && (PUMP2.pomp_state == PUMP_OFF)
        && (current_time > PUMP2.pump_time + PUMP2_WAIT_TIME)
      )
   {
      pump2_pwm = 100;
      DATA_LOG.PUMP2 = pump2_pwm;
      PUMP2.pomp_state = PUMP_ON;
      g_log_counter = 10; // log data to sd card
      gpioTime(PI_TIME_ABSOLUTE , &PUMP2.pump_time, &mics);
   }

   if (    (PUMP2.sensor_temp <= PUMP2_OFF_TEMP)
        && (PUMP2.pomp_state == PUMP_ON)
        && (current_time > PUMP2.pump_time + PUMP2_WAIT_TIME)
      )
   {
      pump2_pwm = 0;
      DATA_LOG.PUMP2 = pump2_pwm;
      PUMP2.pomp_state = PUMP_OFF;
      g_log_counter = 10; // log data to sd card
      gpioTime(PI_TIME_ABSOLUTE , &PUMP2.pump_time, &mics);
   }
}
#endif

void input_test(int gpio, int level, unsigned int tick)
{
   printf("gpio %d became %d at %ud\n", gpio, level, tick);
}

extern int handle_spi;
extern int handle_i2c;

int main ()
{
   int i2c_data = 0x08;
   int deviceHandle;  // wd handler

   init_log_data();

   PUMP1.pomp_state = PUMP_OFF;
   PUMP1.pump_time = 0;
   PUMP2.pomp_state = PUMP_OFF;
   PUMP2.pump_time = 0;

   servos[0].pin = SERVO_0_PIN;
   servos[1].pin = SERVO_1_PIN;
   //servos[2].pin = SERVO_2_PIN;

   gpioCfgClock(5,1,1); // sampling 5 us, PCM, PLLD
   gpioInitialise();

   handle_spi = spiOpen(0, 100000, 0);
   if (handle_spi < 0) WriteLog("%s:%d - spi not ok %d\n", __FILE__, __LINE__, handle_spi);
   printf("%s %d, %d\n", __FUNCTION__, __LINE__, handle_spi);

   handle_i2c = i2cOpen(1, 0x27, 0);  /*  i2cBus: 0-1; i2cAddr: 0x00-0x7F; i2cFlags: 0*/
   if (handle_i2c < 0) WriteLog("%s:%d - i2c not ok %d\n", __FILE__, __LINE__, handle_i2c);
   printf("%s %d, %d\n", __FUNCTION__, __LINE__, handle_i2c);

   LcdI2c_Init();
   LcdI2cBackLightOn(0,0,gpioTick()); //(int gpio, int level, unsigned int tick);
   LcdI2cCursor(0);
   lprintf("     Panou solar");
   LcdI2cCursor(20);
   lprintf("        V1.1");
   LcdI2cCursor(40);
   lprintf("booting up:");
   LcdI2cCursor(60);
   lprintf("[.               ]");
   LcdI2cCursor(62);
   //LcdI2cBackLightDim(0);

   gpioSetMode(PUMP_1_GPIO, PI_OUTPUT); // Set PUMP_1_GPIO as output.
   gpioSetMode(PUMP_2_GPIO, PI_OUTPUT);

   gpioSetMode(RELAY_1, PI_OUTPUT); // Set RELAY_1 as output.
#if (THREAD_MOTOR_CONTROL == 0)
   gpioSetPWMrange(PUMP_1_GPIO, 100); //set range of the pwm to be 100 units
   gpioSetPWMrange(PUMP_2_GPIO, 100);
#endif
   //gpioSetPullUpDown(PUMP_1_GPIO, PI_PUD_DOWN);   // Sets a pull-down.

   /* PIN 24 DEFECT! DEFECT! DEFECT!
    *
   gpioSetMode(CONPARATOR_INPUT, PI_INPUT);  // Set CONPARATOR_INPUT as input.
   gpioSetPullUpDown(CONPARATOR_INPUT, PI_PUD_UP);   // Sets a pull-up.
   gpioSetAlertFunc(CONPARATOR_INPUT, zero_crossing);
   */

   /* PIN 25 DEFECT! DEFECT! DEFECT!
    *
   gpioSetMode(INPUT_TEST_PIN, PI_INPUT);  // Set CONPARATOR_INPUT as input.
   gpioSetPullUpDown(INPUT_TEST_PIN, PI_PUD_UP);   // Sets a pull-up.
   gpioSetAlertFunc(INPUT_TEST_PIN, zero_crossing);
   printf("%s %d\n", __FUNCTION__, __LINE__);
*/
   //=====================================================================================
   gpioSetMode(CONPARATOR_INPUT, PI_INPUT);  // Set CONPARATOR_INPUT as input.
   gpioSetMode(BUTTON_INPUT, PI_INPUT);  // Set BUTTON_INPUT as input.
   gpioSetPullUpDown(CONPARATOR_INPUT, PI_PUD_UP);   // Sets a pull-up.
   gpioSetPullUpDown(BUTTON_INPUT, PI_PUD_UP);   // Sets a pull-up.

   gpioSetAlertFunc(CONPARATOR_INPUT, zero_crossing);
   gpioSetAlertFunc(BUTTON_INPUT, LcdI2cBackLightOn);

   gpioSetWatchdog(CONPARATOR_INPUT, 3000); // call zero_crossing function after 3 seconds if no
                                          // no level change has been detected for the gpio
   printf("%s %d\n", __FUNCTION__, __LINE__);

   // Launch lcd back light thread
   //pthread_t *lcd_backlight_Thread;
   //lcd_backlight_Thread = gpioStartThread(&LcdI2cBackLightDim, NULL); sleep(3);
   //printf("%s %d\n", __FUNCTION__, __LINE__);
   //lprintf("...");

   // Launch read ADCs thread
   pthread_t *adcThread;
   adcThread = gpioStartThread(&read_adc, NULL); sleep(3);
   printf("%s %d\n", __FUNCTION__, __LINE__);
   lprintf("...");

#if (THREAD_MOTOR_CONTROL == 1)
   // Launch motor control thread
   pthread_t *motorThread;
   motorThread = gpioStartThread(&start_pwm_pump, NULL); sleep(3);
   printf("%s %d\n", __FUNCTION__, __LINE__);
   lprintf("...");
#endif

   //Launch print thred
   //pthread_t *printThread;
   //printThread = gpioStartThread(&console_read_char, NULL); sleep(3);
   //printf("%s %d\n", __FUNCTION__, __LINE__);

   // Launch log thread
   //pthread_t *logThread;
   //logThread = gpioStartThread(&log_data, NULL); sleep(3);
   //int logThreadExitCode = pthread_create( &logThread, NULL, &log_data, (void*) NULL);
   //printf("%s %d\n", __FUNCTION__, __LINE__);
   //lprintf("...");

   // Launch HTTP server
   pthread_t httpThread;
   httpThread = gpioStartThread(&launch_server, NULL); sleep(3);
   //int httpThreadExitCode = pthread_create( &httpThread, NULL, &launch_server, (void*) NULL); sleep(3);
   printf("%s %d\n", __FUNCTION__, __LINE__);
   lprintf("...");
   lprintf("\f");

#if (WATCHDOG_ACTIVE == 1)
   // open watchdog device on /dev/watchdog
   if ((deviceHandle = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0)
   {
      printf("Error: Couldn't open watchdog device! %d\n", deviceHandle);
      WriteLog("%s:%d - watchdog not ok %d\n", __FILE__, __LINE__, deviceHandle);
   }
   // get timeout info of watchdog (try to set it to 15s before)
   int timeout = 15;
   ioctl(deviceHandle, WDIOC_SETTIMEOUT, &timeout);
   ioctl(deviceHandle, WDIOC_GETTIMEOUT, &timeout);
   printf("The watchdog timeout is %d seconds.\n\n", timeout);
#endif
   while (1)
   {
      userCommand = malloc(sizeof(char)*10);
      autonomyCommand = malloc(sizeof(char)*10);
      char* copiedCommand = malloc(sizeof(char)*10);

      pthread_mutex_lock( &userCommandMutex );
      strncpy(&copiedCommand[0], &userCommand[0],10);
      pthread_mutex_unlock( &userCommandMutex );

#if (LOG_TO_CSV_FILE==0)
      //handle_temp_diff_circuit();
      //DATA_LOG.ADC1 = get_adc_mV(0);
      handle_temp_diff_circuit_1();
      if (web_motors_control != 1) // activate motor control from web
      {
         //handle_temp_diff_circuit_1();
         change_diff_temp_circuit_1();
         handle_temp_diff_circuit_2();
         //handle_single_temp_sensor_circuit();
         handle_servos ();
      }
      //DATA_LOG.ADC5 = TEMP_ADC_CH(AC_TRANSFORMATOR_SENSOR);

      //DATA_LOG.ADC7 = TEMP_ADC_CH(LM35);
      //DATA_LOG.ADC7 = get_adc_mV(6);
      DATA_LOG.ADC1 = TEMP_ADC_CH(ROOF_TEMP);
      DATA_LOG.ADC2 = TEMP_ADC_CH(TOP_CYLINDER_TEMP);
      DATA_LOG.ADC3 = TEMP_ADC_CH(MID_CYLINDER_TEMP);
      DATA_LOG.ADC4 = TEMP_ADC_CH(BOTTOM_CYLINDER_TEMP);
      DATA_LOG.ADC5 = TEMP_ADC_CH(CHIMNEY_TEMP);
      DATA_LOG.ADC6 = TEMP_ADC_CH(MCP9007_1);
      DATA_LOG.ADC8 = TEMP_ADC_CH(MCP9007_2);

      LcdI2cCursor(0);
      lprintf("T1=%5.1f\t",DATA_LOG.ADC1);
      LcdI2cCursor(10);
      lprintf("T2=%5.1f\t",DATA_LOG.ADC2);
      LcdI2cCursor(20);
      lprintf("T3=%5.1f\t",DATA_LOG.ADC3);
      LcdI2cCursor(30);
      lprintf("T4=%5.1f\t",DATA_LOG.ADC4);
      LcdI2cCursor(40);
      lprintf("T5=%5.1f\t",DATA_LOG.ADC5);
      LcdI2cCursor(50);
      lprintf("S1=%5d\t",DATA_LOG.SERVO1);
      LcdI2cCursor(60);
      lprintf("P1=%5d%%",pump1_pwm);
      LcdI2cCursor(70);
      lprintf("P2=%5d%%",pump2_pwm);

      if (lcd_backlight_on_tick != 0) // lcd button was pressed
         if ((gpioTick() - lcd_backlight_on_tick) > LCD_BACKLIGHT_ON_TIME ) // keep the LCD backlight ON for 1 minute
         {
            lcd_backlight_on_tick = 0;
            LcdI2cBackLightOff();
         }

      if (1 == close_program_now) break;
      if (g_log_counter > 9) //log data every 10 seconds
      {
         g_log_counter = 0;
         log_data_event();
#if (WATCHDOG_ACTIVE == 1)
         ioctl(deviceHandle, WDIOC_KEEPALIVE, 0); // feed watchdog with heartbeats every 10 seconds.
#endif
      }
      g_log_counter ++;
      sleep(1);
#else
      //log_to_csv(AC_TRANSFORMATOR_SENSOR);
      //get_last_AC_zero_crossing();
      sleep(10);
#endif
   }
   //LcdI2cClear();
   //LcdI2cCursor(0);
#if (WATCHDOG_ACTIVE == 1)
   ioctl(deviceHandle, WDIOC_KEEPALIVE, 0); // feed watchdog
#endif

   LcdI2cBackLightOn(0,0,gpioTick()); //(int gpio, int level, unsigned int tick);
   lprintf("\fBYE!!! BYE!!!");
   LcdI2cCursor(20);
   lprintf("Unplug AC after 60 sec");
   printf("close app in 20 seconds \n");
   WriteLog("%s:%d - Application will close in 20 seconds! \n", __FILE__, __LINE__);
   gpioStopThread(httpThread); sleep(3);
   //gpioStopThread(logThread); sleep(3);
#if (THREAD_MOTOR_CONTROL == 1)
   gpioStopThread(motorThread); sleep(3);
#endif
   gpioStopThread(adcThread); sleep(3);
   //gpioStopThread(lcd_backlight_Thread); sleep(3);
   gpioTerminate();
#if (WATCHDOG_ACTIVE == 1)
   printf("Disable watchdog.\n");
   write(deviceHandle, "V", 1);
   close(deviceHandle);
#endif
   /* Clean up and exit */
   printf("BYE!!! \n");
   exit(1);
   return 0;
}

