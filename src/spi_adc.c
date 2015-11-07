//#include <wiringPi.h>
//#include <mcp3004.h>
#include <stdio.h>
#include <stdlib.h>

#include "pigpio.h"
#include "../inc/spi_adc.h"

int handle_spi;

typedef struct termo_resistor_tag
{
   int temperature;
   float resistance;
   float mV;
   float adc_raw;
}termo_resistor_t;

const termo_resistor_t PS1000[] = {
      #include "../inc/PS1000.h"
};

const termo_resistor_t NTC10k[] = {
      #include "../inc/NTC10k.h"
};
raw_adc_t raw_adc[9];

#define NO_OF_FILLTERED_ADC_SAMPLES 256
#define DELAY_BTWN_FILLTERED_ADC_SAMPLES 100 //us
//#define BASE 200
//#define SPI_CHAN 0

float get_filtered_adc_raw(unsigned char chan); //Prototype

//change this to enable / disable adc channels read
//                                           0, 1, 2, 3, 4, 5, 6, 7
const unsigned char adc_chan_available[8] = {1, 1, 1, 1, 0, 1, 1, 1};

void read_adc (void)
{
   unsigned char temp;

//   if (gpioInitialise())
//   {
//      WriteLog("%s:%d - Init OK\n", __FILE__, __LINE__);
//   }
//   else WriteLog("%s:%d - Init NOT OK\n", __FILE__, __LINE__);

//   gpioSetMode(PUMP_1_GPIO, PI_OUTPUT); // Set PUMP_1_GPIO as output.
   for (temp = 0; temp < 9; ++temp)
   {
      raw_adc[temp].value = 0;
      raw_adc[temp].count = 0;
   }
   temp = 0;
   while (1)
   {
      if (temp > 7) temp = 0;
      //temp = adc_chan % 8;
      if (1 == raw_adc[temp].logged)
      {
         raw_adc[temp].value = 0;
         raw_adc[temp].count = 0;
         raw_adc[temp].logged = 0;
      }
      if (1 == adc_chan_available[temp])
      {
         raw_adc[temp].value += get_adc_raw(temp);
         raw_adc[temp].count ++;
         usleep(10000); // below 5000 us the pulse will get unstable
      }
      temp ++;
      //adc_chan ++;
   }
   //gpioTerminate();
   pthread_exit(NULL);
}

float get_PS1000_temp(float raw_value)
{
   int i = 0;
   int max_items;

   float PS1000_temp;

   max_items = sizeof(PS1000)/sizeof(PS1000[0]);
   //raw_value = get_filtered_adc_raw(chan);
   //printf("filtered %f\n", raw_value);
   if (raw_value > PS1000[0].adc_raw) PS1000_temp = PS1000[0].temperature; // highest voltage = lowest temp = -50
   else if (raw_value < PS1000[max_items-1].adc_raw) PS1000_temp = PS1000[max_items-1].temperature;  //lowest voltage = highest temp = 159
   else
   {
      //printf("size of %d\n",max_items);
      while ((raw_value <= PS1000[i].adc_raw) && (i < max_items))
      {
         ++i;
      }
      if (i == max_items) PS1000_temp = PS1000[i-1].temperature;
      else
      PS1000_temp = PS1000[i].temperature - ((PS1000[i].temperature - PS1000[i-1].temperature)*(PS1000[i].adc_raw - raw_value)/(PS1000[i].adc_raw - PS1000[i-1].adc_raw));
   }
   return PS1000_temp;
}

float get_NTC10k_temp(float raw_value)
{
   int i = 0;
   int max_items;

   float NTC10k_temp;

   max_items = sizeof(NTC10k)/sizeof(NTC10k[0]);
   //raw_value = get_filtered_adc_raw(chan);
   //printf("filtered %f\n", raw_value);
   if (raw_value < NTC10k[0].adc_raw) NTC10k_temp = NTC10k[0].temperature; //lowest voltage = lowest temp = -55
   else if (raw_value > NTC10k[max_items - 1].adc_raw) NTC10k_temp = NTC10k[max_items - 1].temperature; //highest voltage = highest temp = 125
   else
   {
      //printf("size of %d\n",max_items);
      while ((raw_value >= NTC10k[i].adc_raw) && (i < max_items))
      {
         ++i;
      }
      if (i == max_items) NTC10k_temp = NTC10k[i-1].temperature;
      else
         NTC10k_temp = NTC10k[i].temperature - ((NTC10k[i].temperature - NTC10k[i-1].temperature)*(NTC10k[i].adc_raw - raw_value)/(NTC10k[i].adc_raw - NTC10k[i-1].adc_raw));
   }
   return NTC10k_temp;
}

float TEMP_ADC_CH(unsigned char chan)
{
   float ret_val = 0;
   float chX_mV = 0;

   switch (chan)
   {
      case 6:
      case 0:
       if (raw_adc[chan].count == 0) ret_val = 0;
       else
       {
          ret_val = get_PS1000_temp((float)raw_adc[chan].value / raw_adc[chan].count);
          /*if (1 == raw_adc[chan].logged)
          {
             raw_adc[chan].value = 0;
             raw_adc[chan].count = 0;
             raw_adc[chan].logged = 0;
          }*/
       }
       //printf("temp ADC %d = %f\n",chan ,ret_val);
       break;
       /*
       chX_mV = (3.3 / 4096) * ((float)raw_adc[chan].value / raw_adc[chan].count);
       if (1 == raw_adc[chan].logged)
       {
          raw_adc[chan].value = 0;
          raw_adc[chan].count = 0;
          raw_adc[chan].logged = 0;
       }
       ret_val = chX_mV * 100; // LM35 DZ temperature sensor
       break;*/
      case 1:
      case 2:
      case 3:
         if (raw_adc[chan].count == 0) ret_val = 0;
         else
         {
            ret_val = get_NTC10k_temp((float)raw_adc[chan].value / raw_adc[chan].count);
            /*if (1 == raw_adc[chan].logged)
            {
               raw_adc[chan].value = 0;
               raw_adc[chan].count = 0;
               raw_adc[chan].logged = 0;
            }*/
         }
         //printf("temp ADC %d = %f\n",chan ,ret_val);
         break;
      //case 4:
      case 5:
      case 7:
         //ret_val = (get_filtered_adc_mV(chan) - 0.5) * 100; // MCP 9007 temperature sensor
         if (raw_adc[chan].count == 0) chX_mV = 0;
         else
         {
            chX_mV = (3.3 / 4096) * ((float)raw_adc[chan].value / raw_adc[chan].count);
            /*if (1 == raw_adc[chan].logged)
            {
               raw_adc[chan].value = 0;
               raw_adc[chan].count = 0;
               raw_adc[chan].logged = 0;
            }*/
            ret_val = (chX_mV - 0.5) * 100; // MCP 9007 temperature sensor
         }
         //ret_val = get_filtered_adc_mV(chan) * 100; // LM35 DZ temperature sensor
         break;

      default:
         ret_val = 0;
         break;
   }
   return ret_val;
}

float get_filtered_adc_mV(unsigned char chan)
{
   float sum;
   int i;

   sum = 0;
   for (i = 0; i < NO_OF_FILLTERED_ADC_SAMPLES; ++i)
   {
      sum = sum + get_adc_mV(chan);
      usleep(DELAY_BTWN_FILLTERED_ADC_SAMPLES);
   }
   return (sum/NO_OF_FILLTERED_ADC_SAMPLES);
}

float get_filtered_adc_raw(unsigned char chan)
{
   float sum;
   int i;

   sum = 0;
   for (i = 0; i < NO_OF_FILLTERED_ADC_SAMPLES; ++i)
   {
      sum = sum + get_adc_raw(chan);
      usleep(DELAY_BTWN_FILLTERED_ADC_SAMPLES);
   }
   return (sum/NO_OF_FILLTERED_ADC_SAMPLES);
}

unsigned int get_adc_raw(unsigned char chan)
{
   char data_write[10];
   char data_read[10];
   unsigned int raw_value = 0;

   data_write[0] = 1;  //  first byte transmitted -> start bit
   data_write[1] = 0b10000000 |( ((chan & 7) << 4)); // second byte transmitted -> (SGL/DIF = 1, D2=D1=D0=0)
   data_write[2] = 0; // third byte transmitted....don't care
   data_write[3] = 0;

   spiXfer(handle_spi, data_write, data_read, 4);
   raw_value = (data_read[1] << 10) | (data_read[2] << 2) | ((data_read[3] & 0xC0) >> 6);

   return raw_value;
}

float get_adc_mV(unsigned char chan)
{
   float chX_mV = 0;
   chX_mV = (3.3 / 4096) * get_adc_raw(chan);
   //printf("chX_mv = %2.4f (%d)\n",chX_mV,temp);

   return chX_mV;
}
