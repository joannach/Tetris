//#include "stm32f10x_rcc.h"
//#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx.h"
#include "LCD.h"
#include "font.h"

//Define the LCD Operation function
void LCD5110_LCD_write_byte(unsigned char dat,unsigned char LCD5110_MOde);
void LCD5110_LCD_delay_ms(unsigned int t);

//Define the hardware operation function
void LCD5110_GPIO_Config(void);
void LCD5110_SCK(unsigned char temp);
void LCD5110_MO(unsigned char temp);
void LCD5110_CS(unsigned char temp);
void LCD5110_RST(unsigned char temp);
void LCD5110_DC(unsigned char temp);


#define BLINK_GPIOx(_N)                 ((GPIO_TypeDef *)(GPIOA_BASE + (GPIOB_BASE-GPIOA_BASE)*(_N)))
#define BLINK_PIN_MASK(_N)              (1 << (_N))
#define BLINK_RCC_MASKx(_N)             (RCC_AHB1ENR_GPIOAEN << (_N))

#define CE_PORT GPIOC
#define CE_PIN GPIO_PIN_13

#define RST_PORT GPIOC
#define RST_PIN GPIO_PIN_15

#define DC_PORT GPIOC
#define DC_PIN GPIO_PIN_14

#define MOSI_PORT GPIOC
#define MOSI_PIN GPIO_PIN_3

#define CLK_PORT GPIOB
#define CLK_PIN GPIO_PIN_10

/*
 * CE        - PC13
 * RST       - PC15
 * DC        - PC14
 * DIN(MOSI) - PC3
 * CLK       - PB10
 */
void LCD5110_GPIO_Config()
{
   GPIO_InitTypeDef GPIOB_Init;
   GPIO_InitTypeDef GPIOC_Init;


   RCC->AHB1ENR |= BLINK_RCC_MASKx(1); // portB
   RCC->AHB1ENR |= BLINK_RCC_MASKx(2); // portC

   GPIOB_Init.Speed = GPIO_SPEED_FAST; // mozna sp©óbowac szybciej
   GPIOB_Init.Mode = GPIO_MODE_OUTPUT_PP;
   GPIOB_Init.Pull = GPIO_PULLUP;//pullup?

   GPIOB_Init.Pin = CE_PIN;
   HAL_GPIO_Init(CE_PORT, &GPIOB_Init);

   GPIOB_Init.Pin = RST_PIN;
   HAL_GPIO_Init(RST_PORT, &GPIOB_Init);

   GPIOB_Init.Pin = DC_PIN;
   HAL_GPIO_Init(DC_PORT, &GPIOB_Init);

   GPIOB_Init.Pin = MOSI_PIN;
   HAL_GPIO_Init(MOSI_PORT, &GPIOB_Init);

   GPIOB_Init.Pin = CLK_PIN;
   HAL_GPIO_Init(CLK_PORT, &GPIOB_Init);
}


void LCD5110_init()
{
	LCD5110_GPIO_Config();

	LCD5110_DC(1);//LCD_DC = 1;
	LCD5110_MO(1);//SPI_MO = 1;
	LCD5110_SCK(1);//SPI_SCK = 1;
	LCD5110_CS(1);//SPI_CS = 1;
	
	LCD5110_RST(0);//LCD_RST = 0;
	LCD5110_LCD_delay_ms(10);
	LCD5110_RST(1);//LCD_RST = 1;

	LCD5110_LCD_write_byte(0x21,0);
	LCD5110_LCD_write_byte(0xc6,0);
	LCD5110_LCD_write_byte(0x06,0);
	LCD5110_LCD_write_byte(0x13,0);
	LCD5110_LCD_write_byte(0x20,0);
	LCD5110_clear();
	LCD5110_LCD_write_byte(0x0c,0);
}

void LCD5110_LCD_write_byte(unsigned char dat,unsigned char mode)
{
	unsigned char i;

	LCD5110_CS(0);//SPI_CS = 0;

	if (0 == mode)
		LCD5110_DC(0);//LCD_DC = 0;
	else
		LCD5110_DC(1);//LCD_DC = 1;

	for(i=0;i<8;i++)
	{
		LCD5110_MO(dat & 0x80);//SPI_MO = dat & 0x80;
		dat = dat<<1;
		LCD5110_SCK(0);//SPI_SCK = 0;
		LCD5110_SCK(1);//SPI_SCK = 1;
	}

	LCD5110_CS(1);//SPI_CS = 1;

}

void LCD5110_write_char(unsigned char c)
{
	unsigned char line;
	unsigned char ch = 0;

	c = c - 32;

	for(line=0;line<6;line++)
	{
		ch = font6_8[c][line];
		LCD5110_LCD_write_byte(ch,1);
		
	}
}
void LCD5110_write_char_reg(unsigned char c)
{
	unsigned char line;
	unsigned char ch = 0;

	c = c - 32;

	for(line=0;line<6;line++)
	{
		ch = ~font6_8[c][line];
		LCD5110_LCD_write_byte(ch,1);
		
	}
}

void LCD5110_write_string(char *s)
{
	unsigned char ch;
  	while(*s!='\0')
	{
		ch = *s;
		LCD5110_write_char(ch);
		s++;
	}
}


void LCD5110_clear()
{
	unsigned char i,j;
	for(i=0;i<6;i++)
		for(j=0;j<84;j++)
			LCD5110_LCD_write_byte(0,1);	
}

void LCD5110_set_XY(unsigned char X,unsigned char Y)
{
	unsigned char x;
	x = 6*X;

	LCD5110_LCD_write_byte(0x40|Y,0);
	LCD5110_LCD_write_byte(0x80|x,0);
}

void LCD5110_Write_Dec(unsigned int b)
{

	unsigned char datas[3];

	datas[0] = b/1000;
	b = b - datas[0]*1000;
	datas[1] = b/100;
	b = b - datas[1]*100;
	datas[2] = b/10;
	b = b - datas[2]*10;
	datas[3] = b;

	datas[0]+=48;
	datas[1]+=48;
	datas[2]+=48;
	datas[3]+=48;

	LCD5110_write_char(datas[0]);
	LCD5110_write_char(datas[1]);
	LCD5110_write_char(datas[2]);
	LCD5110_write_char(datas[3]);

	//a++;
}

void LCD5110_LCD_delay_ms(unsigned int nCount)
{
  unsigned long t;
	t = nCount * 40000;
	while(t--);
}



void LCD5110_CS(unsigned char temp)
{
	if (temp)
//	   GPIO_SetBits(GPIOC,GPIO_Pin_9);
	   HAL_GPIO_WritePin(CE_PORT,CE_PIN, GPIO_PIN_SET);
	else
//	   GPIO_ResetBits(GPIOC,GPIO_Pin_9);
       HAL_GPIO_WritePin(CE_PORT,CE_PIN, GPIO_PIN_RESET);
}

void LCD5110_RST(unsigned char temp)
{
	if (temp)
//	(temp) GPIO_SetBits(GPIOC,GPIO_Pin_8);
       HAL_GPIO_WritePin(RST_PORT,RST_PIN, GPIO_PIN_SET);
	else
//	   GPIO_ResetBits(GPIOC,GPIO_Pin_8);
       HAL_GPIO_WritePin(RST_PORT,RST_PIN, GPIO_PIN_RESET);
}

void LCD5110_DC(unsigned char temp)
{
	if (temp)
//	(temp) GPIO_SetBits(GPIOC,GPIO_Pin_7);
       HAL_GPIO_WritePin(DC_PORT,DC_PIN, GPIO_PIN_SET);
	else
//	   GPIO_ResetBits(GPIOC,GPIO_Pin_7);
       HAL_GPIO_WritePin(DC_PORT,DC_PIN, GPIO_PIN_RESET);
}

void LCD5110_MO(unsigned char temp)
{
	if (temp)
//	(temp) GPIO_SetBits(GPIOC,GPIO_Pin_6);
       HAL_GPIO_WritePin(MOSI_PORT,MOSI_PIN, GPIO_PIN_SET);
	else
//	   GPIO_ResetBits(GPIOC,GPIO_Pin_6);
       HAL_GPIO_WritePin(MOSI_PORT,MOSI_PIN, GPIO_PIN_RESET);
}

void LCD5110_SCK(unsigned char temp)
{
	if (temp)
//	(temp) GPIO_SetBits(GPIOA,GPIO_Pin_8);
       HAL_GPIO_WritePin(CLK_PORT,CLK_PIN, GPIO_PIN_SET);
	else
//	   GPIO_ResetBits(GPIOA,GPIO_Pin_8);
       HAL_GPIO_WritePin(CLK_PORT,CLK_PIN, GPIO_PIN_RESET);
}
