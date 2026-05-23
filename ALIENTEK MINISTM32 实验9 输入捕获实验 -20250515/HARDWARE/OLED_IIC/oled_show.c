//#include "oled.h"
//#include "stdlib.h"
//#include "font.h"
//#include "delay.h"
#include "oled_show.h"

uint8_t SIZE = 24;	   // 字体大小
uint8_t SIZE_X = 18;   // 字体宽度
uint8_t SIZE_Y = 18;   // 字体高度
uint8_t N_SIZE_Y = 20; // 数字高度
uint8_t min_size = 8;

int Warn_Distance,Now_Distance,Now_Distance2,UltraError;
int zhen;//当前距离
int hong;//当前距离
extern u16 AD6Value,TargetSpeed;
extern int16_t encoder_cnt;
extern char encodernum;
void Oled_Show_Dis(void)
{
    //当前距离
//	OLED_ShowChinese(SIZE_X * 0, SIZE_Y * 2 - 10, 2, SIZE, 1);
//	OLED_ShowChinese(SIZE_X * 1, SIZE_Y * 2 - 10, 3, SIZE, 1);
//	OLED_ShowChinese(SIZE_X * 2, SIZE_Y * 2 - 10, 6, SIZE, 1);
//	OLED_ShowChinese(SIZE_X * 3, SIZE_Y * 2 - 10, 7, SIZE, 1);
//	OLED_ShowNum(SIZE_X * 5, N_SIZE_Y * 2 - 15, Now_Distance, 4, SIZE, 1);
//	//安全距离
//	OLED_ShowChinese(SIZE_X * 0, SIZE_Y * 3 - 10, 4, SIZE, 1);
//	OLED_ShowChinese(SIZE_X * 1, SIZE_Y * 3 - 10, 5, SIZE, 1);
//	OLED_ShowChinese(SIZE_X * 2, SIZE_Y * 3 - 10, 6, SIZE, 1);
//	OLED_ShowChinese(SIZE_X * 3, SIZE_Y * 3 - 10, 7, SIZE, 1);
//	OLED_ShowNum(SIZE_X * 5, N_SIZE_Y * 3 - 15, Now_Distance2, 4, SIZE, 1);
//	
//	OLED_ShowString(SIZE_X * 0, N_SIZE_Y * 1 - 15, "Z:", SIZE, 1);
//	OLED_ShowNum(   SIZE_X * 1, N_SIZE_Y * 1 - 15, zhen, 2, SIZE, 1);
//	OLED_ShowString(SIZE_X * 4, N_SIZE_Y * 1 - 15, "H:", SIZE, 1);
//	OLED_ShowNum(   SIZE_X * 5, N_SIZE_Y * 1 - 15, UltraError, 2, SIZE, 1);
	
//	OLED_ShowString(0, 0, "T:", SIZE, 1);
//	OLED_ShowString(105, 7, "RPM", 16, 1);
//	
//	OLED_ShowString(0, 35, "A:", SIZE, 1);
//	OLED_ShowString(105, 41, "RPM", 16, 1);
//	
//	OLED_ShowNum(   25, 0, TargetSpeed, 4, 24, 1);
//	OLED_ShowNum(   25, 35, encodernum, 4, 24, 1);
//	OLED_ShowString(73, 35, "00", SIZE, 1);
//	OLED_ShowString(73, 0 , "00", SIZE, 1);
	OLED_Refresh();
}


