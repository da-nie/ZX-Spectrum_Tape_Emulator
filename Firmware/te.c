//----------------------------------------------------------------------------------------------------
//подключаемые библиотеки
//----------------------------------------------------------------------------------------------------
#include "based.h"
#include "dram.h"
#include "wh1602.h"
#include "sd.h"
#include "fat.h"

//----------------------------------------------------------------------------------------------------
//константы
//----------------------------------------------------------------------------------------------------

static const char Text_Main_Menu_Select[] PROGMEM =       "    Выберите    \0";
static const char Text_Main_Memory_Test[] PROGMEM =       "   Тест памяти  \0";
static const char Text_Main_Memory_Test_Error[] PROGMEM = " Ошибка памяти !\0";
static const char Text_Main_Memory_Test_OK[] PROGMEM =    "Память исправна \0";
static const char Text_Tape_Menu_No_Image[] PROGMEM =     "Нет файлов tap !\0";

//----------------------------------------------------------------------------------------------------
//глобальные переменные
//----------------------------------------------------------------------------------------------------

extern char String[25];//строка

static uint16_t BlockSize=0;//размер блока данных в памяти
static volatile uint16_t DataCounter=0;//колиество выданных байт данных
static volatile short LeadToneCounter=0;//время выдачи пилот-тона
static volatile uint8_t TapeOutMode=0;//режим вывода
static bool TapeOutVolume=false;//выдаваемый сигнал
static volatile uint8_t Speed;//скорость работы

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//макроопределения
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static const uint8_t TAPE_OUT_LEAD=0;
static const uint8_t TAPE_OUT_SYNCHRO_1=1;
static const uint8_t TAPE_OUT_SYNCHRO_2=2;
static const uint8_t TAPE_OUT_DATA=3;
static const uint8_t TAPE_OUT_STOP=4;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//настройки кнопок
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define BUTTON_UP_DDR      DDRD
#define BUTTON_UP_PORT     PORTD
#define BUTTON_UP_PIN      PIND
#define BUTTON_UP          3

#define BUTTON_CENTER_DDR  DDRD
#define BUTTON_CENTER_PORT PORTD
#define BUTTON_CENTER_PIN  PIND
#define BUTTON_CENTER      2

#define BUTTON_DOWN_DDR    DDRD
#define BUTTON_DOWN_PORT   PORTD
#define BUTTON_DOWN_PIN    PIND
#define BUTTON_DOWN        1

#define BUTTON_SELECT_DDR  DDRD
#define BUTTON_SELECT_PORT PORTD
#define BUTTON_SELECT_PIN  PIND
#define BUTTON_SELECT      4

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//линии магнитофона
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#define TAPE_OUT_DDR  DDRD
#define TAPE_OUT_PORT PORTD
#define TAPE_OUT_PIN  PIND
#define TAPE_OUT      0

//----------------------------------------------------------------------------------------------------
//прототипы функций
//----------------------------------------------------------------------------------------------------
void TapeMenu(void);//меню магнитофона
void MemoryTest(void);//тест памяти
void OutputImage(void);//запуск образа
void WaitAnyKey(void);//ожидание любой клавиши
void InitAVR(void);//инициализация контроллера

//----------------------------------------------------------------------------------------------------
//основная функция программы
//----------------------------------------------------------------------------------------------------
int main(void)
{ 
 InitAVR();
 DRAM_Init();
 WH1602_Init(); 
 SD_Init();
 FAT_Init(); 
 
 //запускаем основное меню
 uint8_t select_item=0;
 while(1)
 {  
  WH1602_SetTextProgmemUpLine(Text_Main_Menu_Select);
  if (select_item==0) strcpy(String,"> Магнитофон x1 <");
  if (select_item==1) strcpy(String,"> Магнитофон x2 <");
  if (select_item==2) strcpy(String,"> Магнитофон x4 <"); 
  if (select_item==3) strcpy(String,">  Тест памяти  <");
  WH1602_SetTextDownLine(String);
  _delay_ms(500);
  //ждём нажатий кнопок
  while(1)
  {
   if (BUTTON_UP_PIN&(1<<BUTTON_UP))
   {
    if (select_item==0) select_item=3;
                   else select_item--;
    break;
   }
   if (BUTTON_DOWN_PIN&(1<<BUTTON_DOWN))
   {
    if (select_item==3) select_item=0;
                   else select_item++;       
    break;
   }
   if (BUTTON_SELECT_PIN&(1<<BUTTON_SELECT))
   {
    if (select_item==0) 
	{
	 Speed=0;
	 TapeMenu();
	}
    if (select_item==1) 
	{
	 Speed=1;
	 TapeMenu();
	}
    if (select_item==2)
	{
	 Speed=2;
     TapeMenu();
	}
    if (select_item==3) MemoryTest();
    break;
   }
  }  
 } 
 return(0);
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//общие функции
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++




//----------------------------------------------------------------------------------------------------
//меню магнитофона
//----------------------------------------------------------------------------------------------------
void TapeMenu(void)
{
 uint8_t n;
 //переходим к первому имени файла на карте
 if (FAT_BeginFileSearch()==false)
 {
  WH1602_SetTextProgmemUpLine(Text_Tape_Menu_No_Image); 
  WH1602_SetTextDownLine(""); 
  _delay_ms(2000);
  return;//нет ни одного файла
 }
 bool directory;//это директория
 uint32_t size;//размер файла
 uint16_t index=1;//номер файла
 uint16_t level_index[20];//20 уровней вложенности
 uint8_t level=0;
 level_index[0]=index;
 while(1)
 { 
  //выводим данные с SD-карты
  //читаем имя файла 
  if (FAT_GetFileSearch(String,&size,&directory)==true) WH1602_SetTextDownLine(String);
  if (Directory==false) sprintf(String,"[%02u:%05u] Файл",level,index);
                   else sprintf(String,"[%02u:%05u] Папка",level,index);
  WH1602_SetTextUpLine(String);  
  _delay_ms(200);
  //ждём нажатий кнопок
  while(1)
  {
   if (BUTTON_UP_PIN&(1<<BUTTON_UP))
   {
    uint8_t i=1;
	if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER)) i=10;
	for(n=0;n<i;n++)
	{
	 if (FAT_PrevFileSearch()==true) index--;
	                             else break;
	}
    break;
   }
   if (BUTTON_DOWN_PIN&(1<<BUTTON_DOWN))
   {
    uint8_t i=1;
	if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER)) i=10;
	for(n=0;n<i;n++)
	{
     if (FAT_NextFileSearch()==true) index++;
	                             else break;
	}
    break;
   }   
   if (BUTTON_SELECT_PIN&(1<<BUTTON_SELECT))
   {
    if (Directory==0) OutputImage();//для файла - запускаем на выполнение
    else
	{
	 if (level<20) level_index[level]=index;//запоминаем достигнутый уровень
     if (Directory<0)//если мы вышли на уровень вверх
	 {
	  if (level>0) level--;
	 }
	 else
	 {
	  level++;
      if (level<20) level_index[level]=1;
	 }
     FAT_EnterDirectory(FirstCluster);//заходим в директорию	
	 //проматываем до выбранного файла
	 index=1;
	 if (level<20)
	 {
	  for(uint16_t s=1;s<level_index[level];s++)
	  {
       if (FAT_NextFileSearch()==true) index++;
                                   else break; 
	  }
	 }
	}
    break;
   }
  }
 } 
}
//----------------------------------------------------------------------------------------------------
//тест памяти
//----------------------------------------------------------------------------------------------------
void MemoryTest(void)
{ 
 uint8_t last_p=0xff;
 WH1602_SetTextProgmemUpLine(Text_Main_Memory_Test);  
 for(uint16_t b=0;b<=255;b++)
 {   
  uint8_t progress=(uint8_t)(100UL*(int32_t)b/255UL);
  if (progress!=last_p)
  {
   sprintf(String,"Выполнено:%i %%",progress);
   WH1602_SetTextDownLine(String); 
   last_p=progress;
  } 
  //записываем в ОЗУ значения 
  for(uint32_t addr=0;addr<131072UL;addr++)
  {   
   uint8_t byte=(b+addr)&0xff;
   DRAM_WriteByte(addr,byte);   
   DRAM_Refresh();
  }
  //проверяем, что записанное в ОЗУ совпадает со считанным
  for(uint32_t addr=0;addr<131072UL;addr++)
  {   
   uint8_t byte=(b+addr)&0xff;
   uint8_t byte_r=DRAM_ReadByte(addr);
   DRAM_Refresh();
   if (byte!=byte_r)
   {
    WH1602_SetTextProgmemUpLine(Text_Main_Memory_Test_Error);
    sprintf(String,"%05x = [%02x , %02x]",(unsigned int)addr,byte,byte_r);
    WH1602_SetTextDownLine(String);
	_delay_ms(5000);
    return;
   }
  }
 }
 
 /*
 
 uint8_t last_p=0xff;
 for(uint32_t addr=0;addr<131072UL;addr++)
 {
  uint8_t progress=(uint8_t)(100UL*addr/131071UL);
  if (progress!=last_p)
  {
   sprintf(String,"Выполнено:%i %%",progress);
   WH1602_SetTextDownLine(String); 
   last_p=progress;
  }  
  uint8_t byte=0x01;
  for(uint8_t n=0;n<8;n++,byte<<=1)
  {
   DRAM_WriteByte(addr,byte);
   DRAM_Refresh();
   uint8_t byte_r=DRAM_ReadByte(addr);
   if (byte!=byte_r)
   {
    WH1602_SetTextProgmemUpLine(Text_Main_Memory_Test_Error);
    sprintf(String,"%05x = [%02x , %02x]",(unsigned int)addr,byte,byte_r);
    WH1602_SetTextDownLine(String);
	_delay_ms(5000);
    return;
   }
  }
 }*/
 WH1602_SetTextProgmemUpLine(Text_Main_Memory_Test_OK);
 WH1602_SetTextDownLine("");
 _delay_ms(3000);
}
//----------------------------------------------------------------------------------------------------
//запуск образа
//----------------------------------------------------------------------------------------------------
void OutputImage(void)
{
 _delay_ms(500);
 //повторяем для каждого блока tap-файла
 uint16_t block=0;
 while(1)
 {  
  if (FATWriteBlock(&BlockSize,block)==false) break;//блоки файла закончились 
  //выводим номер блока файла
  sprintf(String,"Блок:%u [%u]",block+1,BlockSize);
  WH1602_SetTextUpLine(String);  
  //запускаем таймер и регенерируем память    
  TCNT0=0;//начальное значение таймера
  LeadToneCounter=6000<<Speed;
  TapeOutMode=TAPE_OUT_LEAD;
  TapeOutVolume=false;    
  DataCounter=0;
  uint16_t dl=0;
  sei();  
  while(1)
  {
   cli();
   DRAM_Refresh();
   if (TapeOutMode==TAPE_OUT_STOP) 
   {    
    sprintf(String,"Блок:%u [0]",block+1);
    WH1602_SetTextUpLine(String);
    uint16_t new_block=block+1;
    //формируем паузу
    int delay=200;
    if (BlockSize>0x13) delay=500;//передавался файл
    for(uint16_t n=0;n<delay;n++)
    {
     _delay_ms(10);
     if (BUTTON_SELECT_PIN&(1<<BUTTON_SELECT))//выход
     {    
	  TAPE_OUT_PORT&=0xff^(1<<TAPE_OUT);
	  return;
     }   
     if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER))//пауза
     {
	  _delay_ms(200);
	  while(1)
	  {
	   if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER)) break;
	  }
	  _delay_ms(200);
     }
     if (BUTTON_UP_PIN&(1<<BUTTON_UP))//на блок вперёд
     {
      _delay_ms(200);
      new_block=block+1;
      break;
     }
     if (BUTTON_DOWN_PIN&(1<<BUTTON_DOWN))//на блок назад
     {
      _delay_ms(200);
      if (block>0) new_block=block-1;
      break;
     }
    }
	block=new_block;
    break;   
   }
   uint16_t dc=BlockSize-DataCounter;
   uint16_t tm=TapeOutMode;
   sei();
   if (tm==TAPE_OUT_DATA)
   {       
    if (dl==30000)
	{
     sprintf(String,"Блок:%u [%u]",block+1,dc);
     WH1602_SetTextUpLine(String);
	 dl=0;
	}
	else dl++;
   }
   _delay_us(10);
   if (BUTTON_SELECT_PIN&(1<<BUTTON_SELECT))//выход
   {
    cli();
	TAPE_OUT_PORT&=0xff^(1<<TAPE_OUT);
	return;
   }   
   if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER))//пауза
   {
	cli();
	_delay_ms(200);
	while(1)
	{
	 if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER)) break;
	}
	sei();
	_delay_ms(200);
   }
   
   if (BUTTON_UP_PIN&(1<<BUTTON_UP))//на блок вперёд
   {
    _delay_ms(200);
    block++;
    break;
   }
   if (BUTTON_DOWN_PIN&(1<<BUTTON_DOWN))//на блок назад
   {
    _delay_ms(200);
    if (block>0) block--;
    break;
   }
  }
  cli();
 }
 TAPE_OUT_PORT&=0xff^(1<<TAPE_OUT);
}
//----------------------------------------------------------------------------------------------------
//ожидание любой клавиши
//----------------------------------------------------------------------------------------------------
void WaitAnyKey(void)
{
 _delay_ms(200);
 while(1)
 {
  if (BUTTON_UP_PIN&(1<<BUTTON_UP)) break;
  if (BUTTON_DOWN_PIN&(1<<BUTTON_DOWN)) break;
  if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER)) break;    
  if (BUTTON_SELECT_PIN&(1<<BUTTON_SELECT)) break;
 }
}
//----------------------------------------------------------------------------------------------------
//инициализация контроллера
//----------------------------------------------------------------------------------------------------
void InitAVR(void)
{
 cli();
 
 //настраиваем порты
 DDRA=0;
 DDRB=0;
 DDRD=0; 
 DDRC=0;
 
 BUTTON_UP_DDR&=0xff^(1<<BUTTON_UP);
 BUTTON_DOWN_DDR&=0xff^(1<<BUTTON_DOWN);
 BUTTON_CENTER_DDR&=0xff^(1<<BUTTON_CENTER);
 BUTTON_SELECT_DDR&=0xff^(1<<BUTTON_SELECT);

 TAPE_OUT_DDR|=(1<<TAPE_OUT);
 
 //задаём состояние портов
 PORTA=0xff;
 PORTB=0xff;
 PORTD=0xff;
 PORTC=0xff;
 
 //настраиваем таймер T0
 TCCR0=((0<<CS02)|(1<<CS01)|(1<<CS00));//выбран режим деления тактовых импульсов на 64
 TCNT0=0;//начальное значение таймера
 TIMSK=(1<<TOIE0);//прерывание по переполнению таймера (таймер T0 восьмибитный и считает на увеличение до 0xff)
 
 TAPE_OUT_PORT&=0xff^(1<<TAPE_OUT);
}
//----------------------------------------------------------------------------------------------------
//обработчик вектора прерывания таймера T0 (8-ми разрядный таймер) по переполнению
//----------------------------------------------------------------------------------------------------
ISR(TIMER0_OVF_vect)
{ 
 static uint8_t byte=0;//выдаваемый байт
 static uint8_t index=0;//номер выдаваемого бита
 static uint16_t addr=0;//текущий адрес
 TCNT0=0;
 if (TapeOutMode==TAPE_OUT_STOP)
 {
  TAPE_OUT_PORT&=0xff^(1<<TAPE_OUT);
  return;
 }
 if (TapeOutVolume==true)
 {
  TAPE_OUT_PORT|=1<<TAPE_OUT;
  TapeOutVolume=false;
 }
 else
 {
  TAPE_OUT_PORT&=0xff^(1<<TAPE_OUT);  
  TapeOutVolume=true;
 }
 //выводим пилот-тон
 if (TapeOutMode==TAPE_OUT_LEAD)
 {
  TCNT0=255-(142>>Speed);//начальное значение таймера
  if (LeadToneCounter>0) LeadToneCounter--;
  else
  {
   TapeOutMode=TAPE_OUT_SYNCHRO_1;
   return;
  }
 }
 //выводим синхросигнал 1
 if (TapeOutMode==TAPE_OUT_SYNCHRO_1)
 {
  TCNT0=255-(43>>Speed);//начальное значение таймера
  TapeOutMode=TAPE_OUT_SYNCHRO_2;
  return;
 }
 //выводим синхросигнал 2
 if (TapeOutMode==TAPE_OUT_SYNCHRO_2)
 {
  TCNT0=255-(48>>Speed);//начальное значение таймера
  TapeOutMode=TAPE_OUT_DATA;
  index=16;
  byte=0;
  addr=0;
  return;
 }
 //передаём данные 
 if (TapeOutMode==TAPE_OUT_DATA)
 {   
  if (index>=16)
  {     
   if (addr>=BlockSize)
   {
    TapeOutMode=TAPE_OUT_STOP;
	DataCounter=0;
	return;
   }
   index=0;
   byte=DRAM_ReadByte(addr);
   addr++;
   DataCounter=addr;
  }
  //выдаём бит
  if (byte&128) TCNT0=255-(112>>Speed);//начальное значение таймера
           else TCNT0=255-(56>>Speed);//начальное значение таймера
  if ((index%2)==1) byte<<=1;  
  index++;
  return;		
 } 
}