#include "recovery.h"

//#define DEBUG
//#define SEE_ERROR

#define DO_NOT_UNLINK FALSE
#define DO_UNLINK     TRUE
#define BUBBLE_UP     TRUE

#define DVORKAS_LOGS

//- WriteAllocUsingLog --------------------------------------------------------

#define TRA_NORMAL	0xAAAAAAAAlu
#define TRA_ROLLBACKED	0xBBBBBBBBlu

typedef struct
{
 dword  Magic;
 dword  Value;		// condition: TRA_NORMAL || TRA_ROLLBACKED
}
LogAllocRecord;

typedef struct
{
 dword  GroupID;
 dword  Logical;
 dword  LogicalInBmp;
 word   Number;
}
BmpDescriptor;



// Magics
#define LOG_BEG_TRANSACTION  0xAAAAAAAAlu
#define LOG_COM_TRANSACTION  0xBBBBBBBBlu

#define LOG_BEG_WR_BMP       0xCCCCCCCClu
#define LOG_COM_WR_BMP       0xDDDDDDDDlu

#define LOG_BEG_WR_GRPSTAT   0xEEEEEEEElu
#define LOG_COM_WR_GRPSTAT   0xFFFFFFFFlu

#define LOG_BEG_WR_SCND_BOOT 0x99999999lu
#define LOG_COM_WR_SCND_BOOT 0x88888888lu

#define LOG_CHECKPOINT       0x77777777lu

#define LOG_BEG_WR_OKA       0x66666666lu
#define LOG_COM_WR_OKA       0x55555555lu


word AllocFreeUsingLog( byte   Device,
			byte   Party,

			dword  GroupID,
			dword  Logical,
			dword  LogicalInBmp,
			word   Number,

			word   PackageID,

			MasterBootRecord  *BR,
			void   far *Bitmap,
			bool   FreeCalls,
			bool   WriteOka
		      )

// CREATES:
// "*.NOT" alloc/free log files without PackageID -> used for logs
// "*.ALF" alloc/free log files with PackageID -> OKA record written here too
//
// This function makes transactional write of alloc/free results:
//
// Creates log file and writes:
//
//
// LOG_BEG_TRANSACTION 			0.. caller is Alloc, 1.. caller is Free
//
//  LOG_BEG_WR_BMP
//    write_to_log   GroupID
//    write_to_log   Logical
//    write_to_log   LogicalInBmp (0-4095)
//    write_to_log   Number
//    write_to_log   Bitmap
//   LOG_CHECKPOINT
//    write_to_party Bitmap
//  LOG_COM_WR_BMP
//
//  LOG_BEG_WR_GRPSTAT
//    write_to_log   OldGrpFreeSpace
//   LOG_CHECKPOINT
//    write_to_party NewGrpFreeSpace
//  LOG_COM_WR_GRPSTAT
//
//  LOG_BEG_WR_SCND_BOOT
//    write_to_log   OldPartyFreeSpace
//   LOG_CHECKPOINT
//    write_to_party NewPartyFreeSpace
//  LOG_COM_WR_SCND_BOOT
//
//  LOG_BEG_WR_OKA
//    // if ALF => OKA record is written
//  LOG_COM_WR_OKA
//
// LOG_COM_TRANSACTION
//
//
// if system crash UNDO is done using log

// DODELAT bitmapa je bad...
{
 int  		 Handle,
		 RecordHandle;
 word            FreeGrpSpace;          // free group space
 dword           FreeSpace;		// free party space
 LogAllocRecord  LogRecord;
 BmpDescriptor   BmpDescr;
 OkaRecord	 Record;
 long 		 value;

 char            String[8+1+3 +1],
		 OkaLogFileName[8+1+3 +1],
		 LogFileName[8+1+3 +1];

		 LogFileName[0]=Device+'0'; // first cipher is device
		 LogFileName[1]=Party+'0';  // first cipher is device
		 LogFileName[2]=0;

 Device&=0x0F;

 // now add packageID to be unicate
 value=(long)PackageID;

 ltoa( value, String, 10 );

 strcat( LogFileName, String );

 if( PackageID )
  strcat( LogFileName, ".ALF" );       // alloc log
 else
  strcat( LogFileName, ".NOT" );       // alloc log



 #ifdef DVORKAS_LOGS
 // create log file
 if ((Handle = open( LogFileName, O_CREAT  | O_RDWR | O_BINARY,
				  S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't create alloc log file...");
  #endif

  return ERR_FS_FATAL_ERROR;
 }
 #else
  fs_open( LogFileName, )


 #endif


 // --- LOG_BEG_TRANSACTION ---

 LogRecord.Magic=LOG_BEG_TRANSACTION;
 LogRecord.Value=(dword)FreeCalls;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) )
 {
  #ifdef SEE_ERROR
   printf("\n Error writing Log file...");
  #endif

  return ERR_FS_FATAL_ERROR;
 }



 // --- LOG_BEG_WR_BMP ---

 LogRecord.Magic        =LOG_BEG_WR_BMP;
 LogRecord.Value        =TRA_NORMAL;
  BmpDescr.GroupID      =GroupID;
  BmpDescr.Logical      =Logical;
  BmpDescr.LogicalInBmp =LogicalInBmp;
  BmpDescr.Number       =Number;

  if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }
   if( write( Handle, &BmpDescr, sizeof(BmpDescriptor) ) != sizeof(BmpDescriptor) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }
   if( write( Handle, Bitmap, 512u ) != 512u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

  LogRecord.Magic        =LOG_CHECKPOINT;
  LogRecord.Value        =TRA_NORMAL;

  if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }


    if( SaveBitmap( Device, Party, GroupID, BR, Bitmap ))
    {
     #ifdef SEE_ERROR
      printf("\n Error saving bitmap: Dev %x, Party %u Grp %lu... ", Device, Party, GroupID );
     #endif

     farfree( Bitmap );
     return ERR_FS_FATAL_ERROR;
    }

    #ifdef DEBUG
     printf("\n Saving bmp: Dev %x, Party %u Grp %lu... ", Device, Party, GroupID );
    #endif

 LogRecord.Magic=LOG_COM_WR_BMP;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 // --- LOG_BEG_WR_GRPSTAT ---

 LogRecord.Magic=LOG_BEG_WR_GRPSTAT;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

	GetGroupFreeSpace( Device, Party, GroupID, FreeGrpSpace );
	if( write( Handle, &FreeGrpSpace, 2u ) != 2u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

    LogRecord.Magic        =LOG_CHECKPOINT;
    LogRecord.Value        =TRA_NORMAL;

    if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

	if( FreeCalls )
	 FreeGrpSpace+=Number;
	else
	 FreeGrpSpace-=Number;

	SetGroupFreeSpace( Device, Party, GroupID, FreeGrpSpace );

 LogRecord.Magic=LOG_BEG_WR_SCND_BOOT;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 // --- LOG_BEG_WR_SCND_BOOT ---

 LogRecord.Magic=LOG_COM_WR_GRPSTAT;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

	GetPartyFreeSpace( Device, Party, FreeSpace );
	if( write( Handle, &FreeSpace, 4u ) != 4u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

    LogRecord.Magic        =LOG_CHECKPOINT;
    LogRecord.Value        =TRA_NORMAL;

    if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

	if( FreeCalls )
	 FreeSpace+=Number;
	else
	 FreeSpace-=Number;

	SetPartyFreeSpace( Device, Party, FreeSpace );

 LogRecord.Magic=LOG_COM_WR_SCND_BOOT;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 // --- LOG_BEG_WR_OKA ---

 LogRecord.Magic=LOG_BEG_WR_OKA;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

	// write one record to OKA file
	if( WriteOka && PackageID ) // PackageID must be != 0
	{
	 // construct log name
	 OkaLogFileName[0]=Device+'0'; // first cipher is device
	 OkaLogFileName[1]=Party+'0';  // first cipher is device
	 OkaLogFileName[2]=0;

	 long value=(long)PackageID;   // now add packageID to be unicate
	 ltoa( value, String, 10 );

	 strcat( OkaLogFileName, String );
	 strcat( OkaLogFileName, ".OKA" );       // alloc log

	 // fill record
	 if( FreeCalls )
	  Record.Record_Beg   = FREE_RECORD;
	 else
	  Record.Record_Beg   = ALLOC_RECORD;
	 Record.State        = NORMAL_STATE;
	 Record.Logical      = Logical;
	 Record.Number       = Number;
	 Record.GroupID      = GroupID;
	 Record.LogicalInBmp = LogicalInBmp;
	 Record.Record_End   = OKA_RECORD_END;

	 // open for append or create log file
	 if ((RecordHandle = open( OkaLogFileName, O_CREAT  | O_RDWR | O_BINARY | O_APPEND,
				     S_IWRITE | S_IREAD 		)) == -1)
	 {
	  #ifdef SEE_ERROR
	   printf("\n Error: cann't create alloc log file...");
	  #endif

	  // DODELAT: ma smysl takovyhle error?
	  return ERR_FS_FATAL_ERROR;
	 }

	 #ifdef DEBUG
	  printf("\n Log file %s opened (created) PackID %u FreeCalls %d...", OkaLogFileName, PackageID, FreeCalls );
	 #endif

	 // write record
	 if( write( RecordHandle, &Record, sizeof(OkaRecord) ) != sizeof(OkaRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

	 #ifdef DEBUG
	  printf(" written...");
	 #endif

	 close( RecordHandle );

	 #ifdef DEBUG
	  printf(" closed...");
	 #endif
	}

 LogRecord.Magic=LOG_COM_WR_OKA;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 // --- LOG_COM_TRANSACTION ---
 LogRecord.Magic=LOG_COM_TRANSACTION;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 close( Handle );



 #ifdef UNLINK_LOGS
  unlink( LogFileName );
 #endif



 return ERR_FS_NO_ERROR;
}

//- GenerateHlpLog -----------------------------------------------------------

word GenerateHlpLog( char   *LogFileName,
		     dword  GroupID,
		     dword  Logical,
		     dword  LogicalInBmp,
		     word   Number,
		     bool   FreeCalls
		   )

// CREATES "*.HLP" log which has the same structure as "*.ALF"

// DODELAT
//  - bitmapa je bad... ( postara se o to load bitmap )
{
 byte   	 Device=(byte)LogFileName[0]-'0';
 byte   	 Party=(byte)LogFileName[1]-'0';
 word            FreeGrpSpace;          // free group space
 dword           FreeSpace;		// free party space
 int  		 Handle;
 LogAllocRecord  LogRecord;
 BmpDescriptor   BmpDescr;
 MasterBootRecord BR;			// load it
 void            far *Bitmap;           // bitmap must be allocated and loaded



 #ifdef DEBUG
  printf("\n Generating log file %s...", LogFileName );
 #endif

 Device&=0x0F;



 Bitmap=farmalloc( 512lu );
 if( !Bitmap ) { printf("\n Not enough far memory in %s line %d ( Cache init )...", __FILE__, __LINE__ ); exit(0); }



 if( LoadBoot( Device, Party, &BR ) )
 {
  #ifdef SEE_ERROR
   printf("\n Error reading BR Dev %d Party %d in %s line %d...", Device, Party, __FILE__, __LINE__ );
  #endif

  farfree( Bitmap );
  return ERR_FS_FATAL_ERROR;
 }


 if( LoadBitmap( Device, Party, GroupID, &BR, Bitmap ))
 {
  #ifdef SEE_ERROR
   printf("\n Error saving bitmap: Dev %x, Party %u Grp %lu... ", Device, Party, GroupID );
  #endif

  farfree( Bitmap );
  return ERR_FS_FATAL_ERROR;
 }


 // create log file
 if ((Handle = open( LogFileName, O_CREAT  | O_RDWR | O_BINARY,
				  S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
  printf("\n Error: cann't create log file %s...", LogFileName );
  #endif

  return ERR_FS_FATAL_ERROR;
 }


 // --- LOG_BEG_TRANSACTION ---

 LogRecord.Magic=LOG_BEG_TRANSACTION;
 LogRecord.Value=(dword)FreeCalls;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 // --- LOG_BEG_WR_BMP ---

 LogRecord.Magic        =LOG_BEG_WR_BMP;
 LogRecord.Value        =TRA_NORMAL;
  BmpDescr.GroupID      =GroupID;
  BmpDescr.Logical      =Logical;
  BmpDescr.LogicalInBmp =LogicalInBmp;
  BmpDescr.Number       =Number;

  if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }
   if( write( Handle, &BmpDescr, sizeof(BmpDescriptor) ) != sizeof(BmpDescriptor) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }
   if( write( Handle, Bitmap, 512u ) != 512u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

  LogRecord.Magic        =LOG_CHECKPOINT;
  LogRecord.Value        =TRA_NORMAL;

  if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

    // Bitmap is not written to party here!

 LogRecord.Magic=LOG_COM_WR_BMP;
 LogRecord.Value=(dword)Number;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 // --- LOG_BEG_WR_GRPSTAT ---

 LogRecord.Magic=LOG_BEG_WR_GRPSTAT;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

	// get new free space and put old to log
	GetGroupFreeSpace( Device, Party, GroupID, FreeGrpSpace );

	if( FreeCalls )
	 FreeGrpSpace+=Number;
	else
	 FreeGrpSpace-=Number;

	if( write( Handle, &FreeGrpSpace, 2u ) != 2u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

    LogRecord.Magic        =LOG_CHECKPOINT;
    LogRecord.Value        =TRA_NORMAL;

    if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

    // free space is not set here

 LogRecord.Magic=LOG_BEG_WR_SCND_BOOT;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 // --- LOG_BEG_WR_SCND_BOOT ---

 LogRecord.Magic=LOG_COM_WR_GRPSTAT;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

	// get new free space and put old to log
	GetPartyFreeSpace( Device, Party, FreeSpace );

	if( FreeCalls )
	 FreeSpace+=Number;
	else
	 FreeSpace-=Number;

	if( write( Handle, &FreeSpace, 4u ) != 4u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

    LogRecord.Magic        =LOG_CHECKPOINT;
    LogRecord.Value        =TRA_NORMAL;

    if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

    // free space is not written here

 LogRecord.Magic=LOG_COM_WR_SCND_BOOT;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 // --- LOG_BEG_WR_OKA ---

 LogRecord.Magic=LOG_BEG_WR_OKA;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

	// Call function which writes to *.OKA

 LogRecord.Magic=LOG_COM_WR_OKA;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 // --- LOG_COM_TRANSACTION ---
 LogRecord.Magic=LOG_COM_TRANSACTION;
 LogRecord.Value=TRA_NORMAL;

 if( write( Handle, &LogRecord, sizeof(LogAllocRecord) ) != sizeof(LogAllocRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }



 farfree( Bitmap );

 close( Handle );



 #ifdef DEBUG
  printf("\n Generating of %s done...", LogFileName );
 #endif


 return ERR_FS_NO_ERROR;
}

//- UndoAlfLog ---------------------------------------------------------------

word UndoAlfLog( byte Device, byte Party, char *LogFileName,
		 bool UnlinkLog,
		 bool Bubble
	       )
//
// 			  IS IDEMPOTENT!
//
// What it UnDOes:
//  - what's done using ALF ( Alloc/Free ) log with PackageID
//    => bubbles to corresponding OKA file, where marks corresponding
//    record as rollbacked
//  - what's done using  NOT ( Alloc/Free ) log with PackageID==0 => used for
//    logs
//  - what's done using  HLP ( Alloc/Free ) logs generated in OKA rollback
//
//
// Log structure:                                                 Size:  Off:
//
// LOG_BEG_TRANSACTION 	( 0 Alloc, 1 Free )                        8      0
//
//  LOG_BEG_WR_BMP                                                 8      8
//    write_to_log   GroupID                                       4      16
//    write_to_log   Logical                                       4      20
//    write_to_log   LogicalInBmp (0-4095)                         4      24
//    write_to_log   Number                                        2      28
//    write_to_log   Bitmap                                        512    30
//   LOG_CHECKPOINT                                                8      542
//    write_to_party Bitmap                                        ..
//  LOG_COM_WR_BMP                                                 8      550
//
//  LOG_BEG_WR_GRPSTAT                                             8      558
//    write_to_log   OldGrpFreeSpace                               2      566
//   LOG_CHECKPOINT                                                8      568
//    write_to_party NewGrpFreeSpace                               ..
//  LOG_COM_WR_GRPSTAT                                             8      576
//
//  LOG_BEG_WR_SCND_BOOT                                           8      584
//    write_to_log   OldPartyFreeSpace                             4      592
//   LOG_CHECKPOINT                                                8      596
//    write_to_party NewPartyFreeSpace                             ..
//  LOG_COM_WR_SCND_BOOT                                           8      604
//
//  LOG_BEG_WR_OKA                                                 8      612
//    ( call function ... which writes it transactionaly to OKA )  ..
//  LOG_COM_WR_OKA                                                 8      620
//
// LOG_COM_TRANSACTION                                             8      628
//							        ---------
//                                                        Total:   636
//
// Statistisc in log contains old values => can be rewritten two,... times
// Bmp undo can be done => I have old BMP =>can be rewritten two,... times

{
 void  far *AlfLog;
 bool  BubbleToOka;
 char  OkaLogFileName[8+1+3 +1];
 int   AlfHandle,
       OkaHandle;
 word  LogLng,
       i,
       Number;
 long  H;
 long  FileLength,
       RecordOff,
       NrOfRecords,
       LastRecordNotComplete;
 dword Logical;

 OkaRecord        Record;
 MasterBootRecord BR;



 Device&=0x0F;

 #ifdef DEBUG
  printf("\n *.ALF || *.NOT UNDO using LogFile %s...", LogFileName );
 #endif

 // open log file
 if ((AlfHandle = open( LogFileName, O_RDONLY | O_BINARY,
				     S_IWRITE | S_IREAD 	)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't open log file %s...", LogFileName );
  #endif

  return ERR_FS_FATAL_ERROR;
 }

 H=filelength( AlfHandle );
 LogLng=(word)H;

 #ifdef DEBUG
  printf("\n  Length of %s is: %uB... ", LogFileName, LogLng );
 #endif

 // alloc space for it
 if( !LogLng )
 {
  AlfLog=farmalloc( 1lu );
 }
 else
 {
  AlfLog=farmalloc( (dword)LogLng );
 }

 if( !AlfLog ) { printf("\n Not enough far memory in %s line %d ( Cache init )...", __FILE__, __LINE__ ); exit(0); }

 // read log content to buffer
 if( read( AlfHandle, AlfLog, LogLng ) != LogLng ) { printf("\n Error reading Log file..."); return ERR_FS_FATAL_ERROR; }


 // --- ANALYZE ALF LOG ---


 // if( LogLng==636 ) => is complete + should be completely rollbacked
 // => there's no problem because this operation is idempotent

 // LOG_BEG_WR_OKA magic in log file...
 if( LogLng>=624
      &&
     Bubble
   )
 {
  // for now only set flag, bubble must be done after ALF rollback
  // and before ALF unlink...
  #ifdef DEBUG
   printf("\n  DO bubble...");
  #endif

  BubbleToOka=TRUE;
 }
 else
 {
  // for now only set flag, bubble must be done after ALF rollback
  // and before ALF unlink...
  #ifdef DEBUG
   printf("\n  DO NOT bubble...");
  #endif

  BubbleToOka=FALSE;
 }


 if( LogLng>=(596+4) && *(dword far *)((byte far *)AlfLog+596)==LOG_CHECKPOINT  )
 {
  // writing party total free size commited
  #ifdef DEBUG
   printf("\n  Party total free space checkpoint OK -> UNDO:");
  #endif

  #ifdef DEBUG
   printf("\n   Old free party %u space was %luB so write it...",
	   Party,
	   *(dword far *)((byte far *)AlfLog+592)
	 );
  #endif

  SetPartyFreeSpace( Device, Party, *(dword far *)((byte far *)AlfLog+592) );

  // continue rollback...
 }


 if( LogLng>=(568+4) && *(dword far *)((byte far *)AlfLog+568)==LOG_CHECKPOINT  )
 {
  // writing party total free size commited
  #ifdef DEBUG
   printf("\n  Group free space checkpoint OK -> UNDO:");
  #endif

  #ifdef DEBUG
   printf("\n   Group %lu free space was %u secs so write it...",
	   *(dword far *)((byte far *)AlfLog+16),
	   *(word far *)((byte far *)AlfLog+566)
	 );
  #endif

  SetGroupFreeSpace( Device, Party,
		     *(dword far *)((byte far *)AlfLog+16),
		     (word)*(dword far *)((byte far *)AlfLog+566) );

  // continue rollback...
 }

 if( LogLng>=(542+4) && *(dword far *)((byte far *)AlfLog+542)==LOG_CHECKPOINT  )
 {
  // writing party total free size commited
  #ifdef DEBUG
   printf("\n  Bitmap backup checkpoint OK -> UNDO:"
	  "\n   First undo operations in bitmap");
  #endif

  long FirstBit=*(long far *)((byte far *)AlfLog+24);
  long LastBit=FirstBit;
       LastBit+=(long)*(word far *)((byte far *)AlfLog+28);
       LastBit--;                                     // LogicalInBmp + number - 1

  #ifdef DEBUG
   printf(" on < %lu, %lu >...",(dword)FirstBit, (dword)LastBit );

  #endif

  BMP  Bmp(512l*8l);
  word i;

  for( i=0; i<512; i++ )
   Bmp.Bmp[i]=*(byte far *)((byte far *)AlfLog+32+i);



/*
 close(AlfHandle);

 int Handle1;

 if ((Handle1 = open( "BMP1", O_CREAT  | O_RDWR | O_BINARY,
				  S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't create alloc log file...");
  #endif

  return ERR_FS_FATAL_ERROR;
 }
 if( write( Handle1, Bmp.Bmp, 512u ) != 512u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }
 close(Handle1);
//*/

  #ifdef DEBUG
   printf("\n   Log was created by");
  #endif

  if( (word)*(dword far *)((byte far *)AlfLog+4)==FALSE ) // Alloc/Free
  {
   // Make space FULL if was free
   Bmp.SetInterval( (long)FirstBit, (long)LastBit );
   #ifdef DEBUG
    printf(" ALLOC -> set interval to undo...");
   #endif
  }
  else
  {
   // Make space FREE if was alloc
   Bmp.ClearInterval( (long)FirstBit, (long)LastBit );
   #ifdef DEBUG
    printf(" FREE -> clear interval to undo...");
   #endif
  }

/*
 if ((Handle1 = open( "BMP2", O_CREAT  | O_RDWR | O_BINARY,
				  S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't create alloc log file...");
  #endif

  return ERR_FS_FATAL_ERROR;
 }
 if( write( Handle1, Bmp.Bmp, 512u ) != 512u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

 close(Handle1);
//*/


  if( LoadBoot( Device, Party, &BR ) )
  {
   #ifdef SEE_ERROR
    printf("\n Error reading BR Dev %d Party %d in %s line %d...", Device, Party, __FILE__, __LINE__ );
   #endif

   return ERR_FS_FATAL_ERROR;
  }

  #ifdef DEBUG
   printf("\n BR read: Dev %d Party %d in %s line %d...", Device, Party, __FILE__, __LINE__ );
  #endif


  SaveBitmap( Device,
	      Party,
	      *(dword far *)((byte far *)AlfLog+16), 	// group ID
	      &BR,
	      Bmp.Bmp
	    );

  // continue rollback...
 }

 close( AlfHandle );



 // --- BUBBLE TO CORESPONDING OKA IF WANTED ---

 // has sense only if I can recognize Logical and Number
 if( BubbleToOka
      &&
     LogLng>=32
   )
 {
  // recognising Logical and Number
  Logical = *(dword far *)((byte far *)AlfLog+20);
  Number = *(word far *)((byte far *)AlfLog+28);

  // open OKA, analyze it and search for the same record, search
  // from back to from between records in NORMAL_STATE, if record
  // not found => it must be rollbacked sometime before

  // construct OKA log name from ALF name ( change extension )

  for( i=0; LogFileName[i]!='.' && LogFileName[i]!=0; i++ )
  {
   OkaLogFileName[i]=LogFileName[i];
  }

  OkaLogFileName[i]='.';
  OkaLogFileName[++i]=0;
  strcat( OkaLogFileName, "OKA" );

  printf("\n  Bubbled to %s and searching for Logical %lu Number %u", OkaLogFileName, Logical, Number );

  // open log file
  if ((OkaHandle = open( OkaLogFileName, O_RDWR | O_BINARY,
					 S_IWRITE | S_IREAD )) == -1)
  {
   // OKA log not exist => there was no successful allocation =>
   // no rollback needed

   #ifdef DEBUG
    printf("\n LogFile %s not exist...\n", OkaLogFileName );
   #endif

   #ifdef UNLINK_LOGS
    if( UnlinkLog )
     unlink( LogFileName );
   #endif

   #ifdef DEBUG
    printf("\n ROLLBACK using LogFile %s is DONE...\n", LogFileName );
   #endif

   farfree( AlfLog );

   return ERR_FS_NO_ERROR;
  }



  FileLength=filelength( OkaHandle );

  printf("\n\n\n LNG: %lu, %d", (dword)FileLength, FileLength);

  if( !FileLength )
  {
   // OKA log not exist => there was no successful allocation =>
   // no rollback needed

   #ifdef DEBUG
    printf("\n No record in %s...\n", OkaLogFileName );
   #endif

   #ifdef UNLINK_LOGS
    if( UnlinkLog )
     unlink( LogFileName );
   #endif

   #ifdef DEBUG
    printf("\n ROLLBACK using LogFile %s is DONE...\n", LogFileName );
   #endif

   farfree( AlfLog );

   return ERR_FS_NO_ERROR;
  }

  // count off of last record

  // test if the last record is complete or not
  LastRecordNotComplete=FileLength%OKA_RECORD_LNG;
			// now i know how many bytes
			// i can read from the last
			// record
  RecordOff=FileLength/OKA_RECORD_LNG;
  NrOfRecords=RecordOff;

  if( !LastRecordNotComplete )
   RecordOff--;				// off 0..
  // else last not complete

  RecordOff*=OKA_RECORD_LNG;           	// on it


  #ifdef DEBUG
   printf("\n  Length of log is: %luB, %lu Items, LastRecOff %lu... ", (dword)FileLength, (dword)NrOfRecords, (dword)RecordOff );
  #endif


  if( LastRecordNotComplete )
  {
   // seek last not complete rollbacked it ( don't read it ) is idempotent
   // and last not complete record is nowhere tested
   #ifdef DEBUG
    printf("\n  Item on Off %lu not complete => mark it as rollbacked...", (dword)RecordOff );
   #endif

   lseek( OkaHandle, RecordOff, SEEK_SET );

   Record.State=ROLLBACKED_STATE; 	// mark record as rollbacked

   // write only mark -> nothing else
   if( write( OkaHandle, &Record, 4u) != 4u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

   // this item is rollbacked => move previous
   NrOfRecords--;

   RecordOff-= OKA_RECORD_LNG;   // move MY offset to previous record
  }



  // it was mark "na slepo" => therefore now I must search if there is
  // the record I wanted to found
  while( NrOfRecords )
  {

   #ifdef DEBUG
    printf("\n  Item from offset %lu... ", (dword)RecordOff );
   #endif


   // seek last not rollbacked record
   lseek( OkaHandle, RecordOff, SEEK_SET );

   // read it
   if( read( OkaHandle, &Record, sizeof(OkaRecord) ) != sizeof(OkaRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }


   if( Record.State==ROLLBACKED_STATE )
   {
    #ifdef DEBUG
     printf("  Already ROLLBACKED... ");
    #endif

    NrOfRecords--;

    RecordOff-= OKA_RECORD_LNG;   // move MY offset to previous record

    // take last this is done
    continue;
   }

   // test it is record I searched for
   if( Record.Logical==Logical && Record.Number==Number )
   {
    #ifdef DEBUG
     printf("  Item Logical %lu Number %u found in OKA...", Logical, Number );
    #endif

    // mark it as rollbacked

    Record.State=ROLLBACKED_STATE; 	// mark record as rollbacked

    if( write( OkaHandle, &Record, 4u) != 4u ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }

    #ifdef DEBUG
     printf("\n   => marked as ROLLBACKED...");
    #endif

    close( OkaHandle );

    #ifdef UNLINK_LOGS
     if( UnlinkLog )
      unlink( LogFileName );	// delete ALF
    #endif

    #ifdef DEBUG
     printf("\n ROLLBACK using LogFile %s is DONE...\n", LogFileName );
    #endif

    farfree( AlfLog );

    // take last this is done
    return ERR_FS_NO_ERROR;
   }
   #ifdef DEBUG
    printf("  Item different: Logical %lu Number %u...", Record.Logical, Record.Number );
   #endif

   // this item is rollbacked => take previous
   NrOfRecords--;

   RecordOff-= OKA_RECORD_LNG;   // move MY offset to previous record
  } // while NrOfRecords
 }  // if Bubble

 close( OkaHandle ); // OKA file




 // item not found in OKA => ALF already rollbacked => delete it



 #ifdef UNLINK_LOGS
  if( UnlinkLog )
   unlink( LogFileName );
 #endif



 #ifdef DEBUG
  printf("\n ROLLBACK using LogFile %s is DONE...\n", LogFileName );
 #endif

 farfree( AlfLog );

 return ERR_FS_NO_ERROR;
}

//- RollbackOkaLog ---------------------------------------------------------------

word RollbackOkaLog( byte Device, byte Party, char *LogFileName )
//
// 			  IS IDEMPOTENT!
//
// - UnDO what's done using OKA ( OK allocated ) log
//
//
// Structure of OKA log file:
//
// RECORD_BEG 		             0xA.A .. Alloc rec, 0xE.E.. Free rec
//
//    write_to_log   State	     Record NORMAL || Record ROLLBACKED
//    write_to_log   Logical
//    write_to_log   Number
//    write_to_log   GroupID
//    write_to_log   LogicalInBmp
//
// CALLING_ALF_WR
//    // called AllocFreeUsingLog();
// ALF_WRITTEN			     End of record
//
//
// What it does:
// 	Reads log from back to front by records. For each item from log
// construct ALF log. This is done this way: first is constructed HLP file
// ( same structure as ALF ) which is then renamed to *.ALF. It it done
// this way because I want ALF would be everytime complete. If system
// crashes in time before rename => HLP file is deleted ( I don't
// know if was complete ). Later I know that on party is right ONE ALF
// log which is after system crash rollbacked as the first. Because
// UndoAlfLogs() is idempotent => i can continue in rollback
//
// Algorithm:
//
//  - now work on whole records ( from back to front, each the same way )
//    - check if record isn't rollbacked -> if it is take previous record
//    - => record is normal
//    - if the record is ALLOC type => generate HLP file and as caller
//      sign ALLOC ( FREE type analogic )
//    - rename HLP to ALF
//    - call ALF undo utility
//    - mark record as rollbacked
//    - unlink ALF record
//
// Hint:
// 	Possible not complete record at the end of OKA not checked because
// is rollbacked in time of ALF undo which is done before OKA rollback...
//

{
 int  		Handle;
 long 		FileLength,
		RecordOff,
		NrOfRecords;
 bool		Caller;
 OkaRecord      Record;
 char           HlpFileName[8+1+3+ 1],
		AlfFileName[8+1+3+ 1];
 word		i;



 // create hlp filename from LogFileName
 for( i=0; LogFileName[i]!='.' && LogFileName[i]!=0; i++ )
 {
  AlfFileName[i]=HlpFileName[i]=LogFileName[i];
 }

 HlpFileName[i]='.';			AlfFileName[i]='.';
 HlpFileName[++i]=0;			AlfFileName[i]=0;
 strcat( HlpFileName, "HLP" );	        strcat( AlfFileName, "ALF" );


 Device&=0x0F;

 #ifdef DEBUG
  printf("\n OKA UNDO using LogFile %s...", LogFileName );
 #endif

 // open log file
 if ((Handle = open( LogFileName, O_RDWR | O_BINARY,
				  S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't read %s...", LogFileName );
  #endif

  return ERR_FS_FATAL_ERROR;
 }

 FileLength=filelength( Handle );

 // count off of last record
 RecordOff=FileLength/OKA_RECORD_LNG;
 NrOfRecords=RecordOff;
 RecordOff--;				// off 0..
 RecordOff*=OKA_RECORD_LNG;           	// on it


 #ifdef DEBUG
  printf("\n  Length of log is: %luB, %lu Items, LastRecOff %lu... ", (dword)FileLength, (dword)NrOfRecords, (dword)RecordOff );
 #endif


 while( NrOfRecords )
 {

  #ifdef DEBUG
   printf("\n  Item from offset %lu... ", (dword)RecordOff );
  #endif


  // seek last not rollbacked record
  lseek( Handle, RecordOff, SEEK_SET );

  // read it
  if( read( Handle, &Record, sizeof(OkaRecord) ) != sizeof(OkaRecord) ) { printf("\n Error writing Log file..."); return ERR_FS_FATAL_ERROR; }


  if( Record.State==ROLLBACKED_STATE )
  {
   #ifdef DEBUG
    printf("\n  Already ROLLBACKED... ");
   #endif

   NrOfRecords--;

   RecordOff-= OKA_RECORD_LNG;   // move MY offset to previous record

   // take last this is done
   continue;
  }



  // generate HLP log ( has the ALF structure )

  if( Record.Record_Beg == ALLOC_RECORD )
   Caller=CALLER_IS_ALLOC;
  else
   Caller=CALLER_IS_FREE;


  GenerateHlpLog( HlpFileName,
		  Record.GroupID,
		  Record.Logical,
		  Record.LogicalInBmp,
		  Record.Number,
		  Caller
		);

  // HLP log created -> rename it
  rename( HlpFileName, AlfFileName );
  #ifdef DEBUG
   printf("\n  Renamed: %s => %s... ", HlpFileName, AlfFileName );
  #endif

  // now undo what's done
  UndoAlfLog( (byte)AlfFileName[0],
	      (byte)AlfFileName[1],
	      AlfFileName,
	      DO_NOT_UNLINK,
	      FALSE
	    );

  // undo done => mark record as rollbacked
  Record.State=ROLLBACKED_STATE;

  // seek to record
  lseek( Handle, RecordOff, SEEK_SET );

  // write it
  if( write( Handle, &Record, sizeof(OkaRecord) ) != sizeof(OkaRecord) ) { printf("\n Error writing %s...", LogFileName ); return ERR_FS_FATAL_ERROR; }


  #ifdef UNLINK_LOGS
   unlink( AlfFileName );
  #endif


  #ifdef DEBUG
   printf("\n   Item %lu OKA offset %lu rollbacked...", (dword)NrOfRecords, dword(RecordOff));
  #endif


  // this item is rollbacked => take previous
  NrOfRecords--;

  RecordOff-= OKA_RECORD_LNG;   // move MY offset to previous record
 }



 close( Handle ); // OKA file



 #ifdef UNLINK_LOGS
  unlink( LogFileName );
 #endif



 #ifdef DEBUG
  printf("\n ROLLBACK using LogFile %s DONE...\n", LogFileName );
 #endif



 return ERR_FS_NO_ERROR;
}

//- RecoveryFromCml - structures ---------------------------------------------------------------

// record for log of commiting package

#define BEGIN_COPY      0xBBBBBBBBlu
#define NOT_LAST_RECORD	0xFFFFFFFFlu
#define CACHE_INTERVAL	0xCCCCCCCClu

typedef struct
{
 dword TypeOfRecord;    // for transaction

 byte  Device;		// 0, ..
 byte  Party;           // 1, ..
 byte  SwapDevice;
 byte  SwapParty;

 dword Logical;
 dword LogicalSwap;

 word  Number;
}
LogPackRecord;		// sizeof()==15


//- RecoverUsingCml ---------------------------------------------------------------

word RecoverUsingCml( char *LogFileName )
// Recovery using *.CML ( COMMIT LOG ) => EVERYTIME for some Package!
//
//
// Structure of CML record:
//
// TypeOfRecord  // CACHE_INTERVAL || NOT_LAST_RECORD ( SWAP ) || BEGIN COPY
//  Device;		// 0, ..
//  Party;            // 1, ..
//  SwapDevice;
//  SwapParty;
//  Logical;
//  LogicalSwap;
//  Number
//
//
// Structure of CML log file:
//  Type
//   CACHE_INTERVAL  	// intervals moved in pack commit from cache to swap
//   ..
//   CACHE_INTERVAL  	// intervals moved in pack commit from cache to swap
//   NOT_LAST_RECORD  	// intervals in pack commit time on swap
//   ..
//   NOT_LAST_RECORD  	// intervals in pack commit time on swap
//   BEGIN_COPY         // checkpoint: all data on swap, begin of copy
//                      // from swap party to data party
//
// What it does:
// 	Reads log from from to back, if in this is BEGIN_COPY it means that
// all the data from cache was moved to swap in time of commit => I can do
// redo.
//	UNDO: if magic BEGIN_COPY not found in log file it means, that
// some data from cache are not on swap => if i would copy what's there
// file would be unconsistent => OKA must be rollbacked because space which
// was allocated/released will not be used ( OKA rollback is done at the
// end of RecoverParty()) => here only delete CML log.
//	REDO: 1. try to delete OKA log because space will be used -> I don't
// want rollback it.
//	      2. read log file from front to back and using records copy
// data from swap to data party. This operation is IDEMPOTENT!

{
 void          far *Buffer;
 int           Handle,
	       Handle2;
 LogPackRecord Record;
 word          BeginCopy,
	       i;
 char          OkaFileName[8+1+3+ 1];
 long          OkaRecords;


 #ifdef DEBUG
  printf("\n CML UNDO/REDO using LogFile %s...", LogFileName );
 #endif

 // search for BEGIN_COPY magic => whole log must be read...
 // open log file
 if ((Handle = open( LogFileName, O_RDONLY | O_BINARY,
				  S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't read *.CML log file...");
  #endif

  return ERR_FS_FATAL_ERROR;
 }


 #ifdef DEBUG
  printf("\n Searching for LAST_RECORD checkpoint: ");
 #endif

 BeginCopy=FALSE;

 while( read( Handle, &Record, sizeof(LogPackRecord) ) == sizeof(LogPackRecord) )
 {
  if( Record.TypeOfRecord==BEGIN_COPY )
   BeginCopy=TRUE;

  #ifdef DEBUG
   printf(".");
  #endif
 }


 if( BeginCopy )
 {
  // REDO -> copy everything from swap to data using log
  #ifdef DEBUG
   printf("\n LAST_RECORD found -> REDO copy...");
  #endif


  // delete OKA file => space will be used for data from swap

  // create hlp filename from LogFileName
  for( i=0; LogFileName[i]!='.' && LogFileName[i]!=0; i++ )
  {
   OkaFileName[i]=LogFileName[i];
  }

  OkaFileName[i]='.';
  OkaFileName[++i]=0;
  strcat( OkaFileName, "OKA" );

  Handle2 = open( OkaFileName, O_RDWR | O_BINARY, S_IWRITE | S_IREAD );
  OkaRecords=filelength( Handle );
  OkaRecords/=OKA_RECORD_LNG;

  close( Handle2 );

  #ifdef UNLINK_LOGS
   unlink(OkaFileName);
  #endif

  #ifdef DEBUG
   printf("\n LogName: %s Records: %lu -> unlink done...", OkaFileName, (dword)OkaRecords );
  #endif



  // read log from begin to end and copy it

  // seek to begin of log
  lseek( Handle, 0lu, SEEK_SET );


  BeginCopy=FALSE;

  do
  {
     if( read( Handle, &Record, sizeof(LogPackRecord) ) != sizeof(LogPackRecord) )
     {
      #ifdef SEE_ERROR
       printf("\n Error read Log file %s...", LogFileName );
      #endif

      break;
      // return ERR_FS_FATAL_ERROR;
     }


     if( Record.TypeOfRecord==BEGIN_COPY )
     {
      // it is only checkpoint -> means that everything was copied
      #ifdef DEBUG
       printf("\n LAST_RECORD checkpoint read => it's end of log...");
      #endif

      BeginCopy=TRUE;

      break;
     }



     #ifdef DEBUG
      printf("\n Interval <%lu, %lu> => Lng %u will be written to file...",
	     Record.Logical,
	     Record.Logical+Record.Number-1ul,
	     Record.Number
	    );
     #endif


     Buffer=farmalloc((dword)(Record.Number)*512ul);
     if( !Buffer ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }


     // load data from swap

     if( LoadSectorThrough( Record.SwapDevice,
			    Record.SwapParty,
			    Record.LogicalSwap,
			    Record.Number,
			    Buffer
			  )
       )
     {
      #ifdef SEE_ERROR
       printf("\n Error reading: device %d party %d SwpSec %lu Lng %d... ",
			    Record.SwapDevice,
			    Record.SwapParty,
			    Record.LogicalSwap,
			    Record.Number
	     );
      #endif
      return ERR_FS_FATAL_ERROR;
     }

     #ifdef DEBUG
      printf("\n Sector read from Dev %d Party %d SwpSec %lu Lng %d... ",
			    Record.SwapDevice,
			    Record.SwapParty,
			    Record.LogicalSwap,
			    Record.Number
	    );
     #endif


     // save data to party

     if( SaveSectorThrough( Record.Device, Record.Party,
			    Record.Logical,
			    Record.Number,
			    Buffer
			  )
       )
     {
      #ifdef SEE_ERROR
       printf("\n Error writing: device %d party %d SwpSec %lu Lng %d... ",
			    Record.Device,
			    Record.Party,
			    Record.Logical,
			    Record.Number
	     );
      #endif
      return ERR_FS_FATAL_ERROR;
     }

     #ifdef DEBUG
      printf("\n Sector written to device %d party %d SwpSec %lu Lng %d... ",
			    Record.Device,
			    Record.Party,
			    Record.Logical,
			    Record.Number
	    );
     #endif


     // delete buffer
     farfree( Buffer );

     // free secs on swap
     CacheManFreeSector(
			 Record.SwapDevice,
			 Record.SwapParty,
			 Record.LogicalSwap,
			 Record.Number,
			 0,
			 FPACK_NOTHING
		       );

  }
  while( TRUE );



  #ifdef DEBUG
   printf("\n Deleting Log file...");
  #endif

  close( Handle );

  #ifdef UNLINK_LOGS
   unlink( LogFileName );
  #endif

  #ifdef DEBUG
   printf("\n REDO success...");
  #endif
 }
 else
 {
  // UNDO -> free swap using OKA and ALF, delete CML log ( nothing )
  // will be copied


  #ifdef DEBUG
   printf("\n LAST_RECORD not found  -> correspoindng OKA will be later rollbacked,"
	  "\n  Alloc/Free for data done on swap will be repaired by reformating swap,  "
	  "\n  %s no longer needed ( some data on swap some not ) -> redo "
	  "\n  impossible => deleting Log file %s...", LogFileName, LogFileName );
  #endif


  close( Handle );

  #ifdef UNLINK_LOGS
   unlink( LogFileName );
  #endif

  #ifdef DEBUG
   printf("\n UNDO success...");
  #endif
 }



 return ERR_FS_NO_ERROR;
}

//- RecoverParty --------------------------------------------------------------------------------

word RecoverParty( byte Device, byte Party )
// - recovers RFS on specified party

{
 // What recover does:
 // - searching for *.HLP ( same structure as *.ALF ). These files
 //   are generated when rollback of OKA is done. HLP is created
 //   and must be complete, then it is renamed to *.ALF. In OKA
 //   rollback will be generated again...
 //
 // - search for *.NOT ( alloc/free without PackageID). These logs are
 //   used for alloc/free operations which are not cached. Used are
 //   for example for work with logs ( don't use cache => direct RD/WR )
 //   or work with swap. Swap is at the end of recovery formatted => OK
 //   but on data party must be log space ops rollbacked. In logs there was
 //   no chance to evide this space in FNODE => it would be LOST SECTORS
 //   == are sectors which owned by NO FILE.
 //	=> do UNDO on each founded NOT file.
 //
 // - search for *.ALF ( alloc/free ) rollback it and bubble up through it
 //   to it's *.OKA and write ROLLBACKED magic to corresponding item
 //   If *.ALF done => all other types of logs are in consistent state,
 //   it means that all transactions in them are commited or rollbacked
 //
 // - *.CML ( commit log ) files recovery:
 //
 //    (i) if magic BEGIN_COPY => redo is done ( can be double free on swap )
 //        but it's OK because swap party is then reformated.
 //        *.OKA of this package is deleted because allocations can't have to
 //        be rollbacked ( space is used ).
 //
 //    (ii) if magic not there => undo is done. It means that *.CML is deleted
 //         ( because some data are on swap and some are not ) and swap is
 //         at the end reformated. But must be done ROLLBACK OF *.OKA because
 //         on data party's were allocated/released space and will not
 //         be used
 //
 // - now on party rests *.OKA files. These must be ROLLBACKED ( using
 //   generation of ALFs => if system crashes in time of recovery I can
 //   continue ).ROLLBACK OF *.OKA must be done on because data party's
 //   were allocated/released space and will not be used
 //
 // System is recovered now...
 // If system crashed in time of recovery => no problem recovery continues
 // where it ends it's because using of *.ALF => this type of log can
 // 0 or 1 on party => it SERIALIZES undo steps of transactions

 #ifdef DEBUG
  printf("\n Recover RFS on Dev %u Party %u...", Device, Party );
 #endif


 struct ffblk ffblk;
 int done;



 //--- DELETING *.HLP ---

 #ifdef DEBUG
  printf("\n Deleting *.HLP logs:");
 #endif


 done = findfirst("*.*",&ffblk,0);

 while (!done)
 {
   // printf("  %s\n", ffblk.ff_name);

   // deleting *.HLP files
   if( strstr( ffblk.ff_name, ".HLP") )
   {
    #ifdef UNLINK_LOGS
     unlink( ffblk.ff_name );
    #endif

    #ifdef DEBUG
     printf("\n %s deleted...", ffblk.ff_name);
    #endif
   }

   done = findnext(&ffblk);
 }



 //--- UNDO *.NOT ---

 #ifdef DEBUG
  printf("\n Undo *.NOT logs:");
 #endif


 done = findfirst("*.*",&ffblk,0);

 while (!done)
 {
  if( strstr( ffblk.ff_name, ".NOT") )
   UndoAlfLog( Device, Party, ffblk.ff_name );

   done = findnext(&ffblk);
 }



 //--- UNDO *.ALF ---

 #ifdef DEBUG
  printf("\n Undo *.ALF logs and bubble up to *.OKA:");
 #endif


 done = findfirst("*.*",&ffblk,0);

 while (!done)
 {
  if( strstr( ffblk.ff_name, ".ALF") )
   UndoAlfLog( Device, Party, ffblk.ff_name, DO_UNLINK, BUBBLE_UP );

   done = findnext(&ffblk);
 }



 //--- ROLLBACK *.CML ---

 #ifdef DEBUG
  printf("\n Rollback *.CML logs:");
 #endif


 done = findfirst("*.*",&ffblk,0);

 while (!done)
 {
  if( strstr( ffblk.ff_name, ".CML") )
   RecoverUsingCml( ffblk.ff_name );

   done = findnext(&ffblk);
 }



 //--- WORK ON REST OF *.OKA ---

 #ifdef DEBUG
  printf("\n Rollback *.OKA logs:");
 #endif


 done = findfirst("*.*",&ffblk,0);

 while (!done)
 {
  if( strstr( ffblk.ff_name, ".OKA") )
   RollbackOkaLog( Device, Party, ffblk.ff_name );

   done = findnext(&ffblk);
 }

 return ERR_FS_NO_ERROR;
}

//- EOF --------------------------------------------------------------------------------



