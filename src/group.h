#ifndef __GROUP_H
 #define __GROUP_H

 #include <alloc.h>
 #include <conio.h>

 #include "fstypes.h"
 #include "hardware.h"


 #define GROUP_SIZE_SEC (512lu*8lu+1lu)
 #define SECS_IN_GROUP  (512lu*8lu)


 word CreateNrOfGroups( byte DeviceID,  dword FirstSector, dword Sectors,
			dword &Created, dword &TotalSectors,
			dword  &GroupStatisticSectors );

 word LoadBitmap( byte Device, byte PartyID, dword GroupID,
		  MasterBootRecord *BR, void far *Bitmap
		);
 // loads bitmap GroupID


 word SaveBitmap( byte Device, byte PartyID, dword GroupID,
		  MasterBootRecord *BR, void far *Bitmap );
 // saves bitmap GroupID


#endif
