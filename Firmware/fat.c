//----------------------------------------------------------------------------------------------------
//подключаемые библиотеки
//----------------------------------------------------------------------------------------------------
#include "fat.h"
#include "ff.h"
#include "based.h"
#include "sd.h"
#include "dram.h"
#include "wh1602.h"

//----------------------------------------------------------------------------------------------------
//макроопределения
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//константы
//----------------------------------------------------------------------------------------------------
static const char Text_FileSystem[] PROGMEM =             "Файловая система\0";
static const char Text_FileSystem_NotFound[] PROGMEM =    "  не найдена!   \0";
static const char Text_DiskError[] PROGMEM =              "  Ошибка карты! \0";
static const char Text_InvalidDisk[] PROGMEM =            " Неверая карта! \0";
static const char Text_NotReady[] PROGMEM =               "Карта не готова!\0";

//----------------------------------------------------------------------------------------------------
//глобальные переменные
//----------------------------------------------------------------------------------------------------

extern char String[25];//строка
static FATFS FileSystem;//файловая система
DIR Dir;//текущая открытая директория

//----------------------------------------------------------------------------------------------------
//прототипы функций
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//реализация функций
//----------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------
//переопределенная функция получения времени
//------------------------------------------------------------------------------------------
DWORD get_fattime(void)
{
 uint32_t wYear=2020;
 uint32_t wMonth=04;
 uint32_t wDay=25;
 uint32_t wHour=9;
 uint32_t wMinute=08;
 uint32_t wSecond=00;
	
 DWORD ret=((DWORD)(wYear-1980)<<25);	
 ret|=((DWORD)wMonth<<21);
 ret|=((DWORD)wDay<<16);
 ret|=(WORD)(wHour<<11);
 ret|=(WORD)(wMinute<<5);
 ret|=(WORD)(wSecond>>1);
 return(ret);
}

//------------------------------------------------------------------------------------------
//инициализация файловой системы
//------------------------------------------------------------------------------------------
void FAT_Init(void)
{
 WH1602_SetTextUpLine("");
 WH1602_SetTextDownLine("");

 FRESULT res;
 res=f_mount(&FileSystem,"",1);
 if (res==FR_INVALID_DRIVE) WH1602_SetTextProgmemUpLine(Text_InvalidDisk);
 if (res==FR_DISK_ERR) WH1602_SetTextProgmemUpLine(Text_DiskError);
 if (res==FR_NOT_READY) WH1602_SetTextProgmemUpLine(Text_NotReady);
 if (res==FR_NO_FILESYSTEM) 
 {
  WH1602_SetTextProgmemUpLine(Text_FileSystem);
  WH1602_SetTextProgmemDownLine(Text_FileSystem_NotFound);
 }
 if (res!=FR_OK) 
 {
  while(1); 
 }
}

//----------------------------------------------------------------------------------------------------
//начать поиск файла в кталоге
//----------------------------------------------------------------------------------------------------
bool FAT_BeginFileSearch(void)
{
 if (f_opendir(&Dir,"")!=FR_OK) return(false);
 
 
 return(false);
}
//----------------------------------------------------------------------------------------------------
//перейти к предыдущему файлу в каталоге
//----------------------------------------------------------------------------------------------------
bool FAT_PrevFileSearch(void)
{
 
 return(false);
}
//----------------------------------------------------------------------------------------------------
//перейти к следующему файлу в каталоге
//----------------------------------------------------------------------------------------------------
bool FAT_NextFileSearch(void)
{
 
 return(false);
}
//----------------------------------------------------------------------------------------------------
//получить параметры текущего найденного файла в каталоге
//----------------------------------------------------------------------------------------------------
bool FAT_GetFileSearch(char *filename,uint32_t *Size,bool *directory)
{
 FILINFO fno;
 f_readdir(&Dir,&fno);
 if (fno.fattrib&AM_DIR) directory=true;
                    else directory=false;
 

 
 return(false); 
}
//----------------------------------------------------------------------------------------------------
//зайти в директорию и найти первый файл
//----------------------------------------------------------------------------------------------------
bool FAT_EnterDirectory(uint32_t FirstCluster)
{ 
 return(FAT_BeginFileSearch());
}
//----------------------------------------------------------------------------------------------------
//записать в ОЗУ блок файла
//----------------------------------------------------------------------------------------------------
bool FAT_WriteBlock(uint16_t *BlockSize,uint16_t Block)
{
/* 



 uint32_t CurrentCluster;
 uint32_t Size;

 uint32_t i=0;//номер считываемого байта файла
 uint16_t dram_addr=0;//адрес в динамической памяти
 uint16_t current_block=0;//текущий номер блока
 uint16_t block_size=0;//размер блока
 int8_t Directory;//не директория ли файл
 *BlockSize=0;
 if (FAT_GetFileSearch(String,&CurrentCluster,&Size,&Directory)==false) return(false);
 uint8_t mode=0;              
 while(i<Size)
 {
  DRAM_Refresh();//производим регенерацию памяти 
  //считываем данные
  uint32_t length=ClusterSize;
  if (length+i>=Size) length=Size-i;
  //получаем первый сектор кластера
  uint32_t FirstSectorofCluster=((CurrentCluster-2UL)*SecPerClus)+FirstDataSector; 
  uint32_t addr=FirstSectorofCluster*BytsPerSec;
  for(uint32_t m=0;m<length;m++,i++)
  {
   DRAM_Refresh();//производим регенерацию памяти
   uint8_t b=GetByte(addr+m);
   if (mode==0)//чтение младшего байта длины
   {
    block_size=b;
	mode=1;
	continue;
   }
   if (mode==1)//чтение старшего байта длины
   {
    block_size|=((uint16_t)b)<<8;
	mode=2;
	dram_addr=0;
	continue;
   }
   if (mode==2)//чтение данных
   {
    if (current_block==Block) DRAM_WriteByte(dram_addr,b);//это выбранный блок
	dram_addr++;
	if (dram_addr>=block_size)//блок закончен
	{
	 if (current_block==Block)//закончился выбранный блок
	 {
	  *BlockSize=block_size;  
	  return(true);
	 }
	 //переходим к следующему блоку
	 block_size=0;
	 current_block++;
	 mode=0;
	}
   } 
  }
  //переходим к следующему кластеру файла
  uint32_t FATClusterOffset=0;//смещение по таблице FAT в байтах (в FAT32 они 4-х байтные, а в FAT16 - двухбайтные)
  if (FATType==FAT16) FATClusterOffset=CurrentCluster*2UL;
  uint32_t NextClusterAddr=ResvdSecCnt*BytsPerSec+FATClusterOffset;//адрес следующего кластера
  //считываем номер следующего кластера файла
  uint32_t NextCluster=0;
  if (FATType==FAT16) NextCluster=GetShort(NextClusterAddr);
  if (NextCluster==0) break;//неиспользуемый кластер
  if (NextCluster>=CountofClusters+2UL) break;//номер больше максимально возможного номера кластера - конец файла или сбой
  CurrentCluster=NextCluster;
 }
 */
 //конец файла
 return(false);
}