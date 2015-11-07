/*
 * spi_adc.h
 *
 *  Created on: 01.02.2015
 *      Author: Stefan
 */

#ifndef SPI_ADC_H_
#define SPI_ADC_H_

//*************ADC CHANELS DEFINE********************
#define ROOF_TEMP                6 // roof temp sensor
#define TOP_CYLINDER_TEMP        0 // top cylinder temp sensor
#define MID_CYLINDER_TEMP        1 // middle cylinder temp sensor
#define BOTTOM_CYLINDER_TEMP     2 // bottom cylinder temp sensor
#define CHIMNEY_TEMP             3 // chimney temp sensor
#define UNUSED                   4 // unused
#define MCP9007_1                5 // room temp
#define MCP9007_2                7 // CPU temp

#define REF_HI_PUMP1             ROOF_TEMP
#define REF_LOW_PUMP1            MID_CYLINDER_TEMP
#define REF_HI_PUMP2             CHIMNEY_TEMP
#define REF_LOW_PUMP2            MID_CYLINDER_TEMP
#define SERVO_TEMP_REF_SENSOR    MID_CYLINDER_TEMP
#define PUMP_2_TEMP_SENSOR       MID_CYLINDER_TEMP

typedef struct raw_adc_tag
{
   unsigned int value;
   unsigned int count;
   char logged;
}raw_adc_t;

extern int handle_spi;

void read_adc(void);

float get_adc_mV(unsigned char chan);
float get_filtered_adc_mV(unsigned char chan);
float TEMP_ADC_CH(unsigned char chan);
unsigned int get_adc_raw(unsigned char chan);

#endif /* SPI_ADC_H_ */
