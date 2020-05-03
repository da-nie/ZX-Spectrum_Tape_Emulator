//----------------------------------------------------------------------------------------------------
//подключаемые библиотеки
//----------------------------------------------------------------------------------------------------
#include "sd.h"
#include "based.h"
#include "wh1602.h"

//----------------------------------------------------------------------------------------------------
//макроопределения
//----------------------------------------------------------------------------------------------------

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//настройки SD-карты
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define SD_CS_DDR   DDRB
#define SD_CS_PORT  PORTB
#define SD_CS       4

#define SD_DI_DDR   DDRB
#define SD_DI_PORT  PORTB
#define SD_DI       5

#define SD_DO_DDR   DDRB
#define SD_DO_PORT  PORTB
#define SD_DO_PIN   PINB
#define SD_DO       6

#define SD_SCK_DDR  DDRB
#define SD_SCK_PORT PORTB
#define SD_SCK      7

//коды команд
#define CMD0  0x40
#define CMD1  (CMD0+1)
#define CMD8  (CMD0+8)
#define CMD9  (CMD0+9)
#define CMD16 (CMD0+16)
#define CMD17 (CMD0+17)
#define CMD55 (CMD0+55)
#define CMD58 (CMD0+58)

//----------------------------------------------------------------------------------------------------
//константы
//----------------------------------------------------------------------------------------------------

//размер ответа
static const uint8_t ANSWER_R1_SIZE=1;
static const uint8_t ANSWER_R3_SIZE=5;

//----------------------------------------------------------------------------------------------------
//перечисления
//----------------------------------------------------------------------------------------------------

//типы SD-карт
typedef enum 
{
 SD_TYPE_NONE=0,
 SD_TYPE_MMC_V3=1,
 SD_TYPE_SD_V1=2,
 SD_TYPE_SD_V2=3,
 SD_TYPE_SD_V2_HC=4
} SD_TYPE;

//----------------------------------------------------------------------------------------------------
//глобальные переменные
//----------------------------------------------------------------------------------------------------

extern char String[25];//строка

uint16_t BlockByteCounter=512;//считанный байт блока
SD_TYPE SDType=SD_TYPE_NONE;//тип карты памяти

static const char Text_SD_No_SPI_Up[] PROGMEM =       "Карта памяти не \0";
static const char Text_SD_No_SPI_Down[] PROGMEM =     "поддерживает SPI\0";
static const char Text_SD_No_Response[] PROGMEM =     "Карта молчит!   \0";
static const char Text_SD_Size_Error_Up[] PROGMEM =   "Объем SD-карты  \0";
static const char Text_SD_Size_Error_Down[] PROGMEM = "не определен!   \0";
static const char Text_SD_Size[] PROGMEM =            "Объем SD-карты  \0";

//----------------------------------------------------------------------------------------------------
//прототипы функций
//----------------------------------------------------------------------------------------------------
static inline uint8_t SD_TransmitData(uint8_t data);//послать данные SD-карте и принять ответ
uint16_t inline GetBits(uint8_t *data,uint8_t begin,uint8_t end);//получить биты с begin по end включительно


//----------------------------------------------------------------------------------------------------
//получить биты с begin по end включительно
//----------------------------------------------------------------------------------------------------
uint16_t GetBits(uint8_t *data,uint8_t begin,uint8_t end)
{
 uint16_t bits=0;
 uint8_t size=1+begin-end; 
 for(uint8_t i=0;i<size;i++) 
 {
  uint8_t position=end+i;
  uint16_t byte=15-(position>>3);
  uint16_t bit=position&0x7;
  uint16_t value=(data[byte]>>bit)&1;
  bits|=value<<i;
 }
 return(bits);
}
//----------------------------------------------------------------------------------------------------
//послать данные SD-карте и принять ответ
//----------------------------------------------------------------------------------------------------
static inline uint8_t SD_TransmitData(uint8_t data)
{ 
 uint8_t response=0;
 for (uint8_t i=0;i<8;i++,data=data<<1)
 {
  response=response<<1;
  if (data&128) SD_DI_PORT|=(1<<SD_DI);
           else SD_DI_PORT&=0xff^(1<<SD_DI);
  SD_SCK_PORT|=(1<<SD_SCK);
  asm volatile ("nop"::);
  asm volatile ("nop"::);
  if (SD_DO_PIN&(1<<SD_DO)) response|=1;
  asm volatile ("nop"::);
  asm volatile ("nop"::);
  SD_SCK_PORT&=0xff^(1<<SD_SCK);
  asm volatile ("nop"::);
  asm volatile ("nop"::);
  asm volatile ("nop"::);
  asm volatile ("nop"::);
 }
 return(response);
/*

 SPDR=data;//передаём
 while(!(SPSR&(1<<SPIF)));//ждём завершения передачи и получения ответа
 uint8_t res=SPDR;
 return(res);
 */
}


//----------------------------------------------------------------------------------------------------
//инициализация карты памяти
//----------------------------------------------------------------------------------------------------
void SD_Init(void)
{
 WH1602_SetTextUpLine("");
 WH1602_SetTextDownLine("");
 SD_CS_DDR|=(1<<SD_CS);
 SD_DI_DDR|=(1<<SD_DI);
 SD_SCK_DDR|=(1<<SD_SCK);
 SD_DO_DDR&=0xff^(1<<SD_DO);
 //вывод SPI SS в режиме MASTER сконфигурирован как выход и на SPI не влияет
 _delay_ms(1000);//пауза, пока карта не включится
 uint8_t n;
 //шлём не менее 74 импульсов синхронизации при высоком уровне на CS и DI 
 SD_CS_PORT|=(1<<SD_CS);
 _delay_ms(500);
 SD_DI_PORT|=(1<<SD_DI);
 for(n=0;n<250;n++)
 {
  SD_SCK_PORT|=(1<<SD_SCK);
  _delay_ms(1);
  SD_SCK_PORT&=0xff^(1<<SD_SCK);
  _delay_ms(1);
 }
 SD_CS_PORT&=0xff^(1<<SD_CS);

 /*
 //настраиваем SP
 SPCR=(0<<SPIE)|(1<<SPE)|(0<<DORD)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(0<<SPR1)|(0<<SPR0);
 SPSR=(1<<SPI2X);//удвоенная скорость SPI
 
 */
 
 _delay_ms(100);
 
 uint8_t answer[ANSWER_R3_SIZE];//ответ от карты
 bool res;
 //шлём CMD0
 res=SD_SendCommand(CMD0,0x00,0x00,0x00,0x00,ANSWER_R1_SIZE,answer);
 if (res==false || answer[0]!=1)//ошибка
 {
  WH1602_SetTextProgmemUpLine(Text_SD_No_SPI_Up);
  WH1602_SetTextProgmemDownLine(Text_SD_No_SPI_Down);
  while(1);
  return;
 }

 //определяем тип карты
 SDType=SD_TYPE_NONE;
 //шлём CMD8 (ответ будет R3 в режиме SPI)
 res=SD_SendCommand(CMD8,0xAA,0x01,0x00,0x00,ANSWER_R3_SIZE,answer);
 if (res==true && answer[0]==0x01)
 {  
  if (!((answer[ANSWER_R3_SIZE-2]&0x0F)==0x01 && answer[ANSWER_R3_SIZE-1]==0xAA))
  {
   WH1602_SetTextProgmemUpLine(Text_SD_No_Response);
   while(1);
   return; 
  }
  //шлём ACMD41
  for(n=0;n<65535;n++)
  {
   //шлём ACMD41
   res=SD_SendCommand(CMD55,0x00,0x00,0x00,0x00,ANSWER_R1_SIZE,answer);
   if (res==false || (answer[0]!=0x00 && answer[0]!=0x01))
   {
    WH1602_SetTextProgmemUpLine(Text_SD_No_Response);
    while(1);
    return; 
   }
   res=SD_SendCommand(CMD1,0x00,0x00,0x00,0x40,ANSWER_R1_SIZE,answer);
   if (res==true && answer[0]==0x00) break;
  }
  if (n==65535)
  {
   WH1602_SetTextProgmemUpLine(Text_SD_No_Response);
   while(1);
   return; 
  }
  //шлём CMD58
  res=SD_SendCommand(CMD58,0x00,0x00,0x00,0x00,ANSWER_R3_SIZE,answer);
  if (res==false)
  {
   WH1602_SetTextProgmemUpLine(Text_SD_No_Response);
   while(1);
   return; 
  }
  if (answer[1]&0x40) SDType=SD_TYPE_SD_V2_HC;
                 else SDType=SD_TYPE_SD_V2;  
 } 
 else//карта не ответила на CMD8
 {
  //диаграмма рекомендует слать ACMD41, но совсем старая карта на 16 МБ отказалась с ней работать, при этом ошибку она не формирует.
  //будем такие карты инициализировать только через CMD1, а не через ACMD41
  //шлём ACMD41
  for(n=0;n<65535;n++)
  {   
   /*
   //шлём ACMD41, если только это не MMC V3
   if (SDType!=SD_TYPE_MMC_V3)
   {
	res=SD_SendCommand(CMD55,0x00,0x00,0x00,0x00,ANSWER_R1_SIZE,answer);
    if (res==false || (answer[0]!=0x00 && answer[0]!=0x01)) SDType=SD_TYPE_MMC_V3;//карта не знает ACM41, так что теперь шлём только CMD1
   }
   */
   res=SD_SendCommand(CMD1,0x00,0x00,0x00,0x00,ANSWER_R1_SIZE,answer);
   if (res==true && answer[0]==0x00) break;
  }
  if (n==65535)
  {
   WH1602_SetTextProgmemUpLine(Text_SD_No_Response);
   while(1);
  }
  if (SDType!=SD_TYPE_MMC_V3) SDType=SD_TYPE_SD_V1;
 }
 //задаём размер блока 512 байт
 res=SD_SendCommand(CMD16,0x00,0x00,0x02,0x00,ANSWER_R1_SIZE,answer);

 //инициализация пройдёна успешно
 if (SDType!=SD_TYPE_SD_V2_HC)//узнаем объём карты памяти
 {
  //узнаем объём карты памяти
  uint32_t SD_Size=0;
  if (SD_GetSize(&SD_Size)==false)//ошибка
  {
   WH1602_SetTextProgmemUpLine(Text_SD_Size_Error_Up);
   WH1602_SetTextProgmemDownLine(Text_SD_Size_Error_Down);
   while(1);
   return;
  }
  uint16_t size=(uint16_t)(SD_Size>>20);
  sprintf(String,"%i МБ",size);
  WH1602_SetTextProgmemUpLine(Text_SD_Size);
  WH1602_SetTextDownLine(String);
  _delay_ms(1000);
 }
 for(uint16_t m=0;m<1024;m++) SD_TransmitData(0xff);
}

//----------------------------------------------------------------------------------------------------
//послать команду и получить ответ
//----------------------------------------------------------------------------------------------------
bool SD_SendCommand(uint8_t cmd,uint8_t b0,uint8_t b1,uint8_t b2,uint8_t b3,uint8_t answer_size,uint8_t *answer)
{ 
 //отправляем команду и считаем её CRC7
 uint8_t crc7=0; 
 uint8_t cmd_buf[5]={cmd,b3,b2,b1,b0};
 uint16_t n;
 for(n=0;n<5;n++)
 {
  SD_TransmitData(cmd_buf[n]);
  
  uint8_t b=cmd_buf[n]; 
  for (uint8_t i=0;i<8;i++) 
  { 
   crc7<<=1; 
   if ((b&0x80)^(crc7&0x80)) crc7^=0x09; 
   b<<=1; 
  }
 }
 crc7=crc7<<1;
 crc7|=1; 
 SD_TransmitData(crc7);//CRC
 //карта может ответить не сразу
 //принимаем ответ R1 (старший бит всегда 0)
 for(n=0;n<65535;n++)
 {
  uint8_t res=SD_TransmitData(0xff);
  if ((res&128)==0)
  {  
   answer[0]=res; 
   break;
  }
  _delay_us(10);
 }
 if (n==65535) return(false);
 for(n=1;n<answer_size;n++)
 {
  answer[n]=SD_TransmitData(0xff);
 }
 SD_TransmitData(0xff);
 return(true);//ответ принят
}
//----------------------------------------------------------------------------------------------------
//получить объём SD-карты в байтах
//----------------------------------------------------------------------------------------------------
bool SD_GetSize(uint32_t *size)
{
 uint16_t n;
 uint8_t answer[ANSWER_R1_SIZE];
 if (SD_SendCommand(CMD9,0x00,0x00,0x00,0x00,ANSWER_R1_SIZE,answer)==false) return(false);//ответ не принят
 //считываем 16 байт данных ответа R1
 uint8_t byte=0;
 for(n=0;n<65535;n++)
 {
  byte=SD_TransmitData(0xff);
  if (byte!=0xff) break;
 }
 if (n==65535) return(false);//ответ не принят
 uint8_t b[16];
 n=0;
 if (byte!=0xfe)//принятый байт не был байтом признака начала данных
 {
  b[0]=byte;
  n++;
 }
 for(;n<16;n++) b[n]=SD_TransmitData(0xff);
 //смотрим размер карты памяти
 uint32_t blocks=0;
 if (SDType==SD_TYPE_SD_V2_HC)//регистр CSD изменён
 {
  blocks=GetBits(b,69,48);
  uint32_t read_bl_len=GetBits(b,83,80);
  uint32_t block_size=(1UL<<read_bl_len);
  //пока мне неизвестно, как из этих цифр получить объём карты памяти 

 
 }
 else//обычная SD-карта
 {
  //вычисляем, согласно SDCardManual
  uint32_t read_bl_len=GetBits(b,83,80);
  uint32_t c_size=GetBits(b,73,62);
  uint32_t c_size_mult=GetBits(b,49,47);
  blocks=(c_size+1UL)*(1UL<<(c_size_mult+2UL));
  blocks*=(1UL<<read_bl_len);
 }  
 *size=blocks;
 return(true);
}
//----------------------------------------------------------------------------------------------------
//начать чтение блока
//----------------------------------------------------------------------------------------------------
bool SD_BeginReadBlock(uint32_t BlockAddr)
{
 if (SDType!=SD_TYPE_SD_V2_HC) BlockAddr<<=9;//умножаем на 512 для старых карт памяти
 //даём команду чтения блока
 uint8_t a1=(uint8_t)((BlockAddr>>24)&0xff);
 uint8_t a2=(uint8_t)((BlockAddr>>16)&0xff);
 uint8_t a3=(uint8_t)((BlockAddr>>8)&0xff);
 uint8_t a4=(uint8_t)(BlockAddr&0xff);
 uint8_t answer[ANSWER_R1_SIZE];
 bool ret=SD_SendCommand(CMD17,a4,a3,a2,a1,ANSWER_R1_SIZE,answer);//посылаем CMD17
 if (ret==false || answer[0]!=0) return(false);//ошибка команды
 SD_TransmitData(0xff);//байтовый промежуток
 //ждём начало поступления данных
 uint16_t n;
 for(n=0;n<65535;n++)
 {
  uint8_t res=SD_TransmitData(0xff);
  if (res==0xfe) break;//маркер получен
  _delay_us(10);
 }
 if (n==65535) return(false);//маркер начала данных не получен
 BlockByteCounter=0;
 return(true);
}
//----------------------------------------------------------------------------------------------------
//считать байт блока
//----------------------------------------------------------------------------------------------------
bool SD_ReadBlockByte(uint8_t *byte)
{
 if (BlockByteCounter>=512) return(false);
 *byte=SD_TransmitData(0xff);//читаем байт с SD-карты
 BlockByteCounter++;
 if (BlockByteCounter==512)
 {
  //считываем CRC
  SD_TransmitData(0xff);
  SD_TransmitData(0xff); 
 }
 return(true);
}
//----------------------------------------------------------------------------------------------------
//считать блок в 256 байт в память
//----------------------------------------------------------------------------------------------------
bool SD_ReadBlock(uint32_t BlockAddr,uint8_t *Addr,bool first)
{
 if (SDType!=SD_TYPE_SD_V2_HC) BlockAddr<<=9;//умножаем на 512 для старых карт памяти
 //даём команду чтения блока
 uint8_t a1=(uint8_t)((BlockAddr>>24)&0xff);
 uint8_t a2=(uint8_t)((BlockAddr>>16)&0xff);
 uint8_t a3=(uint8_t)((BlockAddr>>8)&0xff);
 uint8_t a4=(uint8_t)(BlockAddr&0xff);
 uint8_t answer[ANSWER_R1_SIZE];
 bool ret=SD_SendCommand(CMD17,a4,a3,a2,a1,ANSWER_R1_SIZE,answer);//посылаем CMD17
 if (ret==false || answer[0]!=0) return(false);//ошибка команды
 SD_TransmitData(0xff);//байтовый промежуток
 //ждём начало поступления данных
 uint16_t n;
 for(n=0;n<65535;n++)
 {
  uint8_t res=SD_TransmitData(0xff);
  if (res==0xfe) break;//маркер получен
  _delay_us(10);
 }
 if (n==65535) return(false);//маркер начала данных не получен
 for(n=0;n<512;n++)
 {
  uint8_t b=SD_TransmitData(0xff);//читаем байт с SD-карты
  if (first==true && n<256) 
  {
   *Addr=b;
   Addr++;
  }
  if (first==false && n>=256) 
  {
   *Addr=b;
   Addr++;
  }
 }
 //считываем CRC
 SD_TransmitData(0xff);
 SD_TransmitData(0xff); 
 return(true);
}
