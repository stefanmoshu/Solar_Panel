/*
 * main.c
 *
 *  Created on: 31.01.2015
 *      Author: Stefan
 */


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

//for creating folder
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <time.h>
#include "pigpio.h"
//create folder for year,month
//create files for days

#include "../inc/log_data.h"
#include "../inc/spi_adc.h"

//#include "pigpio.h"

//#define FILEPATH "/var/www/data_stefan.js"
#define FOLDER "/var/www/"
#define WEB_PATH "/var/www/index.html"
#define APP_LOG_PATH "/var/www/log.txt"
//#define CSV_FILE "test_ac.csv"
#define CSV_FILE csv_file

log_data_t DATA_LOG;
char file_path[30];
char file_relative_path[30];
char create_new_file_flag;  //flag for creating new file at 00:00
char csv_file[12];// = { __TIME__[0], __TIME__[1], __TIME__[2], __TIME__[3], __TIME__[4], __TIME__[5],'.', 'c','s','v'};
extern unsigned char pump1_diff_temp;

void WriteLog_1(char *str,...)
{

}

void WriteLog(char *str,...)
{
   time_t rawtime;
   struct tm * timeinfo;
   char buffer[25];
   FILE *fp;

   time ( &rawtime );
   timeinfo = localtime ( &rawtime );

   fp = fopen(APP_LOG_PATH,"a");
   if (NULL == fp)
   {
      fp = fopen(APP_LOG_PATH,"w");
      fseek(fp,0, SEEK_END);
   }

   strftime(buffer, 25, "%Y.%m.%d %H:%M:%S", timeinfo);
   fprintf(fp,"[%s]: ",buffer);
   va_list arglist;
   va_start(arglist,str);
   vfprintf(fp,str,arglist);
   va_end(arglist);

   //fprintf(fp," \n");
   fclose(fp);
}

#define FILE_NAME_OFFSET      412//232 - google
void change_web_file(void)
{
   FILE *fp;

   fp = fopen(WEB_PATH,"rb+");
   if (NULL == fp)
   {
      WriteLog("%s:%d - WEB_PATH dose not exist!\n", __FILE__, __LINE__);
   }
   fseek(fp, FILE_NAME_OFFSET, SEEK_SET); // go to file name and replace it with the new one
   fprintf(fp,"%s", file_relative_path);
   fclose(fp);
}

char * create_filePath(const struct tm *timeptr)
{
   sprintf(file_path, "%s%.4d/%.2d/%.4d%.2d%.2d.js", // eg. /var/www/2015/01/20150131.js
         FOLDER,
         1900 + timeptr->tm_year,
         1 + timeptr->tm_mon,
         1900 + timeptr->tm_year,
         1 + timeptr->tm_mon,
         timeptr->tm_mday);
   sprintf(file_relative_path, "./%.4d/%.2d/%.4d%.2d%.2d.js", // eg. ./2015/01/20150131.js
         1900 + timeptr->tm_year,
         1 + timeptr->tm_mon,
         1900 + timeptr->tm_year,
         1 + timeptr->tm_mon,
         timeptr->tm_mday);
   WriteLog("%s:%d - file path = %s\n", __FILE__, __LINE__,file_path);
   WriteLog("%s:%d - file relative path = %s\n", __FILE__, __LINE__,file_relative_path);
   return file_path;
}

char* asctime_local(const struct tm *timeptr)
{
   /*static const char wday_name[][4] = {
   "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
   };
   static const char mon_name[][4] = {
   "Jan", "Feb", "Mar", "Apr", "May", "Jun",
   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
   };*/
   static char result[26];
   //sprintf(result, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
   sprintf(result, "%.4d, %.2d, %.2d, %.2d, %.2d, %.2d",
   //wday_name[timeptr->tm_wday],
   1900 + timeptr->tm_year,
   //mon_name[timeptr->tm_mon],
   timeptr->tm_mon,
   timeptr->tm_mday,
   timeptr->tm_hour,
   timeptr->tm_min,
   timeptr->tm_sec);
   return result;
}

void init_log_data(void)
{
   time_t rawtime;
   struct tm * timeinfo;
   FILE *fp;
   char str1[] = "        var data1 = ([\r\n //google.visualization.arrayToDataTable([\r\n";
   char str2[] = "         //['time', 'ADC1', 'ADC2', 'ADC3', 'ADC4', 'ADC5', 'ADC6', 'ADC7', 'ADC8', 'PUMP1', 'PUMP2', 'PUMP3', 'SERVO1', 'SERVO2', 'SERVO3'],\r\n";
   //char str2[] = "         ['time', 'ADC1', 'ADC8', 'PUMP1', 'SERVO1'],\r\n";


   time ( &rawtime );
   timeinfo = localtime ( &rawtime );

   create_new_file_flag = 1;

   create_filePath(timeinfo);
   change_web_file();

   fp = fopen(file_path,"w");
   fseek(fp,0, SEEK_END);
   fprintf(fp,"%s",str1);
   fprintf(fp,"%s",str2);
   fprintf(fp,"]);");
   fclose(fp);

   strcpy(DATA_LOG.time, asctime_local (timeinfo));
   /*DATA_LOG.ADC1 = 0;
   DATA_LOG.ADC2 = 0;
   DATA_LOG.ADC3 = 0;
   DATA_LOG.ADC4 = 0;
   DATA_LOG.ADC5 = 0;
   DATA_LOG.ADC6 = 0;
   DATA_LOG.ADC8 = 0;*/
   DATA_LOG.P1_ON_TIME = 0;
   DATA_LOG.PUMP1 = 0;
   DATA_LOG.PUMP2 = 0;
   DATA_LOG.PUMP3 = 0;
   DATA_LOG.SERVO1 = 0;
   DATA_LOG.SERVO2 = 0;
   DATA_LOG.SERVO3 = 0;
   pump1_diff_temp = INIT_PUMP1_DIFF_TEMP;

   sprintf(csv_file, "%.2d%.2d%.2d.csv",
         timeinfo->tm_hour,
         timeinfo->tm_min,
         timeinfo->tm_sec);
   /*
   csv_file[0] = __TIME__[0];
   csv_file[1] = __TIME__[1];
   csv_file[2] = __TIME__[3];
   csv_file[3] = __TIME__[4];
   csv_file[4] = __TIME__[6];
   csv_file[5] = __TIME__[7];
   csv_file[6] = '.';
   csv_file[7] = 'c';
   csv_file[8] = 's';
   csv_file[9] = 'v';
   csv_file[10] = '\0';*/
   printf("csv file = %s\n",csv_file);
}

extern raw_adc_t raw_adc[9];

void log_data_event (void)
{
   time_t rawtime;
   struct tm * timeinfo;
   FILE *fp;
   int i = 0;

   for (i = 0; i < 9; ++i) { //mark adc channel as logged to SD card to reset the value & counter
      raw_adc[i].logged = 1;
   }

   time ( &rawtime );
   timeinfo = localtime ( &rawtime );
   //printf ( "Current local time and date: %s\n", asctime_local (timeinfo) );

   if ((timeinfo->tm_hour == 0) && (timeinfo->tm_min == 0) && (create_new_file_flag == 1))
   {
      create_filePath(timeinfo);
      create_new_file_flag = 0;
   }
   if ((timeinfo->tm_hour == 2) && (timeinfo->tm_min == 2))
   {
      create_new_file_flag = 1;
   }
   fp = fopen(file_path,"r+");
   if (NULL == fp)
   {
      init_log_data();
      fp = fopen(file_path,"r+");
      WriteLog("%s:%d - File dose not exist! Recreate.\n", __FILE__, __LINE__);
   }
   fseek(fp,-3, SEEK_END);
   //time ( &rawtime );
   //timeinfo = localtime ( &rawtime );
   //fprintf(fp,"[new Date(%s), %.5d,%.5d, %.5d, %.5d, %.5d, %.5d, %.5d, %.5d, %.5d, %.5d, %.5d, %.5d, %.5d, %.5d],\r\n",
   fprintf(fp,"[new Date(%s), %2.4f, %2.4f, %2.4f, %2.4f, %2.4f, %2.4f, %2.4f, %2.4f, %.2d, %.2d, %.2d, %.5d, %.5d, %.5d],\r\n]);",
            asctime_local (timeinfo),
            DATA_LOG.ADC1,
            DATA_LOG.ADC2,
            DATA_LOG.ADC3,
            DATA_LOG.ADC4,
            DATA_LOG.ADC5,
            DATA_LOG.ADC6,
            DATA_LOG.P1_ON_TIME,
            DATA_LOG.ADC8,
            DATA_LOG.PUMP1,
            DATA_LOG.PUMP2,
            DATA_LOG.PUMP3,
            DATA_LOG.SERVO1,
            DATA_LOG.SERVO2,
            DATA_LOG.SERVO3);
   fclose(fp);
}

void* log_data (void)
{

   while(1)
   {
      log_data_event();
      sleep(10);
   }
   //gpioTerminate();
   pthread_exit(NULL);
}

//void log_to_csv(unsigned char chan)
void log_to_csv(int raw_adc)
{
   time_t rawtime;
   struct tm * timeinfo;

   clock_t time_us;

   struct timespec time_ns;

   FILE *fp;
   static buff_count = 0;
   static unsigned char str[18100];
   static unsigned int time_buff[1000];
   static unsigned int adc_buff[1000];
   static writing_no = 0;
   unsigned int ticks;

   //time ( &rawtime );
   //timeinfo = localtime ( &rawtime );
   //clock_gettime(CLOCK_MONOTONIC, &time_ns);

   ticks = gpioTick();
   //time_buff[buff_count] =
   //sprintf(str + (17*buff_count),"%.9ld, %.4d \n", time_ns.tv_nsec, get_adc_raw(chan));
   //sprintf(str + (17*buff_count),"%.9ld, %.4d \n", time_ns.tv_nsec, raw_adc);
   sprintf(str + (17*buff_count),"%.9d, %.4d \n", ticks, raw_adc);
   buff_count++;
   if (buff_count == 999)
   {
      fp = fopen(csv_file,"a");
      if (NULL == fp)
      {
         fp = fopen(csv_file,"w");
      }
      fseek(fp,0, SEEK_END);
      fprintf(fp,"%s\n",str);
      //fprintf(fp,"%ld.%ld, %d\n",time_ns.tv_sec, time_ns.tv_nsec, get_adc_raw(chan));
      fclose(fp);
      memset(str,0,18000);
      buff_count = 0;
      writing_no ++;
   }
}
