#ifndef __CACHEMAN_H
 #define __CACHEMAN_H


 #include "cachesup.h"
 #include "fdisk.h"
 #include "fstypes.h"
 #include "group.h"
 #include "recovery.h"
 #include "string.h"

 // get new ID
 #define FPACK_CREAT    0x01
 // already exists
 #define FPACK_ADD      0x02
 // in commit
 #define FPACK_DELETE   0x04
 // does nothing
 #define FPACK_NOTHING  0x08


 // CACHEMAN ERRORS ( for other error codes see FSERROS.H )
 #define ERR_FS_BADSECTOR_DATAOK	700
 // data was rescued but this area on HD should be no longer used
 #define ERR_FS_OPERATION_FAILED	701
 // read  failed  => data wasn't read
 // write failled => data wasn't written
 #define ERR_FS_PACKAGE_NOT_EXIST	702
 // package you want to use do not exist ( returned by rd/wr/commit )
 #define ERR_FS_LOG_NOT_CREATED		703
 // in commit log must be created, without log wouldn't be consistent
 // => package was not commited
 #define ERR_FS_LOG_OPEN		704
 #define ERR_FS_LOG_WR			704
 #define ERR_FS_LOG_RD			705
 #define ERR_FS_NO_SPACE_LEFT		706


 word CacheManInit( dword MemForData=64000u,
		    bool  CachingON =TRUE,
		    bool  SwappingON=TRUE );
 // MemForData: how many bytes can cache allocate for data
 // iCachingON and iSwappingON: ON/OFF caching/swapping

 word CacheManDeinit( void );
 // frees service structures allocated by CacheMan

 word CacheManCommitPackage( word PackageID, word Flag=FPACK_NOTHING );
 // - list of sectors which should be commited to disc
 // - packages are usually deleted here
 // ! PutSector DOESN'T write sector into file, but writes it to	!
 // ! cache swap or stays in memory with dirty flag. Only and ONLY      !
 // ! THIS FUNCTION CAN WRITE DATA DIRECTLY TO DISC.                    !
 // - FPACK_NOTHING, FPACK_DELETE

 word CacheManCompleteCommit( word Flag=FPACK_NOTHING );
 // - writes whole content of cache to HD
 // - FPACK_NOTHING, FPACK_DELETE

 word CacheManAllocateSector( byte  Device,     byte Party,
			      dword Previous,   word Number,
			      dword &Logical,   word &GetNumber,
			      word  &PackageID, word Flag
			    );
 // - allocates new block of sectors, returns first sector logical number
 //   and lenght of block successfuly allocated
 // - previous==0 => means don' wanna use this param
 // - FPACK_CREATE, FPACK_ADD, FPACK_NOTHING


 word CacheManFreeSector( byte  Device,     byte Party,
			  dword Logical,    word Number,
			  word  &PackageID, word Flag
			);
 // - returns sectors to system
 // - FPACK_CREATE, FPACK_ADD, FPACK_NOTHING

 word CacheManLoadSector( byte  Device,     byte Party,
			  dword Logical,    word Number,
			  word  &PackageID, word Flag,   void far *Buffer );
 // - loads block of sectors
 // - FPACK_CREATE, FPACK_ADD, FPACK_NOTHING

 word CacheManSaveSector( byte  Device,    byte Party,
			  dword Logical,   word Number,
			  word &PackageID, word Flag,    void far *Buffer );
 // - saves block of sectors
 // - FPACK_CREATE, FPACK_ADD, FPACK_NOTHING

 word CacheManLoadToCache( byte  Device,     byte Party,
			   dword Logical,    word Number,
			   word  &PackageID, word Flag    );
 // - loads block of sectors to cache
 // - FPACK_CREATE, FPACK_ADD

 word SaveSectorThrough( byte  Device,  byte Party,
			 dword Logical, word Number,     void far *Buffer );
 // - writes logical sector(s) to party, logical nr counted from bg of party
 // - writes only upto 128 sectors==65535 bytes!

 word LoadSectorThrough( byte Device,   byte Party,
			 dword Logical, word Number,     void far *Buffer );
 // - reads logical sector(s) to party, logical nr counted from bg of party
 // - reads only upto 128 sectors==65535 bytes!

#endif
