/*
 * log_data.h
 *
 *  Created on: 01.02.2015
 *      Author: Stefan
 */

#ifndef LOG_DATA_H_
#define LOG_DATA_H_

typedef struct log_data_tag
{
   unsigned char time[26];
   float ADC1;
   float ADC2;
   float ADC3;
   float ADC4;
   float ADC5;
   float ADC6;
   float P1_ON_TIME;
   float ADC8;
   unsigned char PUMP1;
   unsigned char PUMP2;
   unsigned char PUMP3;
   unsigned char SERVO1;
   unsigned char SERVO2;
   unsigned char SERVO3;
}log_data_t;

extern log_data_t DATA_LOG;
extern char file_path[30];

void* log_data(void);
void log_data_event (void);

void init_log_data(void);
void WriteLog(char *str,...);
//void log_to_csv(unsigned char chan);
//int get_last_AC_zero_crossing();

#define INIT_PUMP1_DIFF_TEMP     20 // diferenta de temperatura cu care incepe sa functioneze pompa

#endif /* LOG_DATA_H_ */
