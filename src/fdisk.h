#ifndef __FDISK_H
 #define __FDISK_H


 #include "fserrors.h"
 #include "fstypes.h"
 #include "hardware.h"
 #include "striping.h"

 word LoadMasterBoot( byte Device, MasterBootRecord *pMBR );

 word SaveMasterBoot( byte Device, MasterBootRecord *pMBR );

 word LoadBoot( byte Device, byte PartyID, MasterBootRecord *pBR );

 word SaveBoot( byte Device, byte PartyID, MasterBootRecord *pBR );

 word GetPartyFreeSpace( byte Device, byte Party, dword &FreeSpaceSec );
  // - free space is in sectors

 word SetPartyFreeSpace( byte Device, byte Party, dword FreeSpaceSec );
  // - free space is in sectors

 word GetPartyLabel( byte Device, byte Party, char *Label );
  // - space for string is allocated by user

 word SetPartyLabel( byte Device, byte Party, char *Label );
  // - space for string allocated by caller

 word GetGroupFreeSpace( byte Device, byte Party, dword GroupID,
			 word &FreeSpaceSec );
  // - free space is in sectors

 word SetGroupFreeSpace( byte Device, byte Party, dword GroupID,
			 word FreeSpaceSec );
  // - free space is in sectors

 word FSCreatMasterBoot(
			 byte Device=0x80,
			 word SectorSize=512,
			 byte ClusterSectors=1
		       );

 word FSCreatPartition(
			byte Device,
			byte PartyID,
			word FirstCylinder, word LastCylinder,
			byte Active=0x00
		       );

 #define NONACTIVE_PARTITION 	0x00
 #define ACTIVE_PARTITION 	0x80


#endif