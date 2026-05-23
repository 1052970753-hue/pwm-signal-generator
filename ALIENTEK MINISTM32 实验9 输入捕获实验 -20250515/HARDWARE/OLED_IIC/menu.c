#include "stm32f10x.h"                  // Device header
#include "OLED.h"
#include "key.h"
#include "font.h"
#include <string.h>
#include "menu.h"

#define linenumber 3 //行数限制
uint8_t selectItem_current, selectItem_hidden, selectItem_precurrent, selectItem_prehidden, selectItem;
uint8_t KeyNum, interface=1, drawflag=1, menulevel=0;
float prey=16,prelen=20;
float value,course;
u8 fp,fps;

struct MenuItem
{
	char *DisplayString; //当前项目要显示的字符
	void(*Subs)(); //功能函数
	struct MenuItem *PostMenu; //当前项目的子菜单
	struct MenuItem *PreMenu; //当前项目的父菜单
};

struct MenuItem begin[] = {
{(char*)2,0,0,0},
{"on",start,0,0 },
};
//第一级
struct MenuItem MainMenu[] = {
{(char*)6,0,0,0},
{"陋室铭",poem1,0,0},
{"LOGO",0,0,0},
{"图形",0,0,0},
{"1",0,0,0},
{"2",0,0,0},
};
//第二级
struct MenuItem logo[]={
{(char*)3,0,0,0},
{"LOGO1",logo1,0,MainMenu},
{"LOGO2",logo2,0,MainMenu},
};
struct MenuItem graphics[]={
{(char*)5,0,0,0},
{"圆1",Circle1,0,MainMenu},
{"圆2",Circle2,0,MainMenu},
{"椭圆",ellipse1,0,MainMenu},
{"空",0,0,MainMenu},
};
struct MenuItem Setmenu1[]={
{(char*)3,0,0,0},
{"1-1",0,0,MainMenu},
{"1-2",0,0,MainMenu},
};
struct MenuItem *MenuPoint=begin;//起始页

void menu_init(void)
{
	MainMenu[2].PostMenu = logo;
	MainMenu[3].PostMenu = graphics;
	MainMenu[4].PostMenu = Setmenu1;
	selectItem_current =1; //现用行
	selectItem_hidden  =0; //隐藏行数量
	selectItem=selectItem_current+selectItem_hidden; //选择行
}

void menu(void)
{
	KeyNum = KEY_Scan();
	//OLED_ShowNumber(80,16,selectItem_current,1,16);
	//OLED_ShowNumber(80,32,selectItem_hidden,1,16);
	//OLED_ShowNumber(80,48,selectItem,1,16);
	if(interface == 0) //循环执行功能函数时，取消菜单的按键扫描
	switch(KeyNum)
	{
		case Key_up:
		{
			OLED_showclear();
			selectItem_current--; //现用行向上
			drawflag=1;
			if (selectItem_current==0) //到顶了
			{
				selectItem_current++;
				if(selectItem_hidden>0) //有隐藏行就减小
					selectItem_hidden--;
				else //没了就转到最后一项
				{
					selectItem_current = min((intptr_t)MenuPoint->DisplayString-1,linenumber);
					if((intptr_t)MenuPoint->DisplayString-1 > linenumber) //特殊情况菜单条目小于页面行数限制
					selectItem_hidden  = (intptr_t)MenuPoint->DisplayString-1 - linenumber;
				}
			}
		};break;
		case Key_down:
		{
			OLED_showclear();
			selectItem_current++;
			drawflag=1;
			if(selectItem_current > linenumber)//到底了
			{
				selectItem_current--;
				if (selectItem_current+selectItem_hidden < (intptr_t)MenuPoint->DisplayString-1) //选择行小于条目数就加隐藏行
					selectItem_hidden++;
				else //到条目数量了就转到第一行
				{
					selectItem_current =1;
					selectItem_hidden  =0;
				}
			}
			else if(selectItem_current > (intptr_t)MenuPoint->DisplayString-1) //特殊情况菜单条目小于页面行数限制
			{
				selectItem_current =1;
				selectItem_hidden  =0;
			}
		};break;
		case Key_left:
		{
			if(MenuPoint[selectItem].PreMenu != 0) //判断是否有上一级
			{
				OLED_showclear();
				MenuPoint = MenuPoint[selectItem].PreMenu;
				selectItem_current = selectItem_precurrent; //跳到之前的选择行
				selectItem_hidden  = selectItem_prehidden;
				menulevel--;
				drawflag=1; //绘制画面
			}
			else {selectItem = 1;MenuPoint = begin;interface = 1;} //没有就回起始页
		};break;
		case Key_right:
		{
			if (MenuPoint[selectItem].PostMenu != 0)//判断是否有下一级
			{				
				OLED_showclear();
				MenuPoint = MenuPoint[selectItem].PostMenu; //去下一级
				selectItem_precurrent = selectItem_current; //记录当前选择行
				selectItem_prehidden = selectItem_hidden;
				selectItem_current =1;
				selectItem_hidden  =0;
				menulevel++;
				drawflag=1;
			}
			else if(MenuPoint[selectItem].Subs != 0) //有功能就执行功能函数
				MenuPoint[selectItem].Subs();
		};break;
		default : //循环刷新页面
		{
			selectItem=selectItem_current+selectItem_hidden;
			display(MenuPoint,selectItem_current,selectItem_hidden);
		};break;
	}
	else {MenuPoint[selectItem].Subs();OLED_Refresh_Gram();}
}

void OLED_showclear(void)//0-6行清屏，从下往上数
{  
	u16 i,n;  
	for(i=0;i<6;i++)
	{
		for(n=0;n<128;n++)
		{
			OLED_GRAM[n][i]=0X00;  
		}
	}
}

//像素反色
void OLED_invert(int x,int y)
{
	u8 pos,bx,temp=0;
	if(x<0 || x>127 || y<0 || y>63)return;//超出范围了.
	pos=7-y/8;
	bx=y%8;
	temp=1<<(7-bx);
	if((OLED_GRAM[x][pos] & temp) == temp)
		OLED_GRAM[x][pos]&=~temp;
	else
		OLED_GRAM[x][pos]|=temp;
}

//反色线，起始坐标x y，长度，1向右 2向下
void OLED_invert_line(u8 x,u8 y,u8 length,u8 mode)
{
	if(mode == 1)
		for(u8 x1=x;x1<x+length;x1++)
			OLED_invert(x1,y);
	else
		for(u8 y1=y;y1<y+length;y1++)
			OLED_invert(x,y1);
}

void OLED_invert_full(u8 y,u8 x)
{
	for(u8 y1=y;y1<y+16;y1++)
		OLED_invert_line(14,y1,x,1);
}

//项目选择，行数，字符长度
void OLED_sel(float y,float len)
{
	float p=0.06f,y_err,len_err,len1=8*len+4;
	if(y>=1 && y<=3) y=16*(y-1)+16.5f;
	OLED_ShowChar(0,prey,' ',16,1);
	OLED_invert_full(prey,prelen);
	y_err=y-prey;
	prey=prey+p*y_err;
	len_err=len1-prelen;
	prelen=prelen+p*len_err;
	OLED_ShowChar(0,prey,'>',16,1);
	OLED_invert_full(prey,prelen);
}

int min(int a,int b)
{
	if (a<b)return a;
	return b;
}

void display(struct MenuItem * MenuPoint,int selectItem_current,int selectItem_hidden)
{
	int j;
	u16 x=16;
	u16 y=16;
	if(drawflag==1)
	{
		OLED_Show_CH(0,0,menulevel,12,1);
		OLED_Show_CH(121,13+(48*selectItem/(intptr_t)MenuPoint->DisplayString-1),0,16,1);
		for(u16 i=1; i<=(intptr_t)MenuPoint->DisplayString-1; i++)
			{OLED_DrawPoint(123,16+(48*i/(intptr_t)MenuPoint->DisplayString-1),1);
			OLED_DrawPoint(125,16+(48*i/(intptr_t)MenuPoint->DisplayString-1),1);}
		line(124,16,48,2);
		for ( j= selectItem_hidden+1; j < min((intptr_t)MenuPoint->DisplayString,linenumber+selectItem_hidden+1);j++)
		{
			OLED_Show_chString(x,y,(u8*)MenuPoint[j].DisplayString,16);
			y+=16;
		}
		drawflag=0; //页面元素只画一遍
		OLED_invert_full(prey,prelen); //提供初始选择框
	}
	OLED_sel(selectItem_current,strlen(MenuPoint[selectItem].DisplayString)); //移动选择框
	OLED_Refresh_Gram();
}

//页面切换
void menuturn(struct MenuItem *newnemu)
{
	OLED_showclear();
	drawflag=1;
	MenuPoint = newnemu;
	display(MenuPoint,selectItem_current,selectItem_hidden);
	interface=0;
}

void start(void)
{
	OLED_showclear();
	interface = 1;
	OLED_ShowChar(0,0,' ',12,1);OLED_ShowChar(6,0,' ',12,1);
	OLED_Show_CH(16,32,9,12,1);
	OLED_Show_CH(28,32,10,12,1);
	OLED_Show_CH(40,32,1,16,1);
	OLED_Show_CH(56,32,1,16,1);
	OLED_Show_CH(72,32,11,12,1);
	OLED_Show_CH(84,32,12,12,1);
	OLED_Show_CH(96,32,13,12,1);
	if(KeyNum >= 3 && KeyNum <= 6) menuturn(MainMenu); //任意按键跳转到主菜单
}

void poem1(void)
{
	OLED_showclear();
	interface = 1;
	static int xl=0,xh=0;
	OLED_Show_CH(0,0,1,12,1);
	for(u8 i=0;i<16;i++)
		OLED_Show_MY_String(0+8*i-xl,32,lsm[i+xh],16,1);
	xl++;
	if(xl==8) {xl=0;xh++;}
	if(xh==214 || KeyNum == 5) {xh=0;menuturn(MainMenu);}
}

void logo1(void)
{
	interface = 1;
	OLED_ShowBMP(mihoyo);
	if(KeyNum == 5) {OLED_Clear();menuturn(logo);}
}

void logo2(void)
{
	interface = 1;
	OLED_ShowBMP(yuanshen);
	if(KeyNum == 5) {OLED_Clear();menuturn(logo);}
}

void Circle1(void)
{
	interface = 1;
	OLED_showclear();
	OLED_Show_CH(0,0,2,12,1);
	circle(90,40,20);
	fp++;
	OLED_ShowString(0,16,"fps:",16,1);
	OLED_ShowNumber(32,16,fps,3,16);
	if(KeyNum == 5) {OLED_Clear();menuturn(graphics);}
}

void Circle2(void)
{
	interface = 1;
	OLED_showclear();
	OLED_Show_CH(0,0,2,12,1);
	OLED_DrawCircle2(90,40,20);
	fp++;
	OLED_ShowString(0,16,"fps:",16,1);
	OLED_ShowNumber(32,16,fps,3,16);
	if(KeyNum == 5) {OLED_Clear();menuturn(graphics);}
}

void ellipse1(void)
{
	static u16 i;
	interface = 1;
	OLED_showclear();
	OLED_Show_CH(0,0,2,12,1);
	ellipse(90,40,20,10,i);
	//OLED_ShowNumber(32,16,i,3,16);
	i++;fp++;
	OLED_ShowString(0,16,"fps:",16,1);
	OLED_ShowNumber(32,16,fps,3,16);
	if(KeyNum == 5) {i=0;OLED_Clear();menuturn(graphics);}
}
