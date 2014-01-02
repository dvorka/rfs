
#ifndef __SYSCALLS_H
 #define __SYSCALLS_H


 #include "fstypes.h"
 #include "fdisk.h"
 #include "format.h"

#define IO_CAN_WRITE	 1
#define IO_CAN_READ      2
#define IO_CAN_BOTH      3
#define IO_CACHE_ADD     4
#define IO_CACHE_THROUGH 8
#define IO_APPEND	 16
#define IO_CREAT	 32
#define IO_TRUNC	 64


#define LSEEK_START	 0
#define LSEEK_CURRENT    1
#define LSEEK_END        2


// vraci ERR_FS_NO_ERROR
//       ERR_FS_NO_DIRECTORY

word FSFindFirst(byte *dir, byte *dir_record, dword &next_position);

// vraci ERR_FS_NO_ERROR
//       ERR_FS_NO_DIRECTORY
//       FS_ERR_DIR_LAST_RECORD
word FSFindNext(byte *dir, byte *dir_record, dword &position,
		dword &next_position);

// vraci velikost souboru, 0xffffffff je chyba a pricina je
// ulozena v Task->ErrorLevel
dword FSFileSize(int handle);


// pro zadane device a partition vrati string z montovaci tabulky
word GetMntPoint(byte *buffer, byte Device, byte Partition);
// pro zadany mntpoint vrati device a partition, pres ktere se mountuje
word GetMntSystem(byte *buffer, byte &Device, byte &Partition);
// pouzije se pri startu, vytvori montovaci tabulku
word CreateMntTab(byte *path);


//- My syscalls -----------------------------------------------------------------------------
// navratove hodnoty jsou popsane v syscalls.cpp

 word fs_mkdir(byte *path);
 word fs_chroot(byte *path);
 word fs_chgdir(byte *path);
 word fs_unlink(byte *path);
 word fs_link(byte *path1, byte *path2);
 int  fs_creat(byte *path, word mode = FPACK_ADD, word rights = 0xffff);
 int  fs_open(byte *path, word mode, word rights = 0xffff);
 int  fs_close(int index);
 dword fs_read(int handle, void far *buffer, dword size);
 dword fs_write(int handle, void far *buffer, dword size);
 dword fs_lseek(int handle, dword offset, word origin);
 word fs_unmount (byte *path);
 word fs_mount (byte *path);
 word fs_rename(byte *oldpath, byte *newpath);

// =======================================================================


#endif

