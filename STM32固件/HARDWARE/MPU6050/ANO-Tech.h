
#ifndef __ANO_TECH_H_
#define	__ANO_TECH_H_

#include "stm32f10x.h"

void usart1_send_char(u8 c);
void usart1_niming_report(u8 fun,u8*data,u8 len);
void mpu6050_send_data(short aacx,short aacy,short aacz,short gyrox,short gyroy,short gyroz);
void usart1_report_imu(short aacx,short aacy,short aacz,short gyrox,short gyroy,short gyroz,short roll,short pitch,short yaw);
void ANO_DT_Send_Senser(s16 a_x,s16 a_y,s16 a_z,s16 g_x,s16 g_y,s16 g_z);
void Test_Send(short aacx, short aacy, short aacz, short gyrox, short gyroy, short gyroz, short roll, short pitch, short yaw, short Quat1, short Quat2, short Quat3, short Quat4);
void IMU_Send(short roll, short pitch, short yaw);
void Sensor_Send(short aacx, short aacy, short aacz, short gyrox, short gyroy, short gyroz);
void Quat_Send(int Quat1, int Quat2, int Quat3, int Quat4);
void RS232_Q(float Quat1,float Quat2,float Quat3,float Quat4);//四元数显示测试程序
void RS232_Angle(float pitch,float roll,float yaw);//角度显示测试程序
void RS232_Accel(float Accel_x,float Accel_y,float Accel_z);//角度显示测试程序
void RS232_Gyro(float Gyro_x,float Gyro_y,float Gyro_z);//角度显示测试程序
#endif
