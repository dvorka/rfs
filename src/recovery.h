#ifndef __RECOVERY_H
 #define __RECOVERY_H


 #define UNLINK_LOGS


 #include <dir.h>

 #include "cachesup.h"
 #include "fdisk.h"
 #include "fstypes.h"

 //- OKA log --------------------------------------------

 #define ALLOC_RECORD      0xAAAAAAAAlu
 #define FREE_RECORD       0xEEEEEEEElu

 #define NORMAL_STATE      0xBBBBBBBBlu
 #define ROLLBACKED_STATE  0x77777777lu

 #define OKA_RECORD_END    0xFFFFFFFFlu

 typedef struct
 {
  dword  Record_Beg;	// ALLOC_RECORD || FREE+RECORD
   dword  State;        // NORMAL_STATE || ROLLBACKED_STATE
   dword  Logical;
   word   Number;
   dword  GroupID;
   dword  LogicalInBmp;
  dword  Record_End;    // OKA_RECORD_END
 }
 OkaRecord;			// sizeof()=26

 #define OKA_RECORD_LNG 26u

 //------------------------------------------------------

 #define CALLER_IS_ALLOC 	FALSE
 #define CALLER_IS_FREE  	TRUE

 #define WRITE_OKA  		TRUE
 #define DO_NOT_WRITE_OKA  	TRUE


 word AllocFreeUsingLog( byte   Device,
			 byte   Party,

			 dword  GroupID,
			 dword  Logical,
			 dword  LogicalInBmp,
			 word   Number,

			 word   PackageID,

			 MasterBootRecord  *BR,
			 void              far *Bitmap,
			 bool              FreeCalls=FALSE,
			 bool   	   WriteOka=FALSE
		       );

 word UndoAlfLog( byte Device, byte Party, char *LogFileName,
		  bool UnlinkLog=TRUE,
		  bool Bubble=FALSE );

 word RecoverUsingCml( char *LogFileName );

 word RollbackOkaLog( byte Device, byte Party, char *LogFileName );

 word RecoverParty( byte Device, byte Party );

#endif