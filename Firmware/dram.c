//----------------------------------------------------------------------------------------------------
//подключаемые библиотеки
//----------------------------------------------------------------------------------------------------
#include "dram.h"
#include "based.h"

//----------------------------------------------------------------------------------------------------
//макроопределени€
//----------------------------------------------------------------------------------------------------

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//настройки динамической пам€ти
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define DRAM_RAS_PORT PORTD
#define DRAM_RAS_DDR  DDRD
#define DRAM_RAS      5

#define DRAM_CAS_PORT PORTD
#define DRAM_CAS_DDR  DDRD
#define DRAM_CAS      7

#define DRAM_WE_PORT  PORTD
#define DRAM_WE_DDR   DDRD
#define DRAM_WE       6

#define DRAM_OE_PORT  PORTC
#define DRAM_OE_DDR   DDRC
#define DRAM_OE       0

#define DRAM_A0_A7_PORT  PORTA
#define DRAM_A0_A7_DDR   DDRA

#define DRAM_A8_PORT  PORTC
#define DRAM_A8_DDR   DDRC
#define DRAM_A8       1

#define DRAM_D1_D4_PORT PORTB
#define DRAM_D1_D4_PIN  PINB
#define DRAM_D1_D4_DDR  DDRB
#define DRAM_D1_D4_MASK ((1<<0)|(1<<1)|(1<<2)|(1<<3))      

//----------------------------------------------------------------------------------------------------
//инициализаци€ диспле€
//----------------------------------------------------------------------------------------------------
void DRAM_Init(void)
{
 //настроим порты 
 DRAM_RAS_DDR|=(1<<DRAM_RAS);
 DRAM_CAS_DDR|=(1<<DRAM_CAS);
 DRAM_WE_DDR|=(1<<DRAM_WE);
 DRAM_OE_DDR|=(1<<DRAM_OE);
 
 DRAM_A0_A7_DDR=0xff;
 DRAM_A8_DDR|=(1<<DRAM_A8); 
  
 DRAM_D1_D4_DDR&=0xff^(DRAM_D1_D4_MASK);
 //переводим все сигналы управлени€ пам€тью в неактивное состо€ние
 DRAM_RAS_PORT|=(1<<DRAM_RAS);
 DRAM_CAS_PORT|=(1<<DRAM_CAS);
 DRAM_WE_PORT|=(1<<DRAM_WE);
 DRAM_OE_PORT|=(1<<DRAM_OE);
}
//----------------------------------------------------------------------------------------------------
//произвести цикл скрытой регенерации
//----------------------------------------------------------------------------------------------------
void DRAM_Refresh(void)
{
 //даЄм сигнал регенерации
 DRAM_CAS_PORT&=0xff^(1<<DRAM_CAS);
 asm volatile ("nop"::);
 asm volatile ("nop"::);
 DRAM_RAS_PORT&=0xff^(1<<DRAM_RAS);
 asm volatile ("nop"::);
 asm volatile ("nop"::);
 asm volatile ("nop"::);
 asm volatile ("nop"::);
 DRAM_CAS_PORT|=(1<<DRAM_CAS);
 asm volatile ("nop"::);
 asm volatile ("nop"::);
 DRAM_RAS_PORT|=(1<<DRAM_RAS);
}
//----------------------------------------------------------------------------------------------------
//считать ниббл
//----------------------------------------------------------------------------------------------------
uint8_t DRAM_ReadNibble(uint32_t addr,bool nibble_one)
{
 //шина данных - на чтение
 DRAM_D1_D4_DDR&=0xff^(DRAM_D1_D4_MASK);
 DRAM_OE_PORT|=1<<DRAM_OE;
 //выставл€ем младшую часть адреса 
 DRAM_A0_A7_PORT=(addr&0xff);
 DRAM_A8_PORT&=0xff^(1<<DRAM_A8);
 DRAM_A8_PORT|=(((addr>>16)&0x01)<<DRAM_A8);
 //даЄм сигнал RAS
 DRAM_RAS_PORT&=0xff^(1<<DRAM_RAS);
 //выставл€ем старшую часть адреса
 DRAM_A0_A7_PORT=(addr>>8)&0xff;
 DRAM_A8_PORT&=0xff^(1<<DRAM_A8);
 if (nibble_one==false) DRAM_A8_PORT|=1<<DRAM_A8;
 //даЄм сигнал CAS
 DRAM_CAS_PORT&=0xff^(1<<DRAM_CAS); 
 asm volatile ("nop"::);
 asm volatile ("nop"::);
 //считываем данные
 DRAM_OE_PORT&=0xff^(1<<DRAM_OE);
 asm volatile ("nop"::);
 asm volatile ("nop"::);
 
 uint8_t byte=(DRAM_D1_D4_PIN&0x0f);
 DRAM_OE_PORT|=1<<DRAM_OE;
 //снимаем сигнал CAS
 DRAM_CAS_PORT|=(1<<DRAM_CAS); 
 //снимаем сигнал CAS
 DRAM_CAS_PORT|=(1<<DRAM_CAS);
 //снимаем сигнал RAS
 DRAM_RAS_PORT|=(1<<DRAM_RAS);
 return(byte);
}
//----------------------------------------------------------------------------------------------------
//записать ниббл
//----------------------------------------------------------------------------------------------------
void DRAM_WriteNibble(uint32_t addr,uint8_t nibble,bool nibble_one)
{
 DRAM_OE_PORT|=1<<DRAM_OE;
 //шина данных - на запись
 DRAM_D1_D4_DDR|=DRAM_D1_D4_MASK;
 //выставл€ем младшую часть адреса
 DRAM_A0_A7_PORT=(addr&0xff); 
 DRAM_A8_PORT&=0xff^(1<<DRAM_A8);
 DRAM_A8_PORT|=(((addr>>16)&0x01)<<DRAM_A8);
 //даЄм сигнал RAS
 DRAM_RAS_PORT&=0xff^(1<<DRAM_RAS);
 asm volatile ("nop"::);
 //включаем сигнал записи
 DRAM_WE_PORT&=0xff^(1<<DRAM_WE);	 
 asm volatile ("nop"::);
 //выставл€ем старшую часть адреса
 DRAM_A0_A7_PORT=(addr>>8)&0xff; 
 DRAM_A8_PORT&=0xff^(1<<DRAM_A8);
 if (nibble_one==false) DRAM_A8_PORT|=1<<DRAM_A8;
 //задаЄм данные
 DRAM_D1_D4_PORT&=0xff^(DRAM_D1_D4_MASK);
 DRAM_D1_D4_PORT|=nibble&0x0f;
 //даЄм сигнал CAS
 DRAM_CAS_PORT&=0xff^(1<<DRAM_CAS);
 asm volatile ("nop"::);
 asm volatile ("nop"::);
 //снимаем сигнал записи
 DRAM_WE_PORT|=(1<<DRAM_WE);
 //снимаем сигнал CAS
 DRAM_CAS_PORT|=(1<<DRAM_CAS); 
 //снимаем сигнал RAS
 DRAM_RAS_PORT|=(1<<DRAM_RAS);
}

//----------------------------------------------------------------------------------------------------
//считать байт
//----------------------------------------------------------------------------------------------------
uint8_t DRAM_ReadByte(uint32_t addr)
{
 uint8_t byte=DRAM_ReadNibble(addr,false);
 byte<<=4;
 byte|=DRAM_ReadNibble(addr,true);
 return(byte);
}
//----------------------------------------------------------------------------------------------------
//записать байт
//----------------------------------------------------------------------------------------------------
void DRAM_WriteByte(uint32_t addr,uint8_t byte)
{
 DRAM_WriteNibble(addr,byte>>4,false);
 DRAM_WriteNibble(addr,byte&0x0f,true);
}

