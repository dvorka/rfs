#ifndef __HARDWARE_H
 #define __HARDWARE_H


 #include <alloc.h>
 #include <dos.h>
 #include <fcntl.h>
 #include <io.h>
 #include <mem.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <sys\stat.h>


 #include "fserrors.h"
 #include "fstypes.h"
 #include "init.h"
 #include "striping.h"


 word Interrupt(
                 byte IntNum,
                 word &AX,
                 word &BX,
                 word &CX,
                 word &DX,
                 word &ES,
                 word &Flags
               );

word GetHardParameters( byte DeviceID,
                        word &Cylinders, word &TrackSectors, word &Heads );

 word WritePhysicalSector( word howmany,  word sector, word surface,
                           word cylinder, void *bufptr, word jednotka );

 word ReadPhysicalSector( word howmany,  word sector, word surface,
                          word cylinder, void *bufptr, word jednotka );

 void  SectorLToP( dword Logical,   word TrackSectors, word Surfaces,
                   word  &Physical, word &Surface,     word &Cylinder );

 dword SectorPToL( dword Physical, dword TrackSectors, dword Surfaces,
                   dword Surface,  dword Cylinder );

 word VerifyLogicalSector ( byte Device, dword Logical, byte Number=1 );

 void Beep( void );

 void KeybWait( void );

 void KeybClear( void );

#endif
