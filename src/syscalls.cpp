#include <stdio.h>
#include <conio.h>
#include <ctype.h>

#include "syscalls.h"

#define DEBUG

/*
#define IO_CAN_WRITE	 1
#define IO_CAN_READ      2
#define IO_CAN_BOTH      3
#define IO_CACHE_ADD     4
#define IO_CACHE_THROUGH 8

#define LSEEK_START	 0
#define LSEEK_CURRENT    1
#define LSEEK_END        2
*/

word CreatePackage( byte Device, byte Party, word &PackageID );

// syscally zacinaji FS, shelove prikazy fs
// ======================= SYSCALLS =================================



#define SysTab OpenFiles[MySysIndex]

/* ==== mount =====
//  ERR_FS_NO_MOUNTPOINT;
//  ERR_FS_SYSTEM_NOT_FORMATTED;
*/

word fs_mount (byte *path)
{
  MNODE *DirNode;
  MasterBootRecord pBR;
  byte dev, par;
  int  mntindex;
  TaskUArea	*Task = GetCurrentTaskUArea();
  word   CallResult;

  SyscallActive = 1;

  Task->ErrorLevel = ERR_FS_NO_ERROR;

/*
struct MntTabItem
{
  byte Used;
  byte Mounted;

  byte *MntPoint;
  byte MntPointDevice, MntPointPartition, MntPointLogical;
  byte MntRootDevice,  MntRootPartition, MntRootLogical;
};
*/



  if ((mntindex = GetMntSystem(path,dev,par)) == -1)
  {
    Task->ErrorLevel = ERR_FS_NO_MOUNTPOINT;
    SyscallActive = 0;
    return -1;
  };


  LoadBoot(MountTab[mntindex].MntRootDevice,
	   MountTab[mntindex].MntRootPartition, &pBR );
  if (!pBR.BPB.RootFNODEAddr)
  {
    Task->ErrorLevel = ERR_FS_SYSTEM_NOT_FORMATTED;
    SyscallActive = 0;
    return -1;
  }

  MountTab[mntindex].MntRootDevice = pBR.BPB.RootFNODEAddr;

  CallResult = NAMEN(path, DirNode, FPACK_ADD);
  if (CallResult != ERR_FS_NO_ERROR)
  {
    Task->ErrorLevel = CallResult;
    SyscallActive = 0;
    return -1;
  }

  MountTab[mntindex].MntPointDevice = DirNode->Device;
  MountTab[mntindex].MntPointPartition = DirNode->Partition;
  MountTab[mntindex].MntPointLogical = DirNode->logical;

  DirNode->IsMountPoint = 1;
  DirNode->Locked.Release();

  SyscallActive = 0;
  return ERR_FS_NO_ERROR;
}

/* ==== unmount ====
   ERR_FS_NO_MOUNTPOINT;
*/

word fs_unmount (byte *path)
{
  TaskUArea	*Task = GetCurrentTaskUArea();
  MNODE *DirNode;
  word   CallResult;
  int    mntindex;
  byte   dev,par;

    SyscallActive = 1;
  Task->ErrorLevel = ERR_FS_NO_ERROR;

  if ((mntindex = GetMntSystem(path,dev,par)) == -1)
  {
    Task->ErrorLevel = ERR_FS_NO_MOUNTPOINT;
    SyscallActive = 0;
    return -1;
  };

  CallResult = NAMEN(path, DirNode, FPACK_ADD);
  if (CallResult != ERR_FS_NO_ERROR)
  {
    Task->ErrorLevel = CallResult;
    SyscallActive = 0;
    return -1;
  }

  MountTab[mntindex].Mounted = 0;
  DirNode->IsMountPoint = 0;
  DirNode->Locked.Release();
  ActiveNodes->Release(DirNode);

  SyscallActive = 0;
  return ERR_FS_NO_ERROR;
}


/* ==== link ====
     ERR_FS_NO_ERROR           - OK
     ERR_DIRREC_NO_DIR         - cast cesty neni adresar
     ERR_DIRREC_NOT_FOUND
     ERR_NOT_SAME_SYSTEMS;
     ERR_DIRREC_ALREADY_EXISTS;
     ERR_CANNOT_MAKE_DIR_LINK
     + ostatni
*/

word fs_link(byte *path1, byte *path2)
{
   TaskUArea	*Task = GetCurrentTaskUArea();
   MNODE	*OldNode,*DirNode;
   byte		buffer[255],*dir;
   word 	CallResult;
   dword	logical;

    SyscallActive = 1;
   Task->ErrorLevel = ERR_FS_NO_ERROR;

   CallResult = NAMEN(path1, OldNode, FPACK_ADD);
   if (CallResult != ERR_FS_NO_ERROR)
   {
    SyscallActive = 0;
    return ERR_DIRREC_NOT_FOUND;
   }

   if (OldNode->Type & NODE_TYPE_DIRECTORY)
   {
     OldNode->Locked.Release();
     ActiveNodes->Release(OldNode);
     SyscallActive = 0;
     return ERR_CANNOT_MAKE_DIR_LINK;
   }

   OldNode->Links++;
   OldNode->DirtyByData=1;
   OldNode->Locked.Release();

   dir = new byte [strlen(path2)+1];
   strcpy(dir,path2);

   // zjisti umisteni adresare, ve kterem bude ulozen link
   CutLastPathPart (dir,buffer);
   CallResult = NAMEN( dir, DirNode, FPACK_ADD);
   if ( CallResult != ERR_FS_NO_ERROR)
   {
     OldNode->Locked.Acquire();
     OldNode->Links--;
     OldNode->Locked.Release();
     ActiveNodes->Release(OldNode);
     delete [] dir;
     SyscallActive = 0;
     return CallResult;
   }

   if (!(DirNode->Type & NODE_TYPE_DIRECTORY))
   {
     DirNode->Locked.Release();
     ActiveNodes->Release(DirNode);
     OldNode->Locked.Acquire();
     OldNode->Links--;
     OldNode->Locked.Release();
     ActiveNodes->Release(OldNode);
     delete [] dir;
     SyscallActive = 0;
     return ERR_NOT_DIRECTORY;
   }

   if ( (DirNode->Device != OldNode->Device) ||
	(DirNode->Partition != OldNode->Partition) )
   {
     DirNode->Locked.Release();
     ActiveNodes->Release(DirNode);
     OldNode->Locked.Acquire();
     OldNode->Links--;
     OldNode->Locked.Release();
     ActiveNodes->Release(OldNode);
     delete [] dir;
     SyscallActive = 0;
     return ERR_NOT_SAME_SYSTEMS;
   }

   CallResult = SearchDirByName(DirNode, buffer, logical, FPACK_ADD, 0);
   if (CallResult != ERR_DIRREC_NOT_FOUND)
   {
     DirNode->Locked.Release();
     ActiveNodes->Release(DirNode);
     OldNode->Locked.Acquire();
     OldNode->Links--;
     OldNode->Locked.Release();
     ActiveNodes->Release(OldNode);
     delete [] dir;
     SyscallActive = 0;
     if (CallResult == ERR_FS_NO_ERROR) return ERR_DIRREC_ALREADY_EXISTS;
				   else return CallResult;
   }

   CallResult = AddDirectoryEntry(DirNode,   OldNode->logical,
				 FPACK_ADD, buffer, 0);

   if (CallResult != ERR_FS_NO_ERROR)
   {
     DirNode->Locked.Release();
     ActiveNodes->Release(DirNode);
     OldNode->Locked.Acquire();
     OldNode->Links--;
     OldNode->Locked.Release();
     ActiveNodes->Release(OldNode);
     delete [] dir;
     SyscallActive = 0;
     return ERR_NOT_SAME_SYSTEMS;
   }

 DirNode->Locked.Release();

 CacheManCommitPackage(DirNode->packID);
 CacheManCommitPackage(OldNode->packID);

 ActiveNodes->Release(DirNode);
 ActiveNodes->Release(OldNode);
 delete [] dir;
 SyscallActive = 0;
 return ERR_FS_NO_ERROR;
}



/* ==== open ====
     ERR_FS_NO_ERROR           - OK
     ERR_DIRREC_NO_DIR         - cast cesty neni adresar
     ERR_DIRREC_NOT_FOUND :
     ERR_TOO_MANY_OPENED_FILES - otevrenych souboru vice nez MaxOpenedFiles

     + ostatni
*/

int  fs_open(byte *path, word mode, word rights)
{
  int		i,j,MySystemIndex;
  TaskUArea	*Task = GetCurrentTaskUArea();
  word		CallResult, MyIndex;
  MNODE		*MyNode;
//  word		CacheFlag;

   SyscallActive = 1;
  if ((mode & IO_CREAT) ||
      (mode & IO_TRUNC) ) return fs_creat(path, mode, rights);

  Task->ErrorLevel = ERR_FS_NO_ERROR;
//  CacheFlag = (mode &  IO_CACHE_THROUGH) ? FPACK_NOTHING : FPACK_ADD;


  MyIndex = BitmapGetFirstHole(Task->Bmp,MaxOpenedFiles);
  if (MyIndex == 0xffff)
  {
   Task->ErrorLevel = ERR_TOO_MANY_OPENED_FILES;
   SyscallActive = 0;
   return -1;
  }

  CallResult = NAMEN(path, MyNode, FPACK_ADD);
  if ( CallResult != ERR_FS_NO_ERROR)
  {
    Task->ErrorLevel = CallResult;
    SyscallActive = 0;
    return -1;
  }
  MyNode->Locked.Release();

  // MyNode je v tabulce aktivnich nodu odemceny


  // pristup k systemove tabulce, zjisti prvni volnou diru
  SystemTableSem->Down();

  Task->OpenFiles[MyIndex].SystemTabIndex =
   BitmapGetFirstHole(SystemOpenFilesBmp, MaxOpenedFiles);
  if (Task->OpenFiles[MyIndex].SystemTabIndex == 0xffff)
  {
    SystemTableSem->Up();
    MyNode->Locked.Release();
    Task->ErrorLevel = ERR_TOO_MANY_OPENED_FILES;
    SyscallActive = 0;
    return -1;
  }
  BitmapSetBit(SystemOpenFilesBmp, Task->OpenFiles[MyIndex].SystemTabIndex);
  SystemTableSem->Up();

  MySystemIndex = Task->OpenFiles[MyIndex].SystemTabIndex;



  OpenFiles[MySystemIndex].Used = 1;
  OpenFiles[MySystemIndex].Access = mode;

  if (mode & IO_APPEND) OpenFiles[MySystemIndex].Position = MyNode->Size;
			OpenFiles[MySystemIndex].Position = 0;

  OpenFiles[MySystemIndex].DirtyBuffer = 1;
  OpenFiles[MySystemIndex].BufferPos = 0;
  OpenFiles[MySystemIndex].Buffer = new byte[512];
  OpenFiles[MySystemIndex].LastPreload = 0;
  OpenFiles[MySystemIndex].MemNODE = MyNode;


  Task->OpenFiles[MyIndex].Bitmap = new byte[(MaxOpenedFiles>>3)+1];
  memset(Task->OpenFiles[MyIndex].Bitmap,0,(MaxOpenedFiles>>3)+1);

  // podiva se jestli uz proces neotevrel stejny soubor,
  // pokud ano do bitmapy si zanese jeho indexy a svuj index
  // zanese do jeho bitmapy (pokud nejakou ma)
  for (i=5,j=Task->NrOpenedFiles-5; i<MaxOpenedFiles; ++i)
  {
   if (!j) break;
   if ((i != MyIndex) && (Task->OpenFiles[i].Used))
   {
     if (MyNode == OpenFiles[Task->OpenFiles[i].SystemTabIndex].MemNODE)
     {
       if (Task->OpenFiles[i].Bitmap)
	BitmapSetBit(Task->OpenFiles[i].Bitmap, MyIndex);
	BitmapSetBit(Task->OpenFiles[MyIndex].Bitmap, i);
     }
     j--;
   }
  }

  Task->OpenFiles[MyIndex].Used = 1;
  Task->NrOpenedFiles++;
  BitmapSetBit(Task->Bmp,MyIndex);

  SyscallActive = 0;
  return MyIndex;
}




/* ==== LSeek ====
     ERR_FS_NO_ERROR           - OK
     ERR_NOT_VALID_FILE_HANDLE       1509
     + ostatni

     chyba 0xffffffff a chyba u tasku
*/

dword fs_lseek(int handle, dword offset, word origin)
{
  TaskUArea *Task = GetCurrentTaskUArea();
  int	    MySysIndex;

  SyscallActive = 1;

  if (!Task->OpenFiles[handle].Used)
  {
    Task->ErrorLevel = ERR_NOT_VALID_FILE_HANDLE;
    SyscallActive = 0;
    return 0xffffffff;
  }

  MySysIndex = Task->OpenFiles[handle].SystemTabIndex;
  switch(origin)
  {
    case LSEEK_START :   SysTab.Position = offset;
			 break;

    case LSEEK_CURRENT : SysTab.Position += offset;
			 break;

    case LSEEK_END :     SysTab.Position = SysTab.MemNODE->Size - offset;
			 break;
  }

  SysTab.DirtyBuffer = 1;
  if (SysTab.Position > SysTab.MemNODE->Size)
   SysTab.Position = SysTab.MemNODE->Size;

 SyscallActive = 0;
 return SysTab.Position;
}


/* ==== Write ====
     ERR_FS_NO_ERROR           - OK
     ERR_FILE_NOT_TO_WRITE
     ERR_NOT_VALID_FILE_HANDLE       1509
     + ostatni
*/

void SetDirty(word index, ProcessOpenedFiles *fls)
{
  OpenFiles[fls[index].SystemTabIndex].DirtyBuffer = 1;
}

dword fs_write(int handle, void far *buffer, dword size)
{
  TaskUArea *Task = GetCurrentTaskUArea();
  int	    MySysIndex;
  dword	    where_pos,q;
  MNODE	    *MyNode;
  word      CacheFlag, CallResult;
  dword	    preload;

  SyscallActive = 1;
  Task->ErrorLevel = ERR_FS_NO_ERROR;


  if (!Task->OpenFiles[handle].Used)
  {
    Task->ErrorLevel = ERR_NOT_VALID_FILE_HANDLE;
    SyscallActive = 0;
    return 0xffffffff;
  }

  MySysIndex = Task->OpenFiles[handle].SystemTabIndex;
  if (!(SysTab.Access & IO_CAN_WRITE))
  {
    Task->ErrorLevel = ERR_FILE_NOT_TO_WRITE;
    SyscallActive = 0;
    return 0xffffffff;
  }

  where_pos = 0;
  CacheFlag = (SysTab.Access & IO_CACHE_ADD) ? FPACK_ADD : FPACK_NOTHING;

  MyNode = SysTab.MemNODE;
  MyNode->Locked.Acquire();
  if (SysTab.DirtyBuffer)
  {
   CallResult = PMAP ( MyNode,           SysTab.Position,
		       SysTab.logical,          MyNode->packID,
		       PMAP_WRITE,        SysTab.Buffer,
		       SysTab.BufferPos, preload,
		       CacheFlag );
   if (CallResult != ERR_FS_NO_ERROR) goto WriteError;
   SysTab.DirtyBuffer = 0;
  }

  if (size > 512-SysTab.BufferPos)
   {
    q = 512-SysTab.BufferPos;
    size -= (512-SysTab.BufferPos);
   }
   else
   {
    q = size;
    size = 0;
   }

  memcpy(SysTab.Buffer+SysTab.BufferPos,buffer,q);

  CallResult =
   CacheManSaveSector( MyNode->Device, MyNode->Partition,
		       SysTab.logical,1,
		       MyNode->packID, CacheFlag, SysTab.Buffer);
   if (CallResult != ERR_FS_NO_ERROR) goto WriteError;
  where_pos += q; SysTab.Position +=q; SysTab.BufferPos += q;
  if (SysTab.Position > SysTab.MemNODE->Size)
   SysTab.MemNODE->Size = SysTab.Position;


  while (size)
  {
   CallResult = PMAP ( MyNode,           SysTab.Position,
		       SysTab.logical,  MyNode->packID,
		       PMAP_WRITE,       SysTab.Buffer,
		       SysTab.BufferPos, preload,
		       CacheFlag );
   if (CallResult != ERR_FS_NO_ERROR) goto WriteError;

   if (size > 512) { q = 512; size -= 512; }
	      else { q = size; size = 0;   }

   memcpy(SysTab.Buffer+SysTab.BufferPos,buffer,q);

   CallResult =
    CacheManSaveSector( MyNode->Device, MyNode->Partition,
			SysTab.logical,1,
			MyNode->packID, CacheFlag, SysTab.Buffer);
    if (CallResult != ERR_FS_NO_ERROR) goto WriteError;
   where_pos += q; SysTab.Position +=q; SysTab.BufferPos += q;
  if (SysTab.Position > SysTab.MemNODE->Size)
   SysTab.MemNODE->Size = SysTab.Position;
  }


  if (SysTab.Position > SysTab.MemNODE->Size)
  SysTab.MemNODE->Size = SysTab.Position;
  if (CacheFlag == FPACK_NOTHING) SaveMNODE (MyNode, FPACK_NOTHING);
  BitmapDoAction(Task->OpenFiles[handle].Bitmap,MaxOpenedFiles,SetDirty);
  MyNode->Locked.Release();
  SyscallActive = 0;
  return where_pos;


WriteError :
  if (SysTab.Position > SysTab.MemNODE->Size)
  SysTab.MemNODE->Size = SysTab.Position;
  if (CacheFlag == FPACK_NOTHING) SaveMNODE (MyNode, FPACK_NOTHING);
  BitmapDoAction(Task->OpenFiles[handle].Bitmap,MaxOpenedFiles,SetDirty);
  MyNode->Locked.Release();
  Task->ErrorLevel = CallResult;
  SyscallActive = 0;
  return 0xffffffff;
}






/* ==== Read ====
     ERR_FS_NO_ERROR           - OK
     ERR_FILE_NOT_TO_READ
     ERR_NOT_VALID_FILE_HANDLE       1509
     + ostatni

     konec souboru 0;
     chyba         0xffffffff;
     jinak pocet prectenych bytu;
*/


dword fs_read(int handle, void far *buffer, dword size)
{
  TaskUArea *Task = GetCurrentTaskUArea();
  int	    MySysIndex;
  dword	    where_pos,q;
  MNODE	    *MyNode;
  word      CacheFlag, CallResult;
  int 	    eof;
  dword	    logical, preload;

  SyscallActive = 1;
  Task->ErrorLevel = ERR_FS_NO_ERROR;

  if (!Task->OpenFiles[handle].Used)
  {
    Task->ErrorLevel = ERR_NOT_VALID_FILE_HANDLE;
    SyscallActive = 0;
    return 0xffffffff;
  }

  MySysIndex = Task->OpenFiles[handle].SystemTabIndex;
  if (!(SysTab.Access & IO_CAN_READ))
  {
    Task->ErrorLevel = ERR_FILE_NOT_TO_READ;
    SyscallActive = 0;
    return 0xffffffff;
  }

  where_pos = 0; eof = 0;
  CacheFlag = (SysTab.Access & IO_CACHE_ADD) ? FPACK_ADD : FPACK_NOTHING;

  MyNode = SysTab.MemNODE;
  MyNode->Locked.Acquire();
  if (SysTab.DirtyBuffer)
  {
   CallResult = PMAP ( MyNode,           SysTab.Position,
		       logical,          MyNode->packID,
		       PMAP_READ,        SysTab.Buffer,
		       SysTab.BufferPos, preload,
		       CacheFlag );

   if (CallResult != ERR_FS_NO_ERROR) goto ReadError;
   SysTab.DirtyBuffer = 0;
  }

  if (size > 512-SysTab.BufferPos)
   {
    q = 512-SysTab.BufferPos;
    size -= (512-SysTab.BufferPos);
   }
   else
   {
    q = size;
    size = 0;
   }


  if (SysTab.MemNODE->Size < SysTab.Position + q)
  { eof = 1;
    q = SysTab.MemNODE->Size - SysTab.Position;
    if (q<=0) goto ReadEof;
  }

  memcpy((byte *)buffer,SysTab.Buffer+SysTab.BufferPos,q);
  where_pos += q; SysTab.Position +=q; SysTab.BufferPos += q;

  while ((size) && (!eof))
  {
   CallResult = PMAP ( MyNode,           SysTab.Position,
		       logical,          MyNode->packID,
		       PMAP_READ,        SysTab.Buffer,
		       SysTab.BufferPos, preload,
		       CacheFlag );

   if (CallResult != ERR_FS_NO_ERROR) goto ReadError;

   if (size > 512) { q = 512; size -= 512; }
	      else { q = size; size = 0;   }

   if (SysTab.MemNODE->Size <= SysTab.Position + q)
   { eof = 1;
     q = SysTab.MemNODE->Size - SysTab.Position;
     if (q<=0) goto ReadEof;
   }

   memcpy((byte *)buffer+where_pos,SysTab.Buffer+SysTab.BufferPos,q);
   where_pos += q; SysTab.Position +=q; SysTab.BufferPos += q;
  }


  if (eof) goto ReadEof;
ReadFinish:
  MyNode->Locked.Release();
  SyscallActive = 0;
  return where_pos;

ReadError:
  MyNode->Locked.Release();
  Task->ErrorLevel = CallResult;
  SyscallActive = 0;
  return 0xffffffff;

ReadEof:
  SysTab.Position = SysTab.MemNODE->Size;
  SysTab.BufferPos = (SysTab.Position & 511);
  MyNode->Locked.Release();
  SyscallActive = 0;
  return 0;
}



/* ==== Close ====
     ERR_FS_NO_ERROR           - OK
     ERR_NOT_VALID_FILE_HANDLE       1509
     + ostatni
*/

int fs_close(int index)
{
  TaskUArea	*Task = GetCurrentTaskUArea();
  MNODE		*MyNode;
  word		MySysIndex;
  int		i,j;

  SyscallActive = 1;
  Task->ErrorLevel = ERR_FS_NO_ERROR;

  if (!Task->OpenFiles[index].Used)
  {
    Task->ErrorLevel = ERR_NOT_VALID_FILE_HANDLE;
    SyscallActive = 0;
    return -1;
  }

  MySysIndex = Task->OpenFiles[index].SystemTabIndex;
  MyNode = OpenFiles[MySysIndex].MemNODE;

  if (OpenFiles[MySysIndex].Access & IO_CACHE_ADD)
   CacheManCommitPackage(MyNode->packID);

  ActiveNodes->Release(MyNode);

  OpenFiles[MySysIndex].Used = 0;
  delete OpenFiles[MySysIndex].Buffer;


  SystemTableSem->Down();
  BitmapClearBit(SystemOpenFilesBmp, MySysIndex);
  SystemTableSem->Up();

  if (MyNode)
  { // odmaz se z bitmap u ostatnich
    for (i=5,j=Task->NrOpenedFiles-6; i<MaxOpenedFiles; ++i)
    {
     if (!j) break;
     if ((i != index) && (Task->OpenFiles[i].Used))
     {
       if (MyNode == OpenFiles[Task->OpenFiles[i].SystemTabIndex].MemNODE)
       {
	 if (Task->OpenFiles[i].Bitmap)
	  BitmapClearBit(Task->OpenFiles[i].Bitmap, index);

       }
       j--;
     }
    }


  }


//  BitmapClearBit(Task->OpenFiles[index].Bitmap,index);
  Task->OpenFiles[index].Used = 0;
  Task->NrOpenedFiles--;
  BitmapClearBit(Task->Bmp,index);
  delete Task->OpenFiles[index].Bitmap;

  SyscallActive = 0;
  return 0;
}


/* ==== Creat ====
     ERR_FS_NO_ERROR           - OK
     ERR_DIRREC_NO_DIR         - cast cesty neni adresar
     ERR_TOO_MANY_OPENED_FILES - otevrenych souboru vice nez MaxOpenedFiles

     + ostatni
*/

int fs_creat(byte *path, word mode, word rights)
{ // otevre se pro cteni zapis
  MNODE		*DirNode;
  int		i,j,MySystemIndex;
  TaskUArea	*Task = GetCurrentTaskUArea();
  word		MyPackID, CallResult, GetNumber, MyIndex;
  byte		buffer[255],*dir,trunc;
  byte		sector[512];
  MNODE		*MyNode = (MNODE *) sector;
  dword		Node;
  word		CacheFlag;

  SyscallActive = 1;
  CacheFlag = (mode &  IO_CACHE_THROUGH) ? FPACK_NOTHING : FPACK_ADD;
  Task->ErrorLevel = ERR_FS_NO_ERROR;


  MyIndex = BitmapGetFirstHole(Task->Bmp,MaxOpenedFiles);
//  for (i=5; i<MaxOpenedFiles; ++i)
//   if (!Task->OpenFiles[i].Used)  { MyIndex = i; break; }

  if (MyIndex == 0xffff)
  {
   Task->ErrorLevel = ERR_TOO_MANY_OPENED_FILES;
   SyscallActive = 0;
   return -1;
  }

  dir = new byte [strlen(path)+1]; strcpy(dir,path);
  CutLastPathPart (dir,buffer);

  CallResult = NAMEN( dir, DirNode, CacheFlag);
  if ( CallResult != ERR_FS_NO_ERROR)
  {
    delete [] dir;
    Task->ErrorLevel = CallResult;
    SyscallActive = 0;
    return -1;
  }

  CallResult = SearchDirByName(DirNode, buffer, Node, CacheFlag, 0);
  if ( CallResult == ERR_DIRREC_NOT_FOUND)
  { // soubor jeste nexistuje, vypln jeho node
    trunc = 0;
    memset(sector,0,512);
    memcpy(MyNode->MagicWord,FileMagicWord,8);
    memcpy(MyNode->MagicWordII,FileMagicWord,8);
    MyNode->OwnerID = Task->ownerID;
    MyNode->GroupID = Task->groupID;
    MyNode->Type    = NODE_TYPE_FILE;
    MyNode->Rights  = rights;

    GlobalTime.Get(MyNode->FileAccessed);
    MyNode->FileModified = MyNode->FileAccessed;
    MyNode->NODEAccessed = MyNode->FileAccessed;
    MyNode->NODEModified = MyNode->FileAccessed;

    MyNode->Size = 0;
    MyNode->Links = 1;

    MyNode->Device = DirNode->Device;
    MyNode->Partition = DirNode->Partition;

    CallResult =
    CacheManAllocateSector( MyNode->Device,  MyNode->Partition,
			    0,               1,
			    MyNode->logical,GetNumber,
			    MyPackID,	     FPACK_CREAT);
    if (CallResult != ERR_FS_NO_ERROR)
    {
      DirNode->Locked.Release();
      ActiveNodes->Release(DirNode);
      delete [] dir;
    }
    MyNode->packID = MyPackID;
  }
  else
  if (CallResult == ERR_FS_NO_ERROR)
  { // soubor uz existuje -> zkrati se na 0
    trunc = 1;
    ActiveNodes->Add(DirNode->Device, DirNode->Partition, Node, MyNode);
  }
  else
  { // chyba pri pristupu k adresari
    DirNode->Locked.Release();
    ActiveNodes->Release(DirNode);
    delete [] dir;
  }

  // DirNode je v tabulce aktivnich nodu zamceny
  // trunc = 1 MyNode je v tabulce aktivnich nodu odemceny ->nelze ho smazat
  // trunc = 0 MyNode je vyplnen v pameti



  // pristup k systemove tabulce, zjisti prvni volnou diru
  SystemTableSem->Down();

  Task->OpenFiles[MyIndex].SystemTabIndex =
   BitmapGetFirstHole(SystemOpenFilesBmp, MaxOpenedFiles);
  if (Task->OpenFiles[MyIndex].SystemTabIndex == 0xffff)
  {
    SystemTableSem->Up();
    DirNode->Locked.Release();
    ActiveNodes->Release(DirNode);
    if (trunc)
    {
     ActiveNodes->Release(MyNode);
    }
    Task->ErrorLevel = ERR_TOO_MANY_OPENED_FILES;
    delete [] dir;
    SyscallActive = 0;
    return -1;
  }
  BitmapSetBit(SystemOpenFilesBmp, Task->OpenFiles[MyIndex].SystemTabIndex);
  SystemTableSem->Up();

  MySystemIndex = Task->OpenFiles[MyIndex].SystemTabIndex;

  if (trunc)
  { // soubor se urizne na velikost 0
    CallResult = DeleteMNODEData(MyNode, CacheFlag);
    if (CallResult != ERR_FS_NO_ERROR)
    {
     DirNode->Locked.Release();
     ActiveNodes->Release(DirNode);
     ActiveNodes->Release(MyNode);
     // obnov bitmapu systemove tabulky
     SystemTableSem->Down();
     BitmapClearBit(SystemOpenFilesBmp, MySystemIndex);
     SystemTableSem->Up();
     Task->ErrorLevel = CallResult;
     delete [] dir;
     SyscallActive = 0;
     return -1;
    }
   CacheManCommitPackage(MyNode->packID);
  }
  else
  { // ulozi se node a zapise se do direktorare
    CallResult =
     CacheManSaveSector(MyNode->Device, MyNode->Partition,
			MyNode->logical, 1,
			MyPackID,       FPACK_ADD /*CacheFlag*/,
			MyNode);
    if (CallResult != ERR_FS_NO_ERROR)
    {
     DirNode->Locked.Release();
     ActiveNodes->Release(DirNode);
     // obnov bitmapu systemove tabulky
     SystemTableSem->Down();
     BitmapClearBit(SystemOpenFilesBmp, MySystemIndex);
     SystemTableSem->Up();
     Task->ErrorLevel = CallResult;
     delete [] dir;
     SyscallActive = 0;
     return -1;
    }

   // pridej do adresare zaznam o novem souboru
   CallResult = AddDirectoryEntry( DirNode, MyNode->logical,
				   CacheFlag, buffer, 0);
    if (CallResult != ERR_FS_NO_ERROR)
    {
     DirNode->Locked.Release();
     ActiveNodes->Release(DirNode);
     // obnov bitmapu systemove tabulky
     SystemTableSem->Down();
     BitmapClearBit(SystemOpenFilesBmp, MySystemIndex);
     SystemTableSem->Up();
     Task->ErrorLevel = CallResult;
     // uvolni sektor, ktery je naalokovan pro node
     CacheManFreeSector(MyNode->Device, MyNode->Partition,
			MyNode->logical, 1,
			MyPackID,        FPACK_ADD/*CacheFlag*/);
     delete [] dir;
     SyscallActive = 0;
     return -1;
    }
    CacheManCommitPackage(MyNode->packID,FPACK_DELETE);
    ActiveNodes->Add(MyNode->Device,  MyNode->Partition,
		     MyNode->logical, MyNode);
  }

  // MyNode je v tabulce aktivnich uzlu odemceny


  OpenFiles[MySystemIndex].Used = 1;
  OpenFiles[MySystemIndex].Access = IO_CAN_BOTH;
  OpenFiles[MySystemIndex].Access |=
   (CacheFlag == FPACK_NOTHING) ? IO_CACHE_THROUGH : IO_CACHE_ADD;
  OpenFiles[MySystemIndex].Position = 0;
  OpenFiles[MySystemIndex].DirtyBuffer = 1;
  OpenFiles[MySystemIndex].BufferPos = 0;
  OpenFiles[MySystemIndex].Buffer = new byte[512];
  OpenFiles[MySystemIndex].LastPreload = 0;
  OpenFiles[MySystemIndex].MemNODE = MyNode;


  Task->OpenFiles[MyIndex].Bitmap = new byte[(MaxOpenedFiles>>3)+1];
  memset(Task->OpenFiles[MyIndex].Bitmap,0,(MaxOpenedFiles>>3)+1);

  // podiva se jestli uz proces neotevrel stejny soubor,
  // pokud ano do bitmapy si zanese jeho indexy a svuj index
  // zanese do jeho bitmapy (pokud nejakou ma)
  if (trunc)
   for (i=5,j=Task->NrOpenedFiles-5; i<MaxOpenedFiles; ++i)
   {
    if (!j) break;
    if ((i != MyIndex) && (Task->OpenFiles[i].Used))
    {
      if (MyNode == OpenFiles[Task->OpenFiles[i].SystemTabIndex].MemNODE)
      {
	if (Task->OpenFiles[i].Bitmap)
	 BitmapSetBit(Task->OpenFiles[i].Bitmap, MyIndex);
	 BitmapSetBit(Task->OpenFiles[MyIndex].Bitmap, i);
      }
      j--;
    }
   }

  Task->OpenFiles[MyIndex].Used = 1;
  Task->NrOpenedFiles++;
  BitmapSetBit(Task->Bmp,MyIndex);
  DirNode->Locked.Release();
  ActiveNodes->Release(DirNode);


  delete [] dir;
  SyscallActive = 0;

  return MyIndex;

}


/* ==== Unlink ====

     ERR_FS_NO_DIRECTORY   - DirNode neni adresar
     ERR_DIRREC_NOT_FOUND  - zaznam se jmenem name nenalezen
     ERR_NON_EMPTY_DIR     - mazani neprazdneho adresare
     ERR_SHARING_VIOLATION - nekdo jiny pracuje se souborem
     ERR_DIRREC_NO_DIR     - nelze nalezt adresar, ve kterem se ma mazat
     + ostatni
*/
word fs_unlink(byte *path)
{
  byte		buffer[255],*dir;
  MNODE		*MemNode, *DirNode;
  word		CallResult;

  SyscallActive = 1;
  // alokuju, protoze nemuzu uzivateli vratit zmeneny retezec a delka
  // cesty muze byt obecne libovolne dlouha
  dir = new byte [strlen(path)+1];
  strcpy(dir,path);

  // zjisti umisteni adresare, kde se bude mazat
  CutLastPathPart (dir,buffer);

  CallResult = NAMEN( dir, DirNode, FPACK_ADD);
  if ( CallResult != ERR_FS_NO_ERROR)
  {
    delete [] dir;
    SyscallActive = 0;
    return CallResult;
  }
  DirNode->Locked.Release();

  CallResult = NAMEN( path, MemNode, FPACK_ADD);
  if ( CallResult != ERR_FS_NO_ERROR)
  {
    delete [] dir;
    ActiveNodes->Release(DirNode);
    SyscallActive = 0;
    return CallResult;
  }
  MemNode->Locked.Release();

  CallResult = ActiveNodes->Unlink(MemNode, DirNode, buffer, FPACK_ADD);
  if (MemNode) ActiveNodes->Release(MemNode);
  ActiveNodes->Release(DirNode);

  delete [] dir;
  SyscallActive = 0;
  return CallResult;
}


/* ==== Make Dir ====

   vraci ERR_FS_NO_ERROR
	 ERR_DIRREC_NO_DIR
	 ERR_DIRREC_ALREADY_EXISTS
	 ERR_CANNOT_CREATE_DIR
	 + ostatni
*/
word fs_mkdir(byte *path)
{
  byte		buffer[255],*dir;
  byte  	sector2[1024], *DataTag = sector2+512;
  MNODE 	*MemNode;
  word		CallResult,GetNumber,packID;
  dword 	logical;
  NODE		*rnode = (NODE *)sector2;
  FileRecord	*frecord;
  TaskUArea	*Task;

  SyscallActive = 1;
  if (!*path)
  {
    SyscallActive = 0;
    return ERR_CANNOT_CREATE_DIR;
  }
  #ifdef DEBUG
    printf ("\nCreating new directory node");
  #endif


  Task = GetCurrentTaskUArea();

  // alokuju, protoze nemuzu uzivateli vratit zmeneny retezec a delka
  // cesty muze byt obecne libovolne dlouha
  dir = new byte [strlen(path)+1];
  strcpy(dir,path);

  // zjisti umisteni adresare, kde se bude novy adresar vytvaret
  CutLastPathPart (dir,buffer);
  CallResult = NAMEN( dir, MemNode, FPACK_ADD);
  if ( CallResult != ERR_FS_NO_ERROR)
  {
    delete [] dir;
    SyscallActive = 0;
    return CallResult;
  }
  if (!(MemNode->Type & NODE_TYPE_DIRECTORY))
  {
    MemNode->Locked.Release();
    ActiveNodes->Release(MemNode);
    delete [] dir;
    SyscallActive = 0;
    return ERR_NOT_DIRECTORY;
  }

  CallResult = SearchDirByName(MemNode, buffer, logical, FPACK_ADD, 0);
  if (CallResult != ERR_DIRREC_NOT_FOUND)
  {
    MemNode->Locked.Release();
    ActiveNodes->Release(MemNode);
    delete [] dir;
    SyscallActive = 0;
    if (CallResult == ERR_FS_NO_ERROR) return ERR_DIRREC_ALREADY_EXISTS;
				  else return CallResult;
  }

  // naalokuj sektor pro adresarova data
  CreatePackage(MemNode->Device,  MemNode->Partition, packID );
  CallResult = CacheManAllocateSector(MemNode->Device,  MemNode->Partition,
				      MemNode->logical, 2,
				      logical,		GetNumber,
				      packID,		FPACK_ADD);
  if ( CallResult != ERR_FS_NO_ERROR) goto mkdirbad;
  #ifdef DEBUG
    printf ("\n Sectors <%d,%d,%lu> allocated for node, <%d,%d,%lu> for data",
	    MemNode->Device, MemNode->Partition, logical,
	    MemNode->Device, MemNode->Partition, logical+1);
  #endif


  memset(sector2,0,1024);

  memcpy(rnode->MagicWord, DirMagicWord, 8);
  memcpy(rnode->MagicWordII, DirMagicWord, 8);

  rnode->OwnerID = Task->ownerID;
  rnode->GroupID = Task->groupID;
  rnode->Rights  = 0xffff;	// not implemented yet
  rnode->Type    = NODE_TYPE_DIRECTORY;
  rnode->Size    = 512;
  rnode->Links   = 1;

  rnode->DirectBlock[0] = logical+1;

  GlobalTime.Get(rnode->FileAccessed);
  rnode->FileModified = rnode->FileAccessed;
  rnode->NODEAccessed = rnode->FileAccessed;
  rnode->NODEModified = rnode->FileAccessed;

  // init data block
  frecord = (FileRecord *)DataTag;

  frecord->Node = logical;
  frecord->NextRecord = 8;
  frecord->NameLength = 1;
  frecord->FileName[0] = '.';

  DataTag += frecord->NextRecord;
  frecord = (FileRecord *)DataTag;
  frecord->Node = MemNode->logical;
  frecord->NextRecord = 0;
  frecord->NameLength = 2;
  frecord->FileName[0] = '.';
  frecord->FileName[1] = '.';

  CallResult = AddDirectoryEntry( MemNode, logical, FPACK_ADD, buffer, 0);
  if ( CallResult != ERR_FS_NO_ERROR)
  { // odalokuju sektor
    CallResult = CacheManFreeSector( MemNode->Device,  MemNode->Partition,
				     logical,		 2,
				     packID,	       FPACK_ADD);
    goto mkdirbad;
  }

  CallResult = CacheManSaveSector(MemNode->Device,  MemNode->Partition,
				  logical,	    2,
				  packID,           FPACK_ADD,
				  sector2);
  if ( CallResult != ERR_FS_NO_ERROR)
  { // odalokuju sektor
    CallResult = CacheManFreeSector( MemNode->Device,  MemNode->Partition,
				     logical,		 2,
				     packID,	       FPACK_ADD);
    goto mkdirbad;
  }


  CacheManCommitPackage(packID,FPACK_DELETE);
  CacheManCommitPackage(MemNode->packID);
mkdirbad:
  delete [] dir;
  MemNode->Locked.Release();
  ActiveNodes->Release(MemNode);
  SyscallActive = 0;
  return CallResult;
}


/* ==== Change Root ====

    ERR_FS_NO_ERROR
    ERR_DIRREC_NOT_FOUND

*/

word fs_chroot(byte *path)
{
  word CallResult;
  MNODE *MemNode;
  TaskUArea *Task = GetCurrentTaskUArea();

  SyscallActive = 1;
  MakeNiceString(path);
  CallResult = NAMEN( path, MemNode, FPACK_ADD, 1);
  if (CallResult != ERR_FS_NO_ERROR)
  {
    SyscallActive = 0;
    return ERR_DIRREC_NOT_FOUND;
  }

  // uvolni predchozi root a nastav MemNode tasku jako roota
  ActiveNodes->Release(Task->RootNode);

  Task->RootNode = MemNode;
  MemNode->Locked.Release();

  fs_chgdir(path);

  SyscallActive = 0;
  return CallResult;
}




/* ==== Change dir =====
    ERR_FS_NO_ERROR
    ERR_DIRREC_NOT_FOUND
    ERR_NOT_DIRECTORY;
*/


word fs_chgdir(byte *path)
{
  TaskUArea *Task = GetCurrentTaskUArea();
  word CallResult;
  MNODE *MemNode;

  SyscallActive = 1;
  // prazdny retezec zmeni do rootu
  if (!*path)
  {
    ActiveNodes->Release(Task->ActiveNode);
    ActiveNodes->Add( RootMNODE->Device, RootMNODE->Partition,
		      RootMNODE->logical, Task->ActiveNode);

  }
  else
  {
    CallResult = NAMEN( path, MemNode, FPACK_ADD);
    if (CallResult != ERR_FS_NO_ERROR)
    {
     SyscallActive = 0;
     return CallResult;
    }
    if (!(MemNode->Type & NODE_TYPE_DIRECTORY))
    {
      MemNode->Locked.Release();
      ActiveNodes->Release(MemNode);
      SyscallActive = 0;
      return ERR_NOT_DIRECTORY;
    }
    if (Task->ActiveNode != MemNode)
    {
      ActiveNodes->Release(Task->ActiveNode);
      Task->ActiveNode = MemNode;
    }
    MemNode->Locked.Release();
  }
  SyscallActive = 0;
  return ERR_FS_NO_ERROR;
}

word fs_rename(byte *oldpath, byte *newpath)
{
 return ERR_FS_NO_ERROR;
}

// --- programmer's functions -------------------------------------------

word FSFindFirst(byte *dir, byte *dir_record, dword &next_position)
{

  MNODE *DirNode;
  word CallResult;
  dword node;

  CallResult = NAMEN(dir, DirNode, FPACK_ADD);
  if (CallResult != ERR_FS_NO_ERROR) return CallResult;

  if (!(DirNode->Type & NODE_TYPE_DIRECTORY))
  {
    DirNode->Locked.Release();
    ActiveNodes->Release(DirNode);
    return FS_ERR_DIR_LASTRECORD;
  }

  CallResult = FindFirst(DirNode, dir_record, node, next_position, 0);
  DirNode->Locked.Release();
  ActiveNodes->Release(DirNode);
  return CallResult;
}




word FSFindNext(byte *dir, byte *dir_record, dword &position,
	      dword &next_position)
{
  MNODE *DirNode;
  word CallResult;
  dword node;

  CallResult = NAMEN(dir, DirNode, FPACK_ADD);
  if (CallResult != ERR_FS_NO_ERROR) return CallResult;

  if (!(DirNode->Type & NODE_TYPE_DIRECTORY))
  {
    DirNode->Locked.Release();
    ActiveNodes->Release(DirNode);
    return FS_ERR_DIR_LASTRECORD;
  }

  CallResult = FindNext(DirNode, dir_record, position, node,
                         next_position, 0);
  DirNode->Locked.Release();
  ActiveNodes->Release(DirNode);
  return CallResult;
}

dword FSFileSize(int handle)
{
  TaskUArea *Task = GetCurrentTaskUArea();
  int	    MySysIndex;

  if (!Task->OpenFiles[handle].Used)
  {
    Task->ErrorLevel = ERR_NOT_VALID_FILE_HANDLE;
    return 0xffffffff;
  }

  MySysIndex = Task->OpenFiles[handle].SystemTabIndex;
  return OpenFiles[MySysIndex].MemNODE->Size;
}

// ----- montovani -----


// vraci -1, pokud neuspeje, jinak index do montovaci tabulky
word GetMntPoint(byte *buffer, byte Device, byte Partition)
{
  int i;

  for(i=0; i<MountUsed; ++i)
  {
    if ((MountTab[i].Used) && (MountTab[i].Used))
     if ( (MountTab[i].MntRootDevice == Device) &&
	  (MountTab[i].MntRootPartition == Partition) ) break;
  }

  if (i >= MountUsed) return -1;

  strcpy(buffer, MountTab[i].MntPoint);
  return i;

}

// vraci -1, pokud neuspeje, jinak index do montovaci tabulky
word GetMntSystem(byte *buffer, byte &Device, byte &Partition)
{
  int i;

  for(i=0; i<MountUsed; ++i)
  {
    if ((MountTab[i].Used) && (MountTab[i].Used))
     if (!strcmp(buffer,MountTab[i].MntPoint)) break;
  }

  if (i >= MountUsed) return -1;

  Device = MountTab[i].MntRootDevice;
  Partition = MountTab[i].MntRootPartition;

  return i;
}


word CreateMntTab(byte *path)
{
  int   handle,eof=1;
  byte  buffer[1024],*tag;
  int   i;

  handle = fs_open(path,IO_CAN_READ | IO_CACHE_THROUGH);
  if (handle == -1) return ERR_FSTAB_NOT_FOUND;

  MountTab = new MntTabItem[MountTabSize];
  memset(MountTab, 0 ,sizeof(MntTabItem)*MountTabSize);

/*
struct MntTabItem
{
  byte Used;
  byte Mounted;

  byte *MntPoint;
  byte MntPointDevice, MntPointPartition, MntPointLogical;
  byte MntRootDevice,  MntRootPartition, MntRootLogical;
};
*/
  i = 0; tag = buffer;
  while(eof)
  {
    do { eof = fs_read(handle,tag,1); } while ((*tag++ != '-') && (eof));
    *--tag = 0;

    MountTab[i].MntPoint = new byte[strlen(buffer)+1];
    strcpy(MountTab[i].MntPoint,buffer);

    tag = buffer;
    do { eof = fs_read(handle,tag,1); } while ((*tag++ != '-') && (eof));
    *--tag = 0;
    MountTab[i].MntRootDevice = atol(buffer);

    tag = buffer;
    do { eof = fs_read(handle,tag,1); } while ((*tag++ != '-') && (eof));
    *--tag = 0;
    MountTab[i].MntRootPartition = atol(buffer);

    MountTab[i++].Used = 1;


    tag = buffer;
    do { eof = fs_read(handle,tag,1); } while (isspace(*tag) && (eof));
    tag++;
  }

 MountUsed = i;
 fs_close(handle);
 return ERR_FS_NO_ERROR;
}
