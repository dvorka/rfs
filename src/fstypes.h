#ifndef __FSTYPES_H
 #define __FSTYPES_H

 //- HINTS ------------------------------------------------------------------
 //
 // - partitions IDs:		  	1..4
 // - logical sectors adressing:  	0...
 // - PHYSICAL addresing:
 //    surfaces 0..
 //    tracks   0..
 //    sectors  1..
 //
 // - diskette has only one BR, ( no MBR )
 //- Common types ------------------------------------------------------------------

 typedef unsigned char bool;
 typedef unsigned char byte;
 typedef unsigned int  word;
 typedef unsigned long dword;


 #define TRUE  1
 #define FALSE !TRUE

 //- Hardware ------------------------------------------------------------------

 // bit in Flag register 15..0
 // use it as index to BitMask defined in fstypes.cpp
 #define FLAG_CF   0
 #define FLAG_PF   2
 #define FLAG_AF   4
 #define FLAG_ZF   6
 #define FLAG_SF   7
 #define FLAG_TF   8
 #define FLAG_IF   9
 #define FLAG_DF  10
 #define FLAG_OF  11


 //- FDisk -----------------------------------------------------------------------


 #define ACTIVE		 1
 #define NONACTIVE       0

 #define CLUSTER4K       8
 #define CLUSTER8K       16

 #define PARTY_IS_DIRTY  1
 #define PARTY_IS_CLEAN  0

 // magic word can be 8 bytes long ( 7bytes + \0)
 #define RFS_MAGIC_WORD_DATA_PARTY	"RFSData"
 #define RFS_MAGIC_WORD_SWAP_PARTY      "RFSSwap"

 // for format
 #define RFS_DATA	 1
 #define RFS_SWAP        0

 // open file system
 #define CACHE_ON    	 1
 #define CACHE_OFF   	 0
 #define SWAP_ON     	 1
 #define SWAP_OFF    	 0

 #define NR_OF_DEVICES 5

 //- Boot and Master Boot -----------------------------------------------------------------------

 typedef struct
 {
  byte  ActiveArea;		 // non active 0x00, actice 0x80
				 // only one can be active => OS is loaded
				 // using loader of active area
  byte  BeginSurface;            //
  word  BeginCylinderAndSector;  // ccccccccCcSsssss 10c+6s, sector should be 1
  byte  OSFlag;                  // id of OS, 0x50 RDONLY, 0x51 RDWR
  byte  EndSurface;
  word  EndCylinderAndSector;
  dword FirstSectorOff;          // relative from Master Boot Record
  dword SectorNum;               // active area has < sectors then other
				 // because contains MBR
 }
 PartyTableItem;


 typedef struct
 {
  PartyTableItem Party[4];
  word		     ID;	 // == AA55h
 }
 PartyTable;		 	 // 1. sector addr 1BE, size==66


 typedef struct
 {
  byte jmp[3];
 }
 JumpToOSLoader;


 typedef struct
 {
  byte ID[8];			 // something like 'IBM  3.0'
 }
 MBROSID;


 typedef struct
 {
  word  SectorSize; 	// 200h=512
  byte  ClusterSectors; // number of sectors in cluster 8 or 16
  word  ReservedSectors;// number of reserved sectors ( for ex. 1=boot )
  byte  CATCopies;	// nr of cat copies 1
  word  RootRecords;	// nr of records in root dir
  word  OldSectors;     // nr of sectors for partitions>32M => 0, use next
  byte  MediaDescr;	// ID, 0xF8 means fixed disc
  word  SizeOfCat;	// size of CAT in sectors
  word  TrackSectors;	// nr of sectors in one track
  word  Surfaces;	// nr of surfaces
  dword HiddenSectors;	// nr of hidden sectors = nr of previous sectors
  dword TotalSectors;	// total number of sectors on partition/disc

  // rest 10B
  dword RootFNODEAddr;  // 4
  dword NrOfGroups;     // 4
  byte  DirtyFlag;      // 1 FALSE if partition was unmounted OK

  byte  reserved[1];    // i can use it for something
			// - addr of fnode of root dir 4b
			// - NrOfGroupsInPart 4b

 }
 BIOSParameterBlock;    // size = 35;


 typedef struct
 {
  byte 		begin[400];
 }
 OSLoaderCode;				// size 400


 typedef struct
 {
  JumpToOSLoader     OSJmp;
  MBROSID	     OSID;
  BIOSParameterBlock BPB;
  OSLoaderCode	     Code;       	// begin off=46
					// contains partab and OS id
  PartyTable     PartyTable;  	        // in MBR offset 446=1BEh, size 66
 }
 MasterBootRecord;			// in sector 0, size 512B


 //- Format structures -----------------------------------------------------------------------

 typedef struct
 {
  word  MagicBegin;			// 0xCDAB ( for chkdsk if boot bad )
  char  Label[256];		        // label of partition ( C string )
  dword FreeSecs;			// free sectors in partition
  dword StatSectors;                    // nr of sectors for grp statistic

  MBROSID	     OSID;
  BIOSParameterBlock BPB;		// duplicated from BR

  byte  Reserved[240-35-8];		// - sizeof(BPB) - sizeof(OSID)

  word  MagicEnd;			// 0xCDAB ( for chkdsk if boot bad )
 }
 TheSecondBoot;			        //  sizeof() == 512 == 1 sector


 //- System informations -----------------------------------------------------------------------


 // active HD of proces, contains all important informations
 struct ActiveDevice
 {
  byte 		   Device;	// 0x80..0x84  ( AND 0x0F and is 0..4 (array))
  int  		   DevHandle;
  MasterBootRecord MBR;	        // MBR of device
  byte 		   Party;	// 0..3
  MasterBootRecord BR;	        // BR of partition
  char             *Path;       // string path
  dword            *FNodePath;	// array of sector numbers
 };

 //- struct DeviceTabEntry ------------------------------------------------------------------------

 struct DeviceTabEntry
 {
  bool Presented;
  word Cylinders;     	//      0..1023
  byte Sectors;       	// 	0..63
  byte Heads; 		//	0..255
 }; 			//      size 5

 #define DEVICE_ENTRY_LNG 5

 //- EOF ------------------------------------------------------------------------


#endif

