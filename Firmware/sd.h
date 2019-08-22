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
#define SD_DO       6

#define SD_SCK_DDR  DDRB
#define SD_SCK_PORT PORTB
#define SD_SCK      7

//----------------------------------------------------------------------------------------------------
//глобальные переменные
//----------------------------------------------------------------------------------------------------
unsigned short BlockByteCounter=512;//считанный байт блока

const char Text_SD_No_SPI_Up[] PROGMEM =       "Карта памяти не \0";
const char Text_SD_No_SPI_Down[] PROGMEM =     "поддерживает SPI\0";
const char Text_SD_No_Response[] PROGMEM =     "Карта молчит!   \0";
const char Text_SD_Size_Error_Up[] PROGMEM =   "Объем SD-карты  \0";
const char Text_SD_Size_Error_Down[] PROGMEM = "не определен!   \0";
const char Text_SD_Size[] PROGMEM =            "Объем SD-карты  \0";
//----------------------------------------------------------------------------------------------------
//прототипы функций
//----------------------------------------------------------------------------------------------------
void SD_Init(void);//инициализация карты памяти

unsigned char SD_TransmitData(unsigned char data);//послать данные SD-карте и принять ответ
unsigned char SD_SendCommandR1(unsigned char b0,unsigned char b1,unsigned char b2,unsigned char b3,unsigned char b4);//послать команду с ответом R1 SD-карте
unsigned long SD_GetSize(void);//получить объём SD-карты в байтах
unsigned short GetBits(unsigned char *data,unsigned char begin,unsigned char end);//получить биты с begin по end включительно
bool SD_BeginReadBlock(unsigned long BlockAddr);//начать чтение блока
bool SD_ReadBlockByte(unsigned char *byte);//считать байт блока
bool SD_ReadBlock(unsigned long BlockAddr,unsigned char *Addr);//считать блок в 512 байт в память
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
 _delay_ms(100);//пауза, пока карта не включится
 unsigned char n;
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
 //настраиваем SPI
 SPCR=(0<<SPIE)|(1<<SPE)|(0<<DORD)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(0<<SPR1)|(0<<SPR0);
 SPSR=(1<<SPI2X);//удвоенная скорость SPI
 _delay_ms(100);
 unsigned char answer=SD_SendCommandR1(0x40,0x00,0x00,0x00,0x00);//CMD0 
 if (answer!=1)//ошибка
 {
  WH1602_SetTextProgmemUpLine(Text_SD_No_SPI_Up);
  WH1602_SetTextProgmemDownLine(Text_SD_No_SPI_Down);
  _delay_ms(3000);
  return;
 }
 SD_TransmitData(0xff);
 unsigned short m;
 for(m=0;m<65535;m++)
 {
  answer=SD_SendCommandR1(0x41,0x00,0x00,0x00,0x00);//CMD1
  SD_TransmitData(0xff);
  if (answer==0) break;//инициализация успешна
  _delay_us(1);
 }
 if (m==65535)
 {
  WH1602_SetTextProgmemUpLine(Text_SD_No_Response);
  _delay_ms(3000); 
  return; 
 }
 //узнаем объём карты памяти
 unsigned long SD_Size=SD_GetSize();
 if (SD_Size==0xffff)//ошибка
 {
  WH1602_SetTextProgmemUpLine(Text_SD_Size_Error_Up);
  WH1602_SetTextProgmemDownLine(Text_SD_Size_Error_Down);
  _delay_ms(3000);
  return;
 }
 unsigned short size=(unsigned short)(SD_Size>>20);
 sprintf(string,"%i МБ",size);
 WH1602_SetTextProgmemUpLine(Text_SD_Size);
 WH1602_SetTextDownLine(string);
 _delay_ms(1000); 
}

//----------------------------------------------------------------------------------------------------
//послать данные SD-карте и принять ответ
//----------------------------------------------------------------------------------------------------
inline unsigned char SD_TransmitData(unsigned char data)
{ 
 SPDR=data;//передаём
 while(!(SPSR&(1<<SPIF)));//ждём завершения передачи и получения ответа
 unsigned char res=SPDR;
 return(res);
}

//----------------------------------------------------------------------------------------------------
//послать команду с ответом R1
//----------------------------------------------------------------------------------------------------
unsigned char SD_SendCommandR1(unsigned char b0,unsigned char b1,unsigned char b2,unsigned char b3,unsigned char b4)
{
 //отправляем команду и считаем её CRC7
 unsigned char crc7=0; 
 unsigned char cmd[5]={b0,b1,b2,b3,b4};
 for(unsigned char n=0;n<5;n++)
 {
  SD_TransmitData(cmd[n]);
  /*
  unsigned char b=cmd[n]; 
  for (unsigned char i=0;i<8;i++) 
  { 
   crc7<<=1; 
   if ((b&0x80)^(crc7&0x80)) crc7^=0x09; 
   b<<=1; 
  }*/
 }
 /*
 crc7=crc7<<1;
 crc7|=1;
 */
 crc7=0x95;
 SD_TransmitData(crc7);//CRC
 //карта может ответить не сразу
 //принимаем ответ R1 (старший бит всегда 0)
 for(unsigned short n=0;n<65535;n++)
 {
  unsigned char res=SD_TransmitData(0xff);
  if ((res&128)==0)
  {
   return(res);//это действительно ответ
  }
 }
 return(0xff);//ответ не принят
}
//----------------------------------------------------------------------------------------------------
//получить объём SD-карты в байтах
//----------------------------------------------------------------------------------------------------
unsigned long SD_GetSize(void)
{
 unsigned short n;
 if (SD_SendCommandR1(0x49,0x00,0x00,0x00,0x00)==0xff) return(0xffff);//ответ не принят
 //считываем 16 байт ответа
 for(n=0;n<65535;n++)
 {
  if (SD_TransmitData(0xff)==0xfe) break;//получено начало ответа
  _delay_us(1);
 }
 if (n==65535) return(0xffff);//ответ не принят
 unsigned char b[16];
 for(n=0;n<16;n++) b[n]=SD_TransmitData(0xff);
 //пустые байты
 for(n=0;n<255;n++) SD_TransmitData(0xff);
 //смотрим размер карты памяти
 unsigned long c_size=GetBits(b,73,62);
 unsigned long c_size_mult=GetBits(b,49,47);
 unsigned long blocks=(c_size+1UL)*(1UL<<(c_size_mult+2UL));
 unsigned long read_bl_len=GetBits(b,83,80);
 blocks*=(1UL<<read_bl_len);
 return(blocks);
}
//----------------------------------------------------------------------------------------------------
//получить биты с begin по end включительно
//----------------------------------------------------------------------------------------------------
unsigned short GetBits(unsigned char *data,unsigned char begin,unsigned char end)
{
 unsigned short bits=0;
 unsigned char size=1+begin-end; 
 for(unsigned char i=0;i<size;i++) 
 {
  unsigned char position=end+i;
  unsigned short byte=15-(position>>3);
  unsigned short bit=position&0x7;
  unsigned short value=(data[byte]>>bit)&1;
  bits|=value<<i;
 }
 return(bits);
}
//----------------------------------------------------------------------------------------------------
//начать чтение блока
//----------------------------------------------------------------------------------------------------
bool SD_BeginReadBlock(unsigned long BlockAddr)
{
 //даём команду чтения блока
 unsigned char a1=(unsigned char)((BlockAddr>>24)&0xff);
 unsigned char a2=(unsigned char)((BlockAddr>>16)&0xff);
 unsigned char a3=(unsigned char)((BlockAddr>>8)&0xff);
 unsigned char a4=(unsigned char)(BlockAddr&0xff);
 unsigned char res=SD_SendCommandR1(0x51,a1,a2,a3,a4);//посылаем CMD17
 if (res!=0) return(false);//ошибка команды
 SD_TransmitData(0xff);//байтовый промежуток
 //ждём начало поступления данных
 unsigned short n;
 for(n=0;n<65535;n++)
 {
  res=SD_TransmitData(0xff);
  if (res==0xfe) break;//маркер получен
 }
 if (n==65535) return(false);//маркер начала данных не получен
 BlockByteCounter=0;
 return(true);
}
//----------------------------------------------------------------------------------------------------
//считать байт блока
//----------------------------------------------------------------------------------------------------
bool SD_ReadBlockByte(unsigned char *byte)
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
//считать блок в 512 байт в память
//----------------------------------------------------------------------------------------------------
bool SD_ReadBlock(unsigned long BlockAddr,unsigned char *Addr)
{
 //даём команду чтения блока
 unsigned char a1=(unsigned char)((BlockAddr>>24)&0xff);
 unsigned char a2=(unsigned char)((BlockAddr>>16)&0xff);
 unsigned char a3=(unsigned char)((BlockAddr>>8)&0xff);
 unsigned char a4=(unsigned char)(BlockAddr&0xff);
 unsigned char res=SD_SendCommandR1(0x51,a1,a2,a3,a4);//посылаем CMD17
 if (res!=0) return(false);//ошибка команды
 SD_TransmitData(0xff);//байтовый промежуток
 //ждём начало поступления данных
 unsigned short n;
 for(n=0;n<65535;n++)
 {
  res=SD_TransmitData(0xff);
  if (res==0xfe) break;//маркер получен
 }
 if (n==65535) return(false);//маркер начала данных не получен
 for(n=0;n<512;n++,Addr++)
 {
  *Addr=SD_TransmitData(0xff);//читаем байт с SD-карты
 }
 //считываем CRC
 SD_TransmitData(0xff);
 SD_TransmitData(0xff); 
 return(true);
}