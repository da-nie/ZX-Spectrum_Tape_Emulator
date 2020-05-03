//----------------------------------------------------------------------------------------------------
//подключаемые библиотеки
//----------------------------------------------------------------------------------------------------
#include "fat.h"
#include "based.h"
#include "sd.h"
#include "dram.h"
#include "wh1602.h"

//----------------------------------------------------------------------------------------------------
//макроопределения
//----------------------------------------------------------------------------------------------------

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//смещения в FAT
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//BOOT-сектор и структура BPB
#define BS_jmpBoot			0
#define BS_OEMName			3
#define BPB_BytsPerSec		11
#define BPB_SecPerClus		13
#define BPB_ResvdSecCnt		14
#define BPB_NumFATs			16
#define BPB_RootEntCnt		17
#define BPB_TotSec16		19
#define BPB_Media			21
#define BPB_FATSz16			22
#define BPB_SecPerTrk		24
#define BPB_NumHeads		26
#define BPB_HiddSec			28
#define BPB_TotSec32		32
#define BS_DrvNum			36
#define BS_Reserved1		37
#define BS_BootSig			38
#define BS_VolID			39
#define BS_VolLab			43
#define BS_FilSysType		54
#define BPB_FATSz32			36
#define BPB_ExtFlags		40
#define BPB_FSVer			42
#define BPB_RootClus		44
#define BPB_FSInfo			48
#define BPB_BkBootSec		50
#define BPB_Reserved		52

//тип файловой системы
#define FAT12 0
#define FAT16 1
#define FAT32 2

//последний кластер
#define FAT16_EOC 0xFFF8UL

//атрибуты файла
#define ATTR_READ_ONLY		0x01
#define ATTR_HIDDEN 		0x02
#define ATTR_SYSTEM 		0x04
#define ATTR_VOLUME_ID 		0x08
#define ATTR_DIRECTORY		0x10
#define ATTR_ARCHIVE  		0x20
#define ATTR_LONG_NAME 		(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

//структура MBR

//положение таблицы разделов
#define MBR_TABLE_OFFSET 446
//размер записи
#define MBR_SIZE_OF_PARTITION_RECORD 16
//смещение первого сектора
#define MBR_START_IN_LBA 8

//----------------------------------------------------------------------------------------------------
//константы
//----------------------------------------------------------------------------------------------------

static const char Text_FAT_Type[] PROGMEM =  "Тип ф. системы  \0";
static const char Text_FAT32[] PROGMEM =     "FAT32- ошибка!  \0";
static const char Text_FAT16[] PROGMEM =     "FAT16- ок.      \0";
static const char Text_FAT12[] PROGMEM =     "FAT12- ошибка!  \0";
static const char Text_NoFAT[] PROGMEM =     " FAT  не найдена\0";
static const char Text_MBR_Found[] PROGMEM = "   Найден MBR   \0";

//----------------------------------------------------------------------------------------------------
//глобальные переменные
//----------------------------------------------------------------------------------------------------

extern char String[25];//строка



uint8_t Sector[256];//данные для сектора
uint32_t LastReadSector=0xffffffffUL;//последний считанный сектор
uint32_t FATOffset=0;//смещение FAT

//структура доля поиска файлов внутри каталога
struct SFATRecordPointer
{
 uint32_t BeginFolderAddr;//начальный адрес имён файлов внутри директории
 uint32_t CurrentFolderAddr;//текущий адрес имён файлов внутри директории
 uint32_t BeginFolderCluster;//начальный кластер имени файла внутри директории
 uint32_t CurrentFolderCluster;//текущий кластер имени файла внутри директории
 uint32_t BeginFolderClusterAddr;//начальный адрес текущего кластера
 uint32_t EndFolderClusterAddr;//конечный адрес имён файлов внутри директории (или кластера)
} sFATRecordPointer;

uint32_t FirstRootFolderAddr;//начальный адрес корневой директории

uint32_t SecPerClus;//количество секторов в кластере
uint32_t BytsPerSec;//количество байт в секторе
uint32_t ResvdSecCnt;//размер резервной области

uint32_t FATSz;//размер таблицы FAT
uint32_t DataSec;//количество секторов в регионе данных диска
uint32_t RootDirSectors;//количество секторов, занятых корневой директорией 
uint32_t CountofClusters;//количество кластеров для данных (которые начинаются с номера 2! Это КОЛИЧЕСТВО, а не номер последнего кластера)
uint32_t FirstDataSector;//первый сектор данных
uint32_t FirstRootFolderSecNum;//начало корневой директории (для FAT16 - это сектор и отдельная область, для FAT32 - это ФАЙЛ в области данных с кластером BPB_RootClus)
uint32_t ClusterSize;//размер кластера в байтах

uint8_t FATType=FAT12;//тип файловой системы

//----------------------------------------------------------------------------------------------------
//прототипы функций
//----------------------------------------------------------------------------------------------------
uint32_t GetByte(uint32_t offset);//считать байт
uint32_t GetShort(uint32_t offset);//считать два байта
uint32_t GetLong(uint32_t offset);//считать 4 байта
bool FAT_RecordPointerStepForward(struct SFATRecordPointer *sFATRecordPointerPtr);//переместиться по записи вперёд
bool FAT_RecordPointerStepReverse(struct SFATRecordPointer *sFATRecordPointerPtr);//переместиться по записи назад

//----------------------------------------------------------------------------------------------------
//считать байт
//----------------------------------------------------------------------------------------------------
uint32_t GetByte(uint32_t offset)
{
 offset+=FATOffset;
 uint32_t s=offset>>8UL;//делим на 256
 if (s!=LastReadSector)
 {
  LastReadSector=s;
  bool first=true;
  if ((offset&0x1ffUL)>=256) first=false;
  SD_ReadBlock(s>>1UL,Sector,first);
  //ошибки не проверяем, всё равно ничего сделать не сможем - либо работает, либо нет
 }
 return(Sector[offset&0xFFUL]);
}
//----------------------------------------------------------------------------------------------------
//считать два байта
//----------------------------------------------------------------------------------------------------
uint32_t GetShort(uint32_t offset)
{
 uint32_t v=GetByte(offset+1UL);
 v<<=8UL;
 v|=GetByte(offset);
 return(v);
}
//----------------------------------------------------------------------------------------------------
//считать 4 байта
//----------------------------------------------------------------------------------------------------
uint32_t GetLong(uint32_t offset)
{
 uint32_t v=GetByte(offset+3UL);
 v<<=8UL;
 v|=GetByte(offset+2UL);
 v<<=8UL;
 v|=GetByte(offset+1UL);
 v<<=8UL;
 v|=GetByte(offset);
 return(v);
}

//----------------------------------------------------------------------------------------------------
//переместиться по записи вперёд
//----------------------------------------------------------------------------------------------------
bool FAT_RecordPointerStepForward(struct SFATRecordPointer *sFATRecordPointerPtr)
{
 sFATRecordPointerPtr->CurrentFolderAddr+=32UL;//переходим к следующей записи 
 if (sFATRecordPointerPtr->CurrentFolderAddr>=sFATRecordPointerPtr->EndFolderClusterAddr)//вышли за границу кластера или директории
 {
  if (sFATRecordPointerPtr->BeginFolderAddr==FirstRootFolderAddr)//если у нас закончилась корневая директория
  {
   return(false);
  }  
  else//для не корневой директории узнаём новый адрес кластера
  {
   uint32_t FATClusterOffset=0;//смещение по таблице FAT в байтах (в FAT32 они 4-х байтные, а в FAT16 - двухбайтные)
   if (FATType==FAT16) FATClusterOffset=sFATRecordPointerPtr->CurrentFolderCluster*2UL;//узнаём смещение в таблице FAT
   uint32_t NextClusterAddr=ResvdSecCnt*BytsPerSec+FATClusterOffset;//адрес следующего кластера
   //считываем номер следующего кластера файла
   uint32_t NextCluster=0;
   if (FATType==FAT16) NextCluster=GetShort(NextClusterAddr);
   if (NextCluster==0 || NextCluster>=CountofClusters+2UL || NextCluster>=FAT16_EOC)//такого кластера нет
   {
    return(false);        	
   }
   sFATRecordPointerPtr->CurrentFolderCluster=NextCluster;//переходим к следующему кластеру
   uint32_t FirstSectorofCluster=((sFATRecordPointerPtr->CurrentFolderCluster-2UL)*SecPerClus)+FirstDataSector; 
   sFATRecordPointerPtr->CurrentFolderAddr=FirstSectorofCluster*BytsPerSec; 
   sFATRecordPointerPtr->BeginFolderClusterAddr=sFATRecordPointerPtr->CurrentFolderAddr;
   sFATRecordPointerPtr->EndFolderClusterAddr=sFATRecordPointerPtr->CurrentFolderAddr+SecPerClus*BytsPerSec;
  }
 }
 return(true);
}
//----------------------------------------------------------------------------------------------------
//переместиться по записи назад
//----------------------------------------------------------------------------------------------------
bool FAT_RecordPointerStepReverse(struct SFATRecordPointer *sFATRecordPointerPtr)
{
 sFATRecordPointerPtr->CurrentFolderAddr-=32UL;//возвращаемся на запись назад 
 if (sFATRecordPointerPtr->CurrentFolderAddr<sFATRecordPointerPtr->BeginFolderClusterAddr)//вышли за нижнюю границу кластера
 {
  if (sFATRecordPointerPtr->BeginFolderAddr==FirstRootFolderAddr)//если у нас корневая директория
  {
   return(false);//вышли за пределы директории   
  }  
  else//для не корневой директории узнаём новый адрес
  {
   uint32_t PrevCluster=sFATRecordPointerPtr->BeginFolderCluster;//предыдущий кластер   
   while(1)
   {
    uint32_t FATClusterOffset=0;//смещение по таблице FAT в байтах (в FAT32 они 4-х байтные, а в FAT16 - двухбайтные)
    if (FATType==FAT16) FATClusterOffset=PrevCluster*2UL;
    uint32_t ClusterAddr=ResvdSecCnt*BytsPerSec+FATClusterOffset;//адрес предыдущего кластера
	uint32_t cluster=GetShort(ClusterAddr);
    if (cluster<=2 || cluster>=FAT16_EOC)//такого кластера нет
	{
     return(false);//вышли за пределы директории        	
	}
	if (cluster==sFATRecordPointerPtr->CurrentFolderCluster) break;//мы нашли предшествующий кластер
	PrevCluster=cluster;
   }
   if (PrevCluster<=2 || PrevCluster>=FAT16_EOC)//такого кластера нет
   {
    return(false);//вышли за пределы директории        	
   }
   sFATRecordPointerPtr->CurrentFolderCluster=PrevCluster;//переходим к предыдущему кластеру
   uint32_t FirstSectorofCluster=((sFATRecordPointerPtr->CurrentFolderCluster-2UL)*SecPerClus)+FirstDataSector; 
   sFATRecordPointerPtr->BeginFolderClusterAddr=FirstSectorofCluster*BytsPerSec; 	
   sFATRecordPointerPtr->EndFolderClusterAddr=sFATRecordPointerPtr->BeginFolderClusterAddr+SecPerClus*BytsPerSec;
   sFATRecordPointerPtr->CurrentFolderAddr=sFATRecordPointerPtr->EndFolderClusterAddr-32UL;//на запись назад
  }
 }
 return(true);
}

//----------------------------------------------------------------------------------------------------
//Инициализация FAT
//----------------------------------------------------------------------------------------------------
void FAT_Init(void)
{
 WH1602_SetTextUpLine("");
 WH1602_SetTextDownLine("");
 //ищем раздел с FAT16
 LastReadSector=0xffffffffUL; 
 FATOffset=0;
 
 uint8_t b;
 b=GetByte(0); 
 if (b==0xEB || b==0xE9)//это точно не MBR
 {
  if (GetByte(510UL)==0x55 && GetByte(511UL)==0xAA)//раздел найден и он единственный
  {
  }
  else//раздел не найден
  {
   WH1602_SetTextProgmemDownLine(Text_NoFAT);
   while(1);  
  }
 }
 else//это, возможно, MBR
 {
  if (GetByte(510UL)==0x55 && GetByte(511UL)==0xAA)//это MBR, считываем первый раздел
  {  
   WH1602_SetTextProgmemUpLine(Text_MBR_Found);
   _delay_ms(2000);  
  
   uint8_t offset[4];
   size_t partition;
   for(partition=0;partition<4;partition++)
   {
    FATOffset=0;	
    offset[0]=GetByte(MBR_TABLE_OFFSET+MBR_SIZE_OF_PARTITION_RECORD*partition+MBR_START_IN_LBA+0);
    offset[1]=GetByte(MBR_TABLE_OFFSET+MBR_SIZE_OF_PARTITION_RECORD*partition+MBR_START_IN_LBA+1);
    offset[2]=GetByte(MBR_TABLE_OFFSET+MBR_SIZE_OF_PARTITION_RECORD*partition+MBR_START_IN_LBA+2);
    offset[3]=GetByte(MBR_TABLE_OFFSET+MBR_SIZE_OF_PARTITION_RECORD*partition+MBR_START_IN_LBA+3);
    FATOffset=*((uint32_t*)offset);
    FATOffset*=512UL;
    b=GetByte(0);
    if ((b==0xEB || b==0xE9) && GetByte(510UL)==0x55 && GetByte(511UL)==0xAA) break;//раздел найден
   }
   if (partition==4)//раздел не найден
   {
    WH1602_SetTextProgmemDownLine(Text_NoFAT);
    while(1);  
   }   
  }
  else//это не MBR
  {
   WH1602_SetTextProgmemDownLine(Text_NoFAT);
   while(1);  
  }
 } 
 
 LastReadSector=0xffffffffUL;
 
 SecPerClus=GetByte(BPB_SecPerClus);//количество секторов в кластере
 BytsPerSec=GetShort(BPB_BytsPerSec);//количество байт в секторе
 ResvdSecCnt=GetShort(BPB_ResvdSecCnt);//размер резервной области
 
 //определяем количество секторов, занятых корневой директорией 
 RootDirSectors=(uint32_t)(ceil((GetShort(BPB_RootEntCnt)*32UL+(BytsPerSec-1UL))/BytsPerSec));
 //определяем размер таблицы FAT
 FATSz=GetShort(BPB_FATSz16);//размер одной таблицы FAT в секторах
 if (FATSz==0) FATSz=GetLong(BPB_FATSz32);
 //определяем количество секторов в регионе данных диска
 uint32_t TotSec=GetShort(BPB_TotSec16);//общее количество секторов на диске
 if (TotSec==0) TotSec=GetLong(BPB_TotSec32);
 DataSec=TotSec-(ResvdSecCnt+GetByte(BPB_NumFATs)*FATSz+RootDirSectors);
 //определяем количество кластеров для данных (которые начинаются с номера 2! Это КОЛИЧЕСТВО, а не номер последнего кластера)
 CountofClusters=(uint32_t)floor(DataSec/SecPerClus);
 //определяем первый сектор данных
 FirstDataSector=ResvdSecCnt+(GetByte(BPB_NumFATs)*FATSz)+RootDirSectors;
 //определим тип файловой системы

 FATType=FAT12;
 WH1602_SetTextProgmemUpLine(Text_FAT_Type);
 if (CountofClusters<4085UL)
 {
  WH1602_SetTextProgmemDownLine(Text_FAT12);
  while(1);
 }
 else
 {
  if (CountofClusters<65525UL)
  {
   WH1602_SetTextProgmemDownLine(Text_FAT16);
   _delay_ms(2000);
   FATType=FAT16;
  }
  else
  {
   WH1602_SetTextProgmemDownLine(Text_FAT32);
   FATType=FAT32;
   while(1);   
  }
 }
 if (FATType==FAT12) return;//не поддерживаем
 if (FATType==FAT32) return;//не поддерживаем
 //определяем начало корневой директории (для FAT16 - это сектор и отдельная область, для FAT32 - это ФАЙЛ в области данных с кластером BPB_RootClus)
 FirstRootFolderSecNum=ResvdSecCnt+(GetByte(BPB_NumFATs)*FATSz);
 ClusterSize=SecPerClus*BytsPerSec;//размер кластера в байтах

 //читаем корневую директорию
 FirstRootFolderAddr=FirstRootFolderSecNum*BytsPerSec;//начальный адрес корневой директории
 //настраиваем структуру для поиска внутри директории
 sFATRecordPointer.BeginFolderAddr=FirstRootFolderAddr;//начальный адрес имён файлов внутри директории
 sFATRecordPointer.CurrentFolderAddr=sFATRecordPointer.BeginFolderAddr;//текущий адрес имён файлов внутри директории
 sFATRecordPointer.BeginFolderCluster=0;//начальный кластер имени файла внутри директории
 sFATRecordPointer.CurrentFolderCluster=0;//текущий кластер имени файла внутри директории
 sFATRecordPointer.EndFolderClusterAddr=sFATRecordPointer.BeginFolderAddr+(RootDirSectors*BytsPerSec);//конечный адрес имён файлов внутри директории (или кластера)
 sFATRecordPointer.BeginFolderClusterAddr=sFATRecordPointer.CurrentFolderAddr;//адрес начального кластера директории
}
//----------------------------------------------------------------------------------------------------
//начать поиск файла в кталоге
//----------------------------------------------------------------------------------------------------
bool FAT_BeginFileSearch(void)
{
 uint32_t FirstCluster;//первый кластер файла
 uint32_t Size;//размер файла
 int8_t Directory;//не директория ли файл
 
 sFATRecordPointer.CurrentFolderAddr=sFATRecordPointer.BeginFolderAddr;
 sFATRecordPointer.CurrentFolderCluster=sFATRecordPointer.BeginFolderCluster;
 sFATRecordPointer.BeginFolderClusterAddr=sFATRecordPointer.CurrentFolderAddr;
 if (sFATRecordPointer.BeginFolderAddr!=FirstRootFolderAddr)//это не корневая директория
 {
  sFATRecordPointer.EndFolderClusterAddr=sFATRecordPointer.BeginFolderAddr+SecPerClus*BytsPerSec;
 }
 else sFATRecordPointer.EndFolderClusterAddr=sFATRecordPointer.BeginFolderAddr+(RootDirSectors*BytsPerSec);//конечный адрес имён файлов внутри директории (или кластера)
 //переходим к первому нужному нам файлу
 while(1)
 {
  if (FAT_GetFileSearch(NULL,&FirstCluster,&Size,&Directory)==false)
  {
   if (FAT_NextFileSearch()==false) return(false);
  } 
  else return(true);
 }
 return(false);
}
//----------------------------------------------------------------------------------------------------
//перейти к предыдущему файлу в каталоге
//----------------------------------------------------------------------------------------------------
bool FAT_PrevFileSearch(void)
{
 struct SFATRecordPointer sFATRecordPointer_Copy=sFATRecordPointer;
 while(1)
 {
  if (FAT_RecordPointerStepReverse(&sFATRecordPointer_Copy)==false) return(false);  
  //анализируем имя файла
  uint8_t n;
  bool res=true;
  for(n=0;n<11;n++)
  {
   uint8_t b=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+(uint32_t)(n));
   if (n==0)
   {
    if (b==0x20 || b==0xE5)
	{
     res=false;
     break;	
	}
   }
   if (b<0x20)
   {
    res=false;
    break;
   }
   if (n==1)
   {
    uint8_t a=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr);
    uint8_t b=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+1UL);
    if (a==(uint8_t)'.' && b!=(uint8_t)'.')
    {
     res=false; 
	 break;
    }	
   }   
  }
  //смотрим расширение
  if (res==true)
  {  
   uint8_t type=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+11UL);
   if (type&ATTR_VOLUME_ID) continue;//этот файл - имя диска     
   if ((type&ATTR_DIRECTORY)==0)//это файл
   {
    uint8_t a=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+10UL);
    uint8_t b=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+9UL);
    uint8_t c=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+8UL);
    if (!(a=='P' && b=='A' && c=='T')) continue;//неверное расширение
   }
   sFATRecordPointer=sFATRecordPointer_Copy;
   return(true);
  }
 }
 return(false);
}
//----------------------------------------------------------------------------------------------------
//перейти к следующему файлу в каталоге
//----------------------------------------------------------------------------------------------------
bool FAT_NextFileSearch(void)
{
 struct SFATRecordPointer sFATRecordPointer_Copy=sFATRecordPointer;
 while(1)
 {
  if (FAT_RecordPointerStepForward(&sFATRecordPointer_Copy)==false) return(false); 
  uint8_t n;
  bool res=true;
  for(n=0;n<11;n++)
  {
   uint8_t b=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+(uint32_t)(n));
   if (n==0)
   {
    if (b==0x20 || b==0xE5)
	{
     res=false;
     break;	
	}
   }
   if (b<0x20)
   {
    res=false;
    break;
   }
   if (n==1)
   {
    uint8_t a=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr);
    uint8_t b=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+1UL);
    if (a==(uint8_t)'.' && b!=(uint8_t)'.')
    {
     res=false; 
	 break;
    }	
   }     
  }
  if (res==true)
  {
   uint8_t type=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+11UL);
   if (type&ATTR_VOLUME_ID) continue;//этот файл - имя диска     
   if ((type&ATTR_DIRECTORY)==0)//это файл
   {
    uint8_t a=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+10UL);
    uint8_t b=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+9UL);
    uint8_t c=GetByte(sFATRecordPointer_Copy.CurrentFolderAddr+8UL);
    if (!(a=='P' && b=='A' && c=='T')) continue;//неверное расширение
   }
   sFATRecordPointer=sFATRecordPointer_Copy;
   return(true);
  }
 }
 return(false);
}
//----------------------------------------------------------------------------------------------------
//получить параметры текущего найденного файла в каталоге
//----------------------------------------------------------------------------------------------------
bool FAT_GetFileSearch(char *filename,uint32_t *FirstCluster,uint32_t *Size,int8_t *directory)
{
 uint8_t n;
 bool res=true;
 *directory=0;
 if (filename!=NULL)
 {
  for(n=0;n<11;n++) filename[n]=32;
 }
 for(n=0;n<11;n++)
 {    
  uint8_t b=GetByte(sFATRecordPointer.CurrentFolderAddr+(uint32_t)(n));
  if (n==0)
  {
   if (b==0x20 || b==0xE5)
   {
    res=false;
    break;	
   }
  }
  if (b<0x20)
  {
   res=false;
   break;
  }
  if (filename!=NULL)
  {
   if (n<8) filename[n]=b;
       else filename[n+1]=b;
  }
  if (n==1)
  {
   uint8_t a=GetByte(sFATRecordPointer.CurrentFolderAddr);
   uint8_t b=GetByte(sFATRecordPointer.CurrentFolderAddr+1UL);
   if (a==(uint8_t)'.' && b!=(uint8_t)'.')
   {
    res=false; 
    break;
   }	
  }     
 }
 if (res==true)
 {
  uint8_t type=GetByte(sFATRecordPointer.CurrentFolderAddr+11UL);  
  if (type&ATTR_VOLUME_ID) return(false);//этот файл - имя диска  
  if ((type&ATTR_DIRECTORY)==0)//это файл
  {
   uint8_t a=GetByte(sFATRecordPointer.CurrentFolderAddr+10UL);
   uint8_t b=GetByte(sFATRecordPointer.CurrentFolderAddr+9UL);
   uint8_t c=GetByte(sFATRecordPointer.CurrentFolderAddr+8UL);
   if (!(a=='P' && b=='A' && c=='T')) return(false);//неверное расширение
  }
  else//если это директория
  {
   uint8_t a=GetByte(sFATRecordPointer.CurrentFolderAddr);
   uint8_t b=GetByte(sFATRecordPointer.CurrentFolderAddr+1UL);  
   if (a==(uint8_t)'.' && b==(uint8_t)'.') *directory=-1;//на директорию выше
                                        else *directory=1;//на директорию ниже
  } 
  //первый кластер файла  
  *FirstCluster=(GetShort(sFATRecordPointer.CurrentFolderAddr+20UL)<<16)|GetShort(sFATRecordPointer.CurrentFolderAddr+26UL);
  //узнаём размер файла в байтах
  *Size=GetLong(sFATRecordPointer.CurrentFolderAddr+28UL);
  if (filename!=NULL)
  {
   if ((type&ATTR_DIRECTORY)==0) filename[8]='.';//файлу добавляем точку    
   filename[12]=0;   
   //поищем длинное имя файла   
   struct SFATRecordPointer sFATRecordPointer_Local=sFATRecordPointer;
   uint8_t long_name_length=0;
   while(1)
   {
    if (FAT_RecordPointerStepReverse(&sFATRecordPointer_Local)==false) break;
    uint8_t attr=GetByte(sFATRecordPointer_Local.CurrentFolderAddr+11UL);
    if ((attr&ATTR_LONG_NAME)==ATTR_LONG_NAME)//это длинное имя
    {
     //собираем полное имя
     uint8_t name_index=GetByte(sFATRecordPointer_Local.CurrentFolderAddr);
     for(n=0;n<10 && long_name_length<=16;n+=2,long_name_length++) filename[long_name_length]=GetByte(sFATRecordPointer_Local.CurrentFolderAddr+n+1UL);
     for(n=0;n<12 && long_name_length<=16;n+=2,long_name_length++) filename[long_name_length]=GetByte(sFATRecordPointer_Local.CurrentFolderAddr+n+14UL);
	 for(n=0;n<4 && long_name_length<=16;n+=2,long_name_length++) filename[long_name_length]=GetByte(sFATRecordPointer_Local.CurrentFolderAddr+n+28UL);
	 if (long_name_length>16) break;
     if (name_index&0x40) break;//последняя часть имени
    }
    else break;//это не длинное имя
   }
   if (long_name_length>16) long_name_length=16;
   if (long_name_length>0) filename[long_name_length]=0;
  }
  return(true);
 }
 return(false); 
}
//----------------------------------------------------------------------------------------------------
//зайти в директорию и найти первый файл
//----------------------------------------------------------------------------------------------------
bool FAT_EnterDirectory(uint32_t FirstCluster)
{ 
 if (FirstCluster==0UL)//это корневая директория (номер первого кластера, распределяемого директории)
 {
  sFATRecordPointer.BeginFolderAddr=FirstRootFolderAddr; 
  sFATRecordPointer.EndFolderClusterAddr=sFATRecordPointer.BeginFolderAddr+(RootDirSectors*BytsPerSec);//конечный адрес имён файлов внутри директории (или кластера)
 }
 else
 {
  uint32_t FirstSectorofCluster=((FirstCluster-2UL)*SecPerClus)+FirstDataSector; 
  sFATRecordPointer.BeginFolderAddr=FirstSectorofCluster*BytsPerSec;//начальный адрес имён файлов внутри директории
  sFATRecordPointer.EndFolderClusterAddr=sFATRecordPointer.BeginFolderAddr+SecPerClus*BytsPerSec;
 }
 sFATRecordPointer.BeginFolderCluster=FirstCluster;//начальный кластер имени файла внутри директории
 sFATRecordPointer.CurrentFolderCluster=sFATRecordPointer.BeginFolderCluster;//текущий кластер имени файла внутри директории
 sFATRecordPointer.CurrentFolderAddr=sFATRecordPointer.BeginFolderAddr;//текущий адрес имён файлов внутри директории
 sFATRecordPointer.BeginFolderClusterAddr=sFATRecordPointer.BeginFolderAddr; 
 return(FAT_BeginFileSearch());
}
//----------------------------------------------------------------------------------------------------
//записать в ОЗУ блок файла
//----------------------------------------------------------------------------------------------------
bool FAT_WriteBlock(uint16_t *BlockSize,uint16_t Block)
{
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
 //конец файла
 return(false);
}