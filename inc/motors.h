/*
 * motors.h
 *
 *  Created on: 23.02.2015
 *      Author: Stefan
 */

#ifndef MOTORS_H_
#define MOTORS_H_

#define PUMP_1_GPIO 18
#define PUMP_2_GPIO 23

#define SERVO_0_PIN  22  //from servo power connector
#define SERVO_1_PIN  27
#define SERVO_2_PIN  17

#define RELAY_1      17

#define THREAD_MOTOR_CONTROL     0
#define SERVO_CONNECTED          0

typedef struct servo_state_tag
{
   char pin;
   char pos;
   char on;
   int time;
}servo_state_t;

extern servo_state_t servos[3];

extern unsigned int pump1_pwm;
extern unsigned int pump2_pwm;

void set_servo_pos(char sevo_nr, unsigned char pos);

void zero_crossing(int gpio, int level, unsigned int tick);

int get_last_AC_zero_crossing();
void start_pwm_pump(void);
void console_print(void);
void console_read_char(void);

#endif /* MOTORS_H_ */
