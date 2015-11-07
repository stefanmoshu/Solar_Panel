/*
 * http.c
 *
 *  Created on: 01.04.2014
 *      Author: Stefan
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "mongoose.h"
#include "../inc/http.h"
#include "../inc/motors.h"
#include "../inc/log_data.h" //for WriteLog function

// Mutexes
extern pthread_mutex_t userCommandMutex;
extern pthread_mutex_t autonomyCommandMutex;
extern pthread_mutex_t sensorDataMutex;

// Mutex-controlled Variables
extern char* userCommand;
extern char* autonomyCommand;

extern unsigned int pump1_pwm;
extern unsigned int pump2_pwm;

extern unsigned char back_light_duty_cycle;
extern unsigned char close_program_now;
extern unsigned char web_motors_control;
extern unsigned char pump1_diff_temp;
//int range;
//int bearing;
//int pitch;
//int roll;

static int http_callback(struct mg_connection *conn);
// Launch HTTP server
void* launch_server() {
  struct mg_context *ctx;
  struct mg_callbacks callbacks;

  const char *options[] = {"listening_ports", "3000", NULL};
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.begin_request = http_callback;

  printf("Starting HTTP Server on port 3000\n");

  ctx = mg_start(&callbacks, NULL, options);
  printf("after Starting HTTP Server on port 3000\n");
  //while(1);
  //getchar();  // Wait until user hits "enter".  This will never happen when this
              // code runs on the tank at startup
  //mg_stop(ctx);
}

// HTTP server callback
static int http_callback(struct mg_connection *conn) {

    const struct mg_request_info *request_info = mg_get_request_info(conn);

    char* tempCommand = malloc(sizeof(char)*13);
    char* end;
    int value = 0;

    strncpy(&tempCommand[0], &request_info->query_string[0], 7);
    tempCommand[7] = 0;
    printf("Received command from HTTP: %.*s\n", 7, tempCommand);

    // Set received, so send it over to the control thread
    if ((tempCommand[0] == 's') && (tempCommand[1] == 'e') && (tempCommand[2] == 't')) {
      pthread_mutex_lock( &userCommandMutex );
      strncpy(&userCommand[0], &tempCommand[3], 4);
      pthread_mutex_unlock( &userCommandMutex );

      // Send an HTTP header back to the client
      mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n\r\n");

      switch ((char)*(&userCommand[0])) {
         case '1':
            value = strtod(&userCommand[1], &end);
            printf("valoare 1 = %d\n", value);
            WriteLog("%s:%d - PUMP 1 PWM = %d \n", __FILE__, __LINE__, value);
            pump1_pwm = value;
            //set_servo_pos(0, value);
            break;
         case '2':
            value = strtod(&userCommand[1], &end);
            printf("valoare 2 = %d\n", value);
            pump2_pwm = value;
            WriteLog("%s:%d - PUMP 2 PWM = %d \n", __FILE__, __LINE__, value);
            //set_servo_pos(1, value);
            break;
         //case 3: do not use! - bogus values when refresh web page
         case '4':
            value = strtod(&userCommand[1], &end);
            close_program_now = 1;  //Shouting down the system, first close panou_solar
            system("shutdown -h +1");
            printf("System shutdown in 60 sec");
            WriteLog("%s:%d - System shutdown in 60 sec", __FILE__, __LINE__);
            //servo2_pos = value;
            break;
         case '5':
            value = strtod(&userCommand[1], &end);
            close_program_now = 1;  //Reboot the system, first close panou_solar
            system("shutdown -r +1");
            printf("System reboot in 60 sec");
            WriteLog("%s:%d - System reboot in 60 sec", __FILE__, __LINE__);
            break;
         case '6':
            value = strtod(&userCommand[1], &end);
            printf("close_program_now = %d\n", value);
            close_program_now = value;
            WriteLog("%s:%d - close_program_now", __FILE__, __LINE__);
            break;
         case '7':
            value = strtod(&userCommand[1], &end);
            printf("LCD backlight Duty Cycle = %d\n", value);
            back_light_duty_cycle = value;
            WriteLog("%s:%d - LCD backlight Duty Cycle = %d \n", __FILE__, __LINE__, value);
            break;
         case '8':
            value = strtod(&userCommand[1], &end);
            printf("web_motors_control = %d\n", value);
            web_motors_control = value;
            WriteLog("%s:%d - web_motors_control = %d \n", __FILE__, __LINE__, value);
            break;
         case '9':
            value = strtod(&userCommand[1], &end);
            printf("pump1_diff_temp = %d\n", value);
            pump1_diff_temp = value;
            WriteLog("%s:%d - pump1_diff_temp = %d \n", __FILE__, __LINE__, value);
            break;
         default:
            printf("fara valoare\n");
            break;
      }
    }

    // Get received, so return sensor data
    /*else if ((tempCommand[0] == 'g') && (tempCommand[1] == 'e') && (tempCommand[2] == 't')) {
      // Get data from the variables while the mutex is locked
      pthread_mutex_lock( &sensorDataMutex );
      int tmpRange = range;
      int tmpBearing = bearing;
      int tmpPitch = pitch;
      int tmpRoll = roll;
      pthread_mutex_unlock( &sensorDataMutex );
      printf("Sensor data acquired.\n");

      // Prepare the response
      char response[100];
      int contentLength = snprintf(response, sizeof(response),
            "Range: %d   Bearing: %d   Pitch: %d   Roll: %d",
            tmpRange, tmpBearing, tmpPitch, tmpRoll);

      printf("Sending HTTP response: %s\n", response);

      // Send an HTTP response back to the client
      mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: %d\r\n"
              "\r\n"
              "%s",
              contentLength, response);

    }*/
    printf("Finished responding to HTTP request.\n");

    return 1;  // Mark as processed
}
