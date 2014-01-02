#ifndef __STRIPING.H
 #define __STRIPING.H


 #include "fstypes.h"
 #include "fserrors.h"
 #include "cachesup.h"

 //- types and structures -------------------------------------------------------------

 #define PARITY_STRIP	11
 #define DATA_STRIP	12

 #define ACTIVE         1
 #define NONACTIVE      0



 // STRIPING TYPES AND STRUCTURES ARE DECLARED IN CACHESUP.H
 // 		AND IMPLEMENTED IN CACHESUP.CPP



 //- functions -------------------------------------------------------------

 word InitializeStriping( void far *DirtyDataPartys );

 word CLASSICWriteLogicalSector( byte Device, dword Logical, void far *Buffer, byte Number=1 );
 word CLASSICReadLogicalSector ( byte Device, dword Logical, void far *Buffer, byte Number=1 );

 word STRIPUpdateParityParty( void );
 word STRIPFastUpdateParityParty( void );

 word STRIPFastRescueParty( byte DestDevice, byte DestParty );
 word STRIPRescueParty( byte DestDevice, byte DestParty );

 word STRIPWriteLogicalSector( byte Device, dword Logical, void far *Buffer, byte Number=1 );
 word STRIPReadLogicalSector ( byte Device, dword Logical, void far *Buffer, byte Number=1 );

 word WriteLogicalSector( byte Device, dword Logical, void far *Buffer, byte Number=1 );
 word ReadLogicalSector ( byte Device, dword Logical, void far *Buffer, byte Number=1 );

 word ShutdownStriping( void );

#endif










