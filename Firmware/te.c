//----------------------------------------------------------------------------------------------------
//������������ ����������
//----------------------------------------------------------------------------------------------------
#include "based.h"

//----------------------------------------------------------------------------------------------------
//���������
//----------------------------------------------------------------------------------------------------

static const char Text_Main_Menu_Select[] PROGMEM =       "    ��������    \0";
static const char Text_Main_Memory_Test[] PROGMEM =       "   ���� ������  \0";
static const char Text_Main_Memory_Test_Error[] PROGMEM = " ������ ������ !\0";
static const char Text_Main_Memory_Test_OK[] PROGMEM =    "������ �������� \0";
static const char Text_Tape_Menu_No_Image[] PROGMEM =     "��� ������ tap !\0";


//----------------------------------------------------------------------------------------------------
//���������� ����������
//----------------------------------------------------------------------------------------------------

extern char String[25];//������

uint16_t BlockSize=0;//������ ����� ������ � ������
volatile uint16_t DataCounter=0;//��������� �������� ���� ������
volatile short LeadToneCounter=0;//����� ������ �����-����
volatile uint8_t TapeOutMode=0;//����� ������
bool TapeOutVolume=false;//���������� ������
volatile uint8_t Speed;//�������� ������

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//����������������
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define TAPE_OUT_LEAD 0
#define TAPE_OUT_SYNCHRO_1 1
#define TAPE_OUT_SYNCHRO_2 2
#define TAPE_OUT_DATA 3
#define TAPE_OUT_STOP 4

#define MAX_LEVEL 5

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//��������� ������
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
//����� �����������
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#define TAPE_OUT_DDR  DDRD
#define TAPE_OUT_PORT PORTD
#define TAPE_OUT_PIN  PIND
#define TAPE_OUT      0

//----------------------------------------------------------------------------------------------------
//��������� �������
//----------------------------------------------------------------------------------------------------
void TapeMenu(void);//���� �����������
void MemoryTest(void);//���� ������
void OutputImage(void);//������ ������
void WaitAnyKey(void);//�������� ����� �������
void InitAVR(void);//������������� �����������
//----------------------------------------------------------------------------------------------------
//�������������� ����������
//----------------------------------------------------------------------------------------------------

#include "dram.h"
#include "wh1602.h"
#include "sd.h"
#include "fat.h"

//----------------------------------------------------------------------------------------------------
//�������� ������� ���������
//----------------------------------------------------------------------------------------------------
int main(void)
{ 
 InitAVR();
 DRAM_Init();
 WH1602_Init(); 
 SD_Init();
 FAT_Init();
 //��������� �������� ����
 uint8_t select_item=0;
 while(1)
 {  
  WH1602_SetTextProgmemUpLine(Text_Main_Menu_Select);
  if (select_item==0) strcpy(String,"> ���������� x1 <");
  if (select_item==1) strcpy(String,"> ���������� x2 <");
  if (select_item==2) strcpy(String,"> ���������� x4 <"); 
  if (select_item==3) strcpy(String,">  ���� ������  <");
  WH1602_SetTextDownLine(String); 
  _delay_ms(500);
  //��� ������� ������
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
//����� �������
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//���� �����������
//----------------------------------------------------------------------------------------------------
void TapeMenu(void)
{
 uint8_t n;
 //��������� � ������� ����� ����� �� �����
 if (FAT_BeginFileSearch()==false)
 {
  WH1602_SetTextProgmemUpLine(Text_Tape_Menu_No_Image); 
  WH1602_SetTextDownLine(""); 
  _delay_ms(2000);
  return;//��� �� ������ �����
 }
 int8_t Directory;//��� ����������
 uint32_t FirstCluster;//������ ������� �����
 uint32_t Size;//������ �����
 uint16_t index=1;//����� �����
 static uint16_t level_index[MAX_LEVEL];//MAX_LEVEL ������� �����������
 uint8_t level=0;
 level_index[0]=index;
 while(1)
 { 
  //������� ������ � SD-�����
  //������ ��� ����� 
  if (FAT_GetFileSearch(String,&FirstCluster,&Size,&Directory)==true) WH1602_SetTextDownLine(String);
  if (Directory==false) sprintf(String,"[%02u:%05u] ����",level,index);
                   else sprintf(String,"[%02u:%05u] �����",level,index);
  WH1602_SetTextUpLine(String);  
  _delay_ms(200);
  //��� ������� ������
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
    if (Directory==0) OutputImage();//��� ����� - ��������� �� ����������
    else
	{
	 if (level<MAX_LEVEL) level_index[level]=index;//���������� ����������� �������
     if (Directory<0)//���� �� ����� �� ������� �����
	 {
	  if (level>0) level--;
	 }
	 else
	 {	 
	  level++;
      if (level<MAX_LEVEL) level_index[level]=1;
	 }
     FAT_EnterDirectory(FirstCluster);//������� � ����������
 	 index=1;
	 if (level<MAX_LEVEL)
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
//���� ������
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
   sprintf(String,"���������:%i %%",progress);
   WH1602_SetTextDownLine(String); 
   last_p=progress;
  } 
  //���������� � ��� �������� 
  for(uint32_t addr=0;addr<131072UL;addr++)
  {   
   uint8_t byte=(b+addr)&0xff;
   DRAM_WriteByte(addr,byte);   
   DRAM_Refresh();
  }
  //���������, ��� ���������� � ��� ��������� �� ���������
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
   sprintf(String,"���������:%i %%",progress);
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
//������ ������
//----------------------------------------------------------------------------------------------------
void OutputImage(void)
{
 _delay_ms(500);
 //��������� ��� ������� ����� tap-�����
 uint16_t block=0;
 while(1)
 {  
  if (FAT_WriteBlock(&BlockSize,block)==false) break;//����� ����� ����������� 
  //������� ����� ����� �����
  sprintf(String,"����:%u [%u]",block+1,BlockSize);
  WH1602_SetTextUpLine(String);  
  //��������� ������ � ������������ ������    
  TCNT0=0;//��������� �������� �������
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
    sprintf(String,"����:%u [0]",block+1);
    WH1602_SetTextUpLine(String);
    uint16_t new_block=block+1;
    //��������� �����
    int delay=200;
    if (BlockSize>0x13) delay=500;//����������� ����
    for(uint16_t n=0;n<delay;n++)
    {
     _delay_ms(10);
     if (BUTTON_SELECT_PIN&(1<<BUTTON_SELECT))//�����
     {    
	  TAPE_OUT_PORT&=0xff^(1<<TAPE_OUT);
	  return;
     }   
     if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER))//�����
     {
	  _delay_ms(200);
	  while(1)
	  {
	   if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER)) break;
	  }
	  _delay_ms(200);
     }
     if (BUTTON_UP_PIN&(1<<BUTTON_UP))//�� ���� �����
     {
      _delay_ms(200);
      new_block=block+1;
      break;
     }
     if (BUTTON_DOWN_PIN&(1<<BUTTON_DOWN))//�� ���� �����
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
     sprintf(String,"����:%u [%u]",block+1,dc);
     WH1602_SetTextUpLine(String);
	 dl=0;
	}
	else dl++;
   }
   _delay_us(10);
   if (BUTTON_SELECT_PIN&(1<<BUTTON_SELECT))//�����
   {
    cli();
	TAPE_OUT_PORT&=0xff^(1<<TAPE_OUT);
	return;
   }   
   if (BUTTON_CENTER_PIN&(1<<BUTTON_CENTER))//�����
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
   
   if (BUTTON_UP_PIN&(1<<BUTTON_UP))//�� ���� �����
   {
    _delay_ms(200);
    block++;
    break;
   }
   if (BUTTON_DOWN_PIN&(1<<BUTTON_DOWN))//�� ���� �����
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
//�������� ����� �������
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
//������������� �����������
//----------------------------------------------------------------------------------------------------
void InitAVR(void)
{
 cli();
 
 //����������� �����
 DDRA=0;
 DDRB=0;
 DDRD=0; 
 DDRC=0;
 
 BUTTON_UP_DDR&=0xff^(1<<BUTTON_UP);
 BUTTON_DOWN_DDR&=0xff^(1<<BUTTON_DOWN);
 BUTTON_CENTER_DDR&=0xff^(1<<BUTTON_CENTER);
 BUTTON_SELECT_DDR&=0xff^(1<<BUTTON_SELECT);

 TAPE_OUT_DDR|=(1<<TAPE_OUT);
 
 //����� ��������� ������
 PORTA=0xff;
 PORTB=0xff;
 PORTD=0xff;
 PORTC=0xff;
 
 //����������� ������ T0
 TCCR0=((0<<CS02)|(1<<CS01)|(1<<CS00));//������ ����� ������� �������� ��������� �� 64
 TCNT0=0;//��������� �������� �������
 TIMSK=(1<<TOIE0);//���������� �� ������������ ������� (������ T0 ������������ � ������� �� ���������� �� 0xff)
 
 TAPE_OUT_PORT&=0xff^(1<<TAPE_OUT);
}
//----------------------------------------------------------------------------------------------------
//���������� ������� ���������� ������� T0 (8-�� ��������� ������) �� ������������
//----------------------------------------------------------------------------------------------------
ISR(TIMER0_OVF_vect)
{ 
 static uint8_t byte=0;//���������� ����
 static uint8_t index=0;//����� ����������� ����
 static uint16_t addr=0;//������� �����
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
 //������� �����-���
 if (TapeOutMode==TAPE_OUT_LEAD)
 {
  TCNT0=255-(142>>Speed);//��������� �������� �������
  if (LeadToneCounter>0) LeadToneCounter--;
  else
  {
   TapeOutMode=TAPE_OUT_SYNCHRO_1;
   return;
  }
 }
 //������� ������������ 1
 if (TapeOutMode==TAPE_OUT_SYNCHRO_1)
 {
  TCNT0=255-(43>>Speed);//��������� �������� �������
  TapeOutMode=TAPE_OUT_SYNCHRO_2;
  return;
 }
 //������� ������������ 2
 if (TapeOutMode==TAPE_OUT_SYNCHRO_2)
 {
  TCNT0=255-(48>>Speed);//��������� �������� �������
  TapeOutMode=TAPE_OUT_DATA;
  index=16;
  byte=0;
  addr=0;
  return;
 }
 //������� ������ 
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
  //����� ���
  if (byte&128) TCNT0=255-(112>>Speed);//��������� �������� �������
           else TCNT0=255-(56>>Speed);//��������� �������� �������
  if ((index%2)==1) byte<<=1;  
  index++;
  return;		
 } 
}