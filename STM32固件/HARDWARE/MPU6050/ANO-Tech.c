
#include "usart.h"
#include "ANO-Tech.h"

//匿名地面站所需数据
u8 data_to_send[50];
#define BYTE0(dwTemp)       ( *( (char *)(&dwTemp)		) )
#define BYTE1(dwTemp)       ( *( (char *)(&dwTemp) + 1) )
extern unsigned char fifo_buffer[50];

/**
  **********************************
  * @brief  串口1发送1个字节
  * @param  c：发送数据
  * @retval None
  **********************************
*/
void usart1_send_char(u8 c)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET); //循环发送,直到发送完毕   
	USART_SendData(USART1, c);
	
}

/**
  **********************************
  * @brief  传送数据给匿名四轴上位机软件(V2.6版本)
  * @param  fun:功能字. 0XA0~0XAF  
						data:数据缓存区,最多28字节!!
						len:data区有效数据个数
  * @retval None
  **********************************
*/
void usart1_niming_report(u8 fun, u8*data, u8 len)
{
	u8 send_buf[32];
	u8 i;
	if (len>28)return;	//最多28字节数据 
	send_buf[len + 3] = 0;	//校验数置零
	send_buf[0] = 0X88;	//帧头
	send_buf[1] = fun;	//功能字
	send_buf[2] = len;	//数据长度
	for (i = 0;i<len;i++)send_buf[3 + i] = data[i];			//复制数据
	for (i = 0;i<len + 3;i++)send_buf[len + 3] += send_buf[i];	//计算校验和	
	for (i = 0;i<len + 4;i++)usart1_send_char(send_buf[i]);	//发送数据到串口1 
}

/**
  **********************************
  * @brief  发送加速度传感器数据和陀螺仪数据
  * @param  aacx,aacy,aacz:x,y,z三个方向上面的加速度值  
						gyrox,gyroy,gyroz:x,y,z三个方向上面的陀螺仪值
  * @retval None
  **********************************
*/
void mpu6050_send_data(short aacx, short aacy, short aacz, short gyrox, short gyroy, short gyroz)
{
	u8 tbuf[12];
	tbuf[0] = (aacx >> 8) & 0XFF;
	tbuf[1] = aacx & 0XFF;
	tbuf[2] = (aacy >> 8) & 0XFF;
	tbuf[3] = aacy & 0XFF;
	tbuf[4] = (aacz >> 8) & 0XFF;
	tbuf[5] = aacz & 0XFF;
	tbuf[6] = (gyrox >> 8) & 0XFF;
	tbuf[7] = gyrox & 0XFF;
	tbuf[8] = (gyroy >> 8) & 0XFF;
	tbuf[9] = gyroy & 0XFF;
	tbuf[10] = (gyroz >> 8) & 0XFF;
	tbuf[11] = gyroz & 0XFF;
	usart1_niming_report(0XA1, tbuf, 12);//自定义帧,0XA1
}

/**
  **********************************
  * @brief  通过串口1上报结算后的姿态数据给电脑
  * @param  aacx,aacy,aacz:x,y,z三个方向上面的加速度值
						gyrox,gyroy,gyroz:x,y,z三个方向上面的陀螺仪值
						roll:横滚角.单位0.01度。 -18000 -> 18000 对应 -180.00  ->  180.00度
						pitch:俯仰角.单位 0.01度。-9000 - 9000 对应 -90.00 -> 90.00 度
						yaw:航向角.单位为0.1度 0 -> 3600  对应 0 -> 360.0度
  * @retval None
  **********************************
*/
void usart1_report_imu(short aacx, short aacy, short aacz, short gyrox, short gyroy, short gyroz, short roll, short pitch, short yaw)
{
	u8 tbuf[28];
	u8 i;
	for (i = 0;i<28;i++)tbuf[i] = 0;//清0
	tbuf[0] = (aacx >> 8) & 0XFF;
	tbuf[1] = aacx & 0XFF;
	tbuf[2] = (aacy >> 8) & 0XFF;
	tbuf[3] = aacy & 0XFF;
	tbuf[4] = (aacz >> 8) & 0XFF;
	tbuf[5] = aacz & 0XFF;
	tbuf[6] = (gyrox >> 8) & 0XFF;
	tbuf[7] = gyrox & 0XFF;
	tbuf[8] = (gyroy >> 8) & 0XFF;
	tbuf[9] = gyroy & 0XFF;
	tbuf[10] = (gyroz >> 8) & 0XFF;
	tbuf[11] = gyroz & 0XFF;
	tbuf[18] = (roll >> 8) & 0XFF;
	tbuf[19] = roll & 0XFF;
	tbuf[20] = (pitch >> 8) & 0XFF;
	tbuf[21] = pitch & 0XFF;
	tbuf[22] = (yaw >> 8) & 0XFF;
	tbuf[23] = yaw & 0XFF;
	usart1_niming_report(0XAF, tbuf, 28);//飞控显示帧,0XAF
}

/**
  **********************************
  * @brief  传送数据给匿名地面站软件(V4.0版本)
  * @param  data:数据缓存区,最多28字节!!
						len:data区有效数据个数
  * @retval None
  **********************************
*/
void ANO_DT_Send_Data(u8 *data, u8 len)
{
	u8 i;
	if (len>28)return;	//最多28字节数据 
	for (i = 0;i<len;i++)usart1_send_char(data[i]);	//发送数据到串口1
}

/**
  **********************************
  * @brief  发送加速度传感器数据和陀螺仪数据
  * @param  aacx,aacy,aacz:x,y,z三个方向上面的加速度值  
						gyrox,gyroy,gyroz:x,y,z三个方向上面的陀螺仪值
  * @retval None
  **********************************
*/
void ANO_DT_Test_Send(s16 a_x,s16 a_y,s16 a_z,s16 g_x,s16 g_y,s16 g_z)
{
	u8 _cnt=0;
	vs16 _temp;
	
	data_to_send[_cnt++]=0xAA;
	data_to_send[_cnt++]=0xAA;
	data_to_send[_cnt++]=0x02;
	data_to_send[_cnt++]=0;
	
	_temp = a_x;
	data_to_send[_cnt++]=BYTE1(_temp);
	data_to_send[_cnt++]=BYTE0(_temp);
	_temp = a_y;
	data_to_send[_cnt++]=BYTE1(_temp);
	data_to_send[_cnt++]=BYTE0(_temp);
	_temp = a_z;	
	data_to_send[_cnt++]=BYTE1(_temp);
	data_to_send[_cnt++]=BYTE0(_temp);
	
	_temp = g_x;	
	data_to_send[_cnt++]=BYTE1(_temp);
	data_to_send[_cnt++]=BYTE0(_temp);
	_temp = g_y;	
	data_to_send[_cnt++]=BYTE1(_temp);
	data_to_send[_cnt++]=BYTE0(_temp);
	_temp = g_z;	
	data_to_send[_cnt++]=BYTE1(_temp);
	data_to_send[_cnt++]=BYTE0(_temp);
	
	data_to_send[3] = _cnt-4;
	
	u8 sum = 0;
	for(u8 i=0;i<_cnt;i++)
		sum += data_to_send[i];
	data_to_send[_cnt++] = sum;
	
	ANO_DT_Send_Data(data_to_send, _cnt);
}

void Fifo_Buffer_Send()
{
	u8 tbuf[31];
	u8 i,t;
	for (i = 0;i<31;i++)tbuf[i] = 0;//清0
	tbuf[0] = 0xa5;
	tbuf[1] = 0x0a;
	tbuf[2] = fifo_buffer[0];
	tbuf[3] = fifo_buffer[1];
	tbuf[4] = fifo_buffer[4];
	tbuf[5] = fifo_buffer[5];
	tbuf[6] = fifo_buffer[8];
	tbuf[7] = fifo_buffer[9];
	tbuf[8] = fifo_buffer[12];
	tbuf[9] = fifo_buffer[13];
	tbuf[10] = fifo_buffer[16];
	tbuf[11] = fifo_buffer[17];
	tbuf[12] = fifo_buffer[20];
	tbuf[13] = fifo_buffer[21];
	tbuf[14] = fifo_buffer[24];
	tbuf[15] = fifo_buffer[25];
	tbuf[16] = fifo_buffer[28];
	tbuf[17] = fifo_buffer[29];
	tbuf[18] = fifo_buffer[32];
	tbuf[19] = fifo_buffer[33];
	tbuf[20] = fifo_buffer[36];
	tbuf[21] = fifo_buffer[37];
	tbuf[22] = 0x0d;
	tbuf[23] = 0x0a;
	for(t=0;t<31;t++)
	{
		USART_ClearFlag(USART1, USART_FLAG_TC);
		USART_SendData(USART1,tbuf[t]);
		while((USART_GetFlagStatus(USART1, USART_FLAG_TC))==	RESET);//等待发送结束
	}
}
void Test_Send(short aacx, short aacy, short aacz, short gyrox, short gyroy, short gyroz, short roll, short pitch, short yaw, short Quat1, short Quat2, short Quat3, short Quat4)
{
	u8 tbuf[31];
	u8 i;
	for (i = 0;i<31;i++)tbuf[i] = 0;//清0
	tbuf[0]=0xa5;
	tbuf[1]=0x0a;
	tbuf[2]=0x01;
	tbuf[3] = (aacx >> 8) & 0XFF;
	tbuf[4] = aacx & 0XFF;
	tbuf[5] = (aacy >> 8) & 0XFF;
	tbuf[6] = aacy & 0XFF;
	tbuf[7] = (aacz >> 8) & 0XFF;
	tbuf[8] = aacz & 0XFF;
	tbuf[9] = (gyrox >> 8) & 0XFF;
	tbuf[10] = gyrox & 0XFF;
	tbuf[11] = (gyroy >> 8) & 0XFF;
	tbuf[12] = gyroy & 0XFF;
	tbuf[13] = (gyroz >> 8) & 0XFF;
	tbuf[14] = gyroz & 0XFF;
	tbuf[15] = (roll >> 8) & 0XFF;
	tbuf[16] = roll & 0XFF;
	tbuf[17] = (pitch >> 8) & 0XFF;
	tbuf[18] = pitch & 0XFF;
	tbuf[19] = (yaw >> 8) & 0XFF;
	tbuf[20] = yaw & 0XFF;
	tbuf[21] = (Quat1 >> 8) & 0XFF;
	tbuf[22] = Quat1 & 0XFF;
	tbuf[23] = (Quat2 >> 8) & 0XFF;
	tbuf[24] = Quat2 & 0XFF;
	tbuf[25] = (Quat3 >> 8) & 0XFF;
	tbuf[26] = Quat3 & 0XFF;
	tbuf[27] = (Quat4 >> 8) & 0XFF;
	tbuf[28] = Quat4 & 0XFF;
	tbuf[29]=0x0d;
	tbuf[30]=0x0a;
	for (i = 0;i<31;i++)usart1_send_char(tbuf[i]);	//发送数据到串口1 
//	for(t=0;t<31;t++)
//	{
//		USART_ClearFlag(USART1, USART_FLAG_TC);
//		USART_SendData(USART1,tbuf[t]);
//		while((USART_GetFlagStatus(USART1, USART_FLAG_TC))==	RESET);//等待发送结束
//	}
}
void IMU_Send(short roll, short pitch, short yaw)
{
	u8 tbuf[11];
	u8 i,t;
	for (i = 0;i<11;i++)tbuf[i] = 0;//清0
	tbuf[0]=0xa5;
	tbuf[1]=0x0a;
	tbuf[2]=0x01;
	tbuf[3] = (roll >> 8) & 0XFF;
	tbuf[4] = roll & 0XFF;
	tbuf[5] = (pitch >> 8) & 0XFF;
	tbuf[6] = pitch & 0XFF;
	tbuf[7] = (yaw >> 8) & 0XFF;
	tbuf[8] = yaw & 0XFF;
	tbuf[9]=0x0d;
	tbuf[10]=0x0a;
	
	for(t=0;t<11;t++)
	{
		USART_ClearFlag(USART1, USART_FLAG_TC);
		USART_SendData(USART1,tbuf[t]);
		while((USART_GetFlagStatus(USART1, USART_FLAG_TC))==	RESET);//等待发送结束
	}
}


void Sensor_Send(short aacx, short aacy, short aacz, short gyrox, short gyroy, short gyroz)
{
	u8 tbuf[17];
	u8 i,t;
	for (i = 0;i<17;i++)tbuf[i] = 0;//清0
	tbuf[0]=0xa5;
	tbuf[1]=0x0a;
	tbuf[2]=0x01;
	tbuf[3] = (aacx >> 8) & 0XFF;
	tbuf[4] = aacx & 0XFF;
	tbuf[5] = (aacy >> 8) & 0XFF;
	tbuf[6] = aacy & 0XFF;
	tbuf[7] = (aacz >> 8) & 0XFF;
	tbuf[8] = aacz & 0XFF;
	tbuf[9] = (gyrox >> 8) & 0XFF;
	tbuf[10] = gyrox & 0XFF;
	tbuf[11] = (gyroy >> 8) & 0XFF;
	tbuf[12] = gyroy & 0XFF;
	tbuf[13] = (gyroz >> 8) & 0XFF;
	tbuf[14] = gyroz & 0XFF;

	tbuf[15]=0x0d;
	tbuf[16]=0x0a;
	
	for(t=0;t<17;t++)
	{
		USART_ClearFlag(USART1, USART_FLAG_TC);
		USART_SendData(USART1,tbuf[t]);
		while((USART_GetFlagStatus(USART1, USART_FLAG_TC))==	RESET);//等待发送结束
	}
}


void Quat_Send(int Quat1, int Quat2, int Quat3, int Quat4)
{
	u8 tbuf[13];
	u8 i,t;
	for (i = 0;i<13;i++)tbuf[i] = 0;//清0
	tbuf[0]=0xa5;
	tbuf[1]=0x0a;
	tbuf[2]=0x01;
	tbuf[3] = (Quat1 >> 8) & 0XFF;
	tbuf[4] = Quat1 & 0XFF;
	tbuf[5] = (Quat2 >> 8) & 0XFF;
	tbuf[6] = Quat2 & 0XFF;
	tbuf[7] = (Quat3 >> 8) & 0XFF;
	tbuf[8] = Quat3 & 0XFF;
	tbuf[9] = (Quat4 >> 8) & 0XFF;
	tbuf[10] = Quat4 & 0XFF;
	tbuf[11]=0x0d;
	tbuf[12]=0x0a;
	
	for(t=0;t<13;t++)
	{
		USART_ClearFlag(USART1, USART_FLAG_TC);
		USART_SendData(USART1,tbuf[t]);
		while((USART_GetFlagStatus(USART1, USART_FLAG_TC))==	RESET);//等待发送结束
	}
}

void RS232_Q(float Quat1,float Quat2,float Quat3,float Quat4)//四元数显示测试程序
{
	printf("Quat1 = %.4f  ",Quat1);
	printf("Quat2 = %.4f  ",Quat2);
	printf("Quat3 = %.4f  ",Quat3);
	printf("Quat4 = %.4f  \n",Quat4);
}
void RS232_Angle(float pitch,float roll,float yaw)//角度显示测试程序
{
	printf("pitch = %.4f  ",pitch);
	printf("roll = %.4f  ",roll);
	printf("yaw = %.4f  \n",yaw);
}
void RS232_Accel(float Accel_x,float Accel_y,float Accel_z)//角度显示测试程序
{
	printf("Accel_x = %1.4f  ",Accel_x);
	printf("Accel_y = %1.4f  ",Accel_y);
	printf("Accel_z = %1.4f  \n",Accel_z);
}
void RS232_Gyro(float Gyro_x,float Gyro_y,float Gyro_z)//角度显示测试程序
{
	printf("Gyro_x = %1.4f  ",Gyro_x);
	printf("Gyro_y = %1.4f  ",Gyro_y);
	printf("Gyro_z = %1.4f  \n",Gyro_z);
}







