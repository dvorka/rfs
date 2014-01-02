
#include "cacheman.h"


//#define DEBUG
//#define SEE_ERROR

extern byte        BitMask[16];


CacheManStruct     far *CacheMan;

SwapPartysQueue   far *SwapPartys; // list of swap devices in system
DataPartysQueue   far *DataPartys; // list of data devices in system

//------------------------------------------------------------------------------------

word CacheManInit( dword MemForData, bool CachingON, bool SwappingON )
// MemForData: how many bytes can cache allocate for data
// - touto fci by se mely vlozit pametove limity - nejlepe podle konfiguraku

{

 #ifdef DEBUG
  printf("\n\n INITializing CacheMan with options:"
	 "\n  Memory reserved for data: %luB"
	 "\n  Caching on (Yes=1/No=0) : %d"
	 "\n  Swapping on (Yes=1/No=0): %d"
	 ,
	 MemForData,
	 CachingON,
	 SwappingON
	);
 #endif

 // create CacheMan
 CacheMan   =new CacheManStruct;

 if( !CacheMan ) { printf("\n Not enough far memory in %s line %d ( Cache init )...", __FILE__, __LINE__ ); exit(0); }

 // init CacheManStructure
 CacheMan->CacheOn=CachingON;
 CacheMan->SwapOn =SwappingON;

 CacheMan->MemQuote=MemForData;
 CacheMan->MemUsed=0;
 CacheMan->MemRest=MemForData;

 // where swaps list is filled in OpenFileSystem using congig file

 #ifdef DEBUG
  printf("\n CacheMan initialized..." );
 #endif

 return ERR_FS_NO_ERROR;
}

//------------------------------------------------------------------------------------

word CacheManDeinit( void )
// frees service structures allocated by CacheMan
{
 #ifdef DEBUG
  printf("\n\n Closing CacheMan..." );
 #endif

 delete CacheMan;

 #ifdef DEBUG
  printf("\n CacheMan closed..." );
 #endif

 return ERR_FS_NO_ERROR;
}

//------------------------------------------------------------------------------------

// record for log of commiting package

#define BEGIN_COPY      0xBBBBBBBBlu
#define NOT_LAST_RECORD	0xFFFFFFFFlu
#define CACHE_INTERVAL	0xCCCCCCCClu

typedef struct
{
 dword TypeOfRecord;      // for transaction

 byte Device;		// 0, ..
 byte Party;            // 1, ..
 byte SwapDevice;
 byte SwapParty;

 dword Logical;
 dword LogicalSwap;

 word  Lng;
}
LogPackRecord;		// sizeof()==15

//------------------------------------------------------------------------------------

word CacheManCommitPackage( word PackageID, word Flag )
// - list of sectors which should be commited to disc
// ! PutSector DOESN'T writes sector into file, but writes it to  !
// ! cache swap or stays in memory with dirty flag. Only and ONLY !
// ! THIS FUNCTION CAN WRITE DATA DIRECTLY TO DISC.               !
//
// - creates *.CML ( commit log ) file for recovery
//
// What it does:
//  - writes data from cache to swap && writes records to CML log
//  - writes records about Pack-CacheMan swapped data to CML log
//  - writes magic record "BEGIN_COPY"  which means: every data on swap,
//    beginning to copy data from swap to data party
//  - delete OKA log ( space allocated will be used => no rollback needed )
//  - copy data from swap to party
//  - unlink CML log == commit


// DODELAT: tam kde se mazou swap descriptory odalokovat misto na swapu
// to same udelat se sektory po pruchodu cache - je to napsano v logu
// do logu tedy oznacit ty sektory, ktere sly  z cache, ty potom
// odalokovat podle logu, protoze v cache nejsou

{
 void             far *Buffer;
 PackageQueueItem far *Pack;
 SwapDescrItem    far *HelpSwapDescr,
		  far *SwapDescr;
 CacheDescrItem   far *HelpCacheDescr,
		  far *CacheDescr;
 byte             Device,
		  Party;
 word		  Number,
		  GetNumber,
		  ErrorCode;
 dword		  LogicalSwap,
		  Logical;
 int		  Handle;
 LogPackRecord    LogRecord;

 char            String[8+1+3 +1],
		 LogFileName[8+1+3 +1];



 // begin of serious word
 #ifdef DEBUG
  if( Flag==FPACK_DELETE )
   printf("\n\n COMMIT PACKAGE: commiting and DELETEing package %d...", PackageID );
  else
   printf("\n\n COMMIT PACKAGE: commiting package %d...", PackageID );
 #endif


 // first commit package content, later if FPACK_DELETE => delete package

 // search for package
 Pack=CacheMan->PackQueue.FindPackageID( PackageID );

 if( !Pack )
 {
  #ifdef SEE_ERROR
   printf("\n Error: package %u not exist...", PackageID );
  #endif

  return ERR_FS_PACKAGE_NOT_EXIST;
 }



 // log name template
 // Off
 // 1     device id 0..
 // 2     party  id 1..
 // 5     packID
 // .CML  rest

		 LogFileName[0]=Pack->Device+'0'; // first cipher is device
		 LogFileName[1]=Pack->Party+'0';  // first cipher is device
		 LogFileName[2]=0;

 // now add packageID to be unicate
 long value=(long)PackageID;

 ltoa( value, String, 10 );
 strcat( LogFileName, String );
 strcat( LogFileName, ".CML" );

 // create log file
 if ((Handle = open( LogFileName, O_CREAT  | O_RDWR | O_BINARY,
				  S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't create log file %s...", LogFileName );
  #endif

  return ERR_FS_LOG_NOT_CREATED;
 }

 #ifdef DEBUG
  printf("\n Created log file %s...", LogFileName );
 #endif



 //--- SEARCH CONTENT OF CACHE ---
 #ifdef DEBUG
  printf("\n Searching cache: NOTDIRTY delete, DIRTY save to swap...");
 #endif

 // DODELAT
 // kdyz je caching OFF tak to nezkoumej!
 // ( finta jak to preskocit cache descr=tail )
 // v pripade, ze je swap full, mely by se deskriptory po
 // primem zapisu mazat

 CacheDescr=(CacheDescrItem far *)Pack->CacheQueue.Head->Next;

 while( CacheDescr != Pack->CacheQueue.Tail )
 {

  if( CacheDescr->Dirty==FALSE && Flag==FPACK_DELETE )
  {
   // delete descriptor

   HelpCacheDescr=(CacheDescrItem far *)CacheDescr->Next;



   delete CacheDescr->CacheObject; // is better than farfree( Item->CacheObject );
				   // because was created using new

   #ifdef DEBUG
    printf("\n Interval <%lu, %lu> CLEAN => destroy descriptor:",
	   CacheDescr->FirstLogicalSector,
	   CacheDescr->FirstLogicalSector+(dword)(CacheDescr->IntervalLng)-1ul
	  );
   #endif

   Pack->CacheQueue.Delete((PriQueueItem far *)CacheDescr);


   // take next
   CacheDescr=HelpCacheDescr;

  }
  else // object is dirty must be saved
  {
   #ifdef DEBUG
    printf("\n Interval <%lu, %lu> DIRTY => must be swapped...",
	   CacheDescr->FirstLogicalSector,
	   CacheDescr->FirstLogicalSector+(dword)(CacheDescr->IntervalLng)-1ul
	  );
   #endif

   //--- DIRTY CACHE OBJECTS TO SWAP ---


   Device=Pack->Device;
   Party=Pack->Party;

   Logical=CacheDescr->FirstLogicalSector;
   Number=CacheDescr->IntervalLng;

   Buffer=CacheDescr->CacheObject;


   if( CacheMan->SwapOn )
   {
    // no search => write it to swap ( not marked, only )

    ErrorCode=CacheManAllocateSector( Pack->SwapsTo->Device,
				      Pack->SwapsTo->Party,
				      0,
				      Number,
				      LogicalSwap,
				      GetNumber,
				      Pack->PackageID,
				      FPACK_NOTHING
				    );

    if( ErrorCode || !LogicalSwap ) // unsuccessfull alloc on swap => direct
    {
     // swap full or corrupted => NOT CONSISTENT WRITE DIRECTLY TO FILE
     #ifdef DEBUG
      printf("\n Swap party is full ( Alloc says it )...");
     #endif

     if( SaveSectorThrough( Device, Party, Logical, Number, Buffer ) )
     {
      #ifdef SEE_ERROR
       printf("\n Error writing sec LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
      #endif

      return ERR_FS_FATAL_ERROR;
     }

     #ifdef DEBUG
      printf("\n NON CONSISTENT sector write to LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
     #endif

     // take next descriptor
     CacheDescr=(CacheDescrItem far *)CacheDescr->Next;
     continue;
    }
    else // swap space allocated
    {
     // write record to log

     #ifdef DEBUG
      printf("\n LOG RECORD for:  SwpDev %d SwpParty %d SwpSec %lu Dev %d Party %d Sec %lu Lng %d...",
		  Pack->SwapsTo->Device,
		  Pack->SwapsTo->Party,
		  LogicalSwap,
		  Pack->Device,
		  Pack->Party,
		  Logical,
		  Number
	    );
     #endif


     // write record to log...
     LogRecord.TypeOfRecord=CACHE_INTERVAL; // isn't in swpdescr => uses secfree

     LogRecord.Device=Pack->Device;
     LogRecord.Party=Pack->Party;
     LogRecord.SwapDevice=Pack->SwapsTo->Device;
     LogRecord.SwapParty=Pack->SwapsTo->Party;
     LogRecord.Logical=Logical;
     LogRecord.LogicalSwap=LogicalSwap;
     LogRecord.Lng=Number;

     if( write( Handle, &LogRecord, sizeof(LogPackRecord) ) != sizeof(LogPackRecord) )
     {
      #ifdef SEE_ERROR
       printf("\n Error writing Log file...");
      #endif

      return ERR_FS_LOG_WR;
     }
     #ifdef DEBUG
     else
      printf("\n Record ( cache ) written to Log file...");
     #endif



     // now write data to swap
     if( SaveSectorThrough( Pack->SwapsTo->Device,
			    Pack->SwapsTo->Party,
			    LogicalSwap, Number, Buffer
			  )
       )
     {
      #ifdef SEE_ERROR
       printf("\n Error writing to SWAP sec LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
      #endif

      return ERR_FS_FATAL_ERROR;
     }

     #ifdef DEBUG
      printf("\n Interval SWAPPED...");
     #endif


     // take next
     CacheDescr=(CacheDescrItem far *)CacheDescr->Next;
     continue;
    }
   }
   else // swap full (limit) => NOT CONSISTENT WRITE DIRECTLY TO FILE
   {
    #ifdef DEBUG
     printf("\n Swap full or swapping OFF...");
    #endif

    if( SaveSectorThrough( Device, Party, Logical, Number, Buffer ) )
    {
     #ifdef SEE_ERROR
      printf("\n Error writing sec LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
     #endif

     return ERR_FS_FATAL_ERROR;
    }

    #ifdef DEBUG
     printf("\n NON CONSISTENT sector write to LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
    #endif

    // take next
    CacheDescr=(CacheDescrItem far *)CacheDescr->Next;
    continue;
   }
  }    	// else dirty
 } 	// while


 //--- SWAP DESCRIPTORS CONTENT TO LOG ---
 #ifdef DEBUG
  printf("\n Going through swap descriptors and write content to log...");
 #endif

 SwapDescr=(SwapDescrItem far *)Pack->SwapQueue.Head->Next;

 while( SwapDescr!=Pack->SwapQueue.Tail )
 {

  // write record to log...
  LogRecord.TypeOfRecord=NOT_LAST_RECORD;

  LogRecord.Device=Pack->Device;
  LogRecord.Party=Pack->Party;
  LogRecord.SwapDevice=Pack->SwapsTo->Device;
  LogRecord.SwapParty=Pack->SwapsTo->Party;
  LogRecord.Logical=SwapDescr->FirstLogicalSector;
  LogRecord.LogicalSwap=SwapDescr->LogicalInSwap;
  LogRecord.Lng=SwapDescr->IntervalLng;

  #ifdef DEBUG
   printf("\n LOG RECORD for:  SwpDev %d SwpParty %d SwpSec %lu Dev %d Party %d Sec %lu Lng %d...",
		LogRecord.SwapDevice,
		LogRecord.SwapParty,
		LogRecord.LogicalSwap,
		LogRecord.Device,
		LogRecord.Party,
		LogRecord.Logical,
		LogRecord.Lng
	    );
  #endif

     if( write( Handle, &LogRecord, sizeof(LogPackRecord) ) != sizeof(LogPackRecord) )
     {
      #ifdef SEE_ERROR
       printf("\n Error writing Log file...");
      #endif

      return ERR_FS_LOG_WR;
     }
     #ifdef DEBUG
     else
      printf("\n Record (swap) written to Log file...");
     #endif

  // take next
  SwapDescr=(SwapDescrItem far *)SwapDescr->Next;
 } 	// while


 //--- CHECKPOINT: "BEGIN COPY" TO LOG AND CLOSE LOG FILE ---
 #ifdef DEBUG
  printf("\n Write BEGIN COPY checkpoint to log, closing log file...");
 #endif

 // write record to log, means last record == BEGIN COPY...
 LogRecord.TypeOfRecord=BEGIN_COPY;

 if( write( Handle, &LogRecord, sizeof(LogPackRecord) ) != sizeof(LogPackRecord) )
 {
  #ifdef SEE_ERROR
   printf("\n Error writing Log file...");
  #endif

  return ERR_FS_LOG_WR;
 }
 #ifdef DEBUG
  else
   printf("\n Record written (Last) to Log file...");
 #endif

 close( Handle );



 //--- DELETE OKA FILE => SPACE WILL BE USED ---
 // construct it's name
 char OkaFileName[8+1+3+ 1];
 word		i;
 int  Handle2;
 long OkaRecords;

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



 //--- CACHE CONTENT WRITE TO FILE ---
 #ifdef DEBUG
  printf("\n Write cache content to file...");
 #endif


 CacheDescr=(CacheDescrItem far *)Pack->CacheQueue.Head->Next;

 while( CacheDescr!=Pack->CacheQueue.Tail )
 {
  #ifdef DEBUG
   printf("\n Interval <%lu, %lu> Lng %u will be written to file...",
	  CacheDescr->FirstLogicalSector,
	  CacheDescr->FirstLogicalSector+(dword)(CacheDescr->IntervalLng)-1ul,
	  CacheDescr->IntervalLng
	 );
  #endif


  if( SaveSectorThrough(
			 Pack->Device,
			 Pack->Party,
			 CacheDescr->FirstLogicalSector,
			 CacheDescr->IntervalLng,
			 CacheDescr->CacheObject
		       )
    )
  {
   #ifdef SEE_ERROR
    printf("\n Error writing sec LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
   #endif

   return ERR_FS_FATAL_ERROR;
  }

  #ifdef DEBUG
    printf("\n Written to Dev %d Party %d Logical %lu Number %u...",
	    Pack->Device,
	    Pack->Party,
	    CacheDescr->FirstLogicalSector,
	    CacheDescr->IntervalLng
	  );
  #endif


  if( Flag==FPACK_DELETE )
  {
   // delete descriptor

   HelpCacheDescr=(CacheDescrItem far *)CacheDescr->Next;


   delete CacheDescr->CacheObject; // is better than farfree( Item->CacheObject );
				   // because was created using new

   Pack->CacheQueue.Delete((PriQueueItem far *)CacheDescr);

   // take next
   CacheDescr=HelpCacheDescr;
  }
  else
  {
   // take next
   HelpCacheDescr=(CacheDescrItem far *)CacheDescr->Next;

   CacheDescr=HelpCacheDescr;
  }
 } 	// while


 // now free sectors allocate on swap for cache using log


 // open log file
 if ((Handle = open( LogFileName, O_RDONLY | O_BINARY,
				  S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't open log file...");
  #endif

  return ERR_FS_LOG_OPEN;
 }


 do
 {
     if( read( Handle, &LogRecord, sizeof(LogPackRecord) ) != sizeof(LogPackRecord) )
     {
      #ifdef SEE_ERROR
       printf("\n Error reading log file %s...", LogFileName );
      #endif

      return ERR_FS_FATAL_ERROR;
     }

     if( LogRecord.TypeOfRecord==CACHE_INTERVAL )
     {
      // cache descriptor => free sectors on swap
      CacheManFreeSector( LogRecord.SwapDevice,
			  LogRecord.SwapParty,
			  LogRecord.LogicalSwap,
			  LogRecord.Lng,
			  Pack->PackageID, FPACK_NOTHING
			);
     }
 }
 while( LogRecord.TypeOfRecord==CACHE_INTERVAL );

 close( Handle );

 //--- SWAP CONTENT COPY TO FILE ---
 #ifdef DEBUG
  printf("\n Copy swap content to file...");
 #endif

 // in while() do:
 // find descriptor, allocate buffer, load from swap, save to data,
 // free buffer, free swap secs, delete descriptor


 SwapDescr=(SwapDescrItem far *)Pack->SwapQueue.Head->Next;

 while( SwapDescr!=Pack->SwapQueue.Tail )
 {
  #ifdef DEBUG
   printf("\n Interval <%lu, %lu> => Lng %u will be written to file...",
	  SwapDescr->FirstLogicalSector,
	  SwapDescr->FirstLogicalSector+(dword)(SwapDescr->IntervalLng)-1ul,
	  SwapDescr->IntervalLng
	 );
  #endif


  Buffer=farmalloc((dword)(SwapDescr->IntervalLng)*512ul);
  if( !Buffer ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }


  // load data from swap

  if( LoadSectorThrough( Pack->SwapsTo->Device,
			 Pack->SwapsTo->Party,
			 SwapDescr->LogicalInSwap,
			 SwapDescr->IntervalLng,
			 Buffer
		       )
    )
  {
   #ifdef SEE_ERROR
    printf("\n Error reading: device %d party %d SwpSec %lu Lng %d... ",
	       Pack->SwapsTo->Device,
	       Pack->SwapsTo->Party,
	       SwapDescr->LogicalInSwap,
	       SwapDescr->IntervalLng
	  );
   #endif

   farfree( Buffer );
   return ERR_FS_FATAL_ERROR;
  }

  #ifdef DEBUG
   printf("\n Sector read from Dev %d Party %d SwpSec %lu Lng %d... ",
	       Pack->SwapsTo->Device,
	       Pack->SwapsTo->Party,
	       SwapDescr->LogicalInSwap,
	       SwapDescr->IntervalLng
	  );
  #endif


  // save data to party

  if( SaveSectorThrough( Pack->Device, Pack->Party,
			 SwapDescr->FirstLogicalSector,
			 SwapDescr->IntervalLng,
			 Buffer
		       )
    )
  {
   #ifdef SEE_ERROR
    printf("\n Error writing: device %d party %d SwpSec %lu Lng %d... ",
		Pack->Device, Pack->Party,
		SwapDescr->FirstLogicalSector,
		SwapDescr->IntervalLng
	  );
   #endif

   farfree( Buffer );
   return ERR_FS_FATAL_ERROR;
  }

   #ifdef DEBUG
    printf("\n Sector written to device %d party %d SwpSec %lu Lng %d... ",
		Pack->Device, Pack->Party,
		SwapDescr->FirstLogicalSector,
		SwapDescr->IntervalLng
	  );
   #endif


  // delete buffer
  farfree( Buffer );

  //------------------------
  //- ONLY IF FPACK_DELETE -
  //------------------------

  // if i would free it => in memory stays it => free would be done
  // twice => error

  if( Flag==FPACK_DELETE )
  {
  // free secs on swap
  CacheManFreeSector( Pack->SwapsTo->Device,
		      Pack->SwapsTo->Party,
		      SwapDescr->LogicalInSwap,
		      SwapDescr->IntervalLng,
		      Pack->PackageID, FPACK_NOTHING
		    );
  }


  if( Flag==FPACK_DELETE )
  {
   // if package will be deleted => delete swap descriptor

   #ifdef DEBUG
    printf("\n Destroy descriptor...");
   #endif

   HelpSwapDescr=(SwapDescrItem far *)SwapDescr->Next;

   Pack->SwapQueue.Delete((PriQueueItem far *)SwapDescr);

   // take next
   SwapDescr=HelpSwapDescr;
  }
  else
  {
   HelpSwapDescr=(SwapDescrItem far *)SwapDescr->Next;

   // take next
   SwapDescr=HelpSwapDescr;
  }
 } 	// while


 //--- CHECKPOINT: "BEGIN FREE CACHE SECS ON SWAP USING LOG" ---


 //--- FREE SECS ON SWAP USING LOG ---


 //--- DELETE LOG ---
 #ifdef DEBUG
  printf("\n Delete log %s...", LogFileName );
 #endif

 #ifdef UNLINK_LOGS
  unlink(LogFileName);
 #endif


 //--- DESTROY PACKAGE AND FREE ID ---

 if( Flag==FPACK_DELETE )
 {
  #ifdef DEBUG
   printf("\n Deleting package %d and freeing its ID...", PackageID );
  #endif

  CacheMan->PackIDs.FreeID( Pack->PackageID );

  CacheMan->PackQueue.DeletePack( Pack );
 }


 #ifdef DEBUG
  printf("\n PACKAGE %d COMMITED...", PackageID );
 #endif

 return ERR_FS_NO_ERROR;
}

//------------------------------------------------------------------------------------

word CacheManCompleteCommit( word Flag ) // flag DELETE/NOTHING
// - writes whole content of cache to HD
{
 PackageQueueItem far *Pack,
		  far *HelpPack;

 #ifdef DEBUG
  printf("\n\n COMPLETE COMMIT of CacheMan...");
 #endif

 // go through packages and commit each separately

 Pack=(PackageQueueItem far *)CacheMan->PackQueue.Head->Next;

 while( Pack!=CacheMan->PackQueue.Tail )
 {

  HelpPack=(PackageQueueItem far *)Pack->Next;

  // HelpPack because this can delete it
  CacheManCommitPackage( Pack->PackageID, Flag );

  // take next
  Pack=HelpPack;
 }

 #ifdef DEBUG
  printf("\n");
 #endif

 return ERR_FS_NO_ERROR;
}

//------------------------------------------------------------------------------------


			#include "ALLOCFRE.CPP"

/*

word CacheManAllocateSector( byte  Device,     byte Party,
			     dword Previous,   word Number,
			     dword &Logical,   word &GetNumber,
			     word  &PackageID, word Flag
			   )

//------------------------------------------------------------------------------------

word CacheManFreeSector( byte  Device,     byte Party,
			 dword Logical,    word Number,
			 word  &PackageID, word Flag
			)

*/

//------------------------------------------------------------------------------------

word CacheManLoadSector( byte  Device,     byte Party,
			 dword Logical,    word Number,
			 word  &PackageID, word Flag, void far *Buffer )
// - loads block of sectors
{
 PackageQueueItem far *Pack;
 CacheDescrItem   far *CacheDescr, far *CacheDescrFind, far *CacheFrom;
 SwapDescrItem    far *SwapDescrFind, far *SwapFrom;
 DataPartysItem   far *DataDev;

 dword            LogicalSwap,
		  L, H;
 long		  i, From, FirstBit, LNumber;
 word		  Number2;


 Device&=0x0F;


 switch( Flag )
 {
  case FPACK_CREAT:
       // if creating new package neither cache nor swap is searched

       #ifdef DEBUG
	printf("\n\n Creating new package for interval < %lu, %lu > LoadSec...", Logical, Logical+(dword)Number-1lu );
       #endif

       // create new package
       Pack=new PackageQueueItem;

       if( !Pack ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

       Pack->PackageID=PackageID=CacheMan->PackIDs.GetID();
       #ifdef DEBUG
	printf(" ID=%d...",Pack->PackageID );
       #endif
       Pack->Device =Device;
       Pack->Party  =Party;
       Pack->Dirty  =FALSE;

       DataDev=DataPartys->FindDevParty( Device, Party );

       if( !DataDev )
       {
	#ifdef SEE_ERROR
	 printf("\n Error: Dev %u Party %u not in system...", Device, Party );
        #endif

	return ERR_FS_FATAL_ERROR;
       }

       Pack->SwapsTo=DataDev->Swap;

       if( !Pack->SwapsTo )
       {
	#ifdef SEE_ERROR
	 printf("\n Error: no record in WhereSwaps for this party..." );
	#endif

	return ERR_FS_FATAL_ERROR;
       }

       // add package to priority queue
       CacheMan->PackQueue.Insert( Pack );

       // if creating new package neither cache nor swap is searched...
       // data are loaded directly
       if( LoadSectorThrough( Device, Party, Logical, Number, Buffer ) )
       {
	#ifdef SEE_ERROR
	 printf("\n Error reading sec LogHDSec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	#endif

	return ERR_FS_FATAL_ERROR;
       }

       #ifdef DEBUG
	printf("\n Sector read from LogHDSec %lu, device 0x%x, party %u... ", Logical, Device, Party );
       #endif

       //--- TRY PUT IT TO CACHE ---
       // if there is free capacity, put it to cache ( copy from user buf
       // to cache )

       if( CacheMan->MemRest >= Number*512u && CacheMan->CacheOn )
       {
	// yes there is space
	CacheMan->MemRest -= Number*512u;
	#ifdef DEBUG
	 printf("\n CacheObj will be created, MemRest will be: %lu", CacheMan->MemRest );
	#endif

	// create new cache descriptor in current package
	CacheDescr=new CacheDescrItem;

	if( !CacheDescr ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	CacheDescr->FirstLogicalSector=Logical;
	CacheDescr->IntervalLng=Number;
	CacheDescr->Dirty=FALSE;
	CacheDescr->CacheObject=(byte far *)farmalloc((dword)Number*512lu);

	if( !CacheDescr->CacheObject ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	// copy data from user buffer,
	// for future HUGE models size_t is only word => used for()

	for( i=(long)Number*512l-1l; i>=0l; i-- )
	 CacheDescr->CacheObject[i]=((byte far*)Buffer)[i];

	Pack->CacheQueue.Insert( CacheDescr );

	#ifdef DEBUG
	 printf("\n Interval <%lu, %lu> inserted to cache... ", Logical, Logical+(dword)(Number)-1ul);
	#endif
       }
       #ifdef DEBUG
       else
	printf("\n Cache is full or caching OFF...");
       #endif
       // else not enough cache memory rests

       // swap has no sense - read from party is same as read from swap

       return ERR_FS_NO_ERROR;

   case FPACK_ADD:
       // - package already exists, find it and search it cache+swap for data
       // - create bitmap ( bitmap: bit means sector )
       //
       // (i) swap -  load from swap save intervals compl. or part. over
       //	      each other, mark in bmp
       //             ( go throug whole buffer )
       // (ii) cache - each cache object P/C over load+bmp
       // (iii) rest - load rest, if free space in cache - create cache obj
       //              ( swap has no sense - read as read )

       #ifdef DEBUG
	printf("\n\n Adding to package %u interval < %lu, %lu > LoadSec...", PackageID, Logical, Logical+(dword)Number-1lu );
       #endif

       // search for package
       Pack=CacheMan->PackQueue.FindPackageID( PackageID );

       if( !Pack )
       {
	#ifdef SEE_ERROR
	 printf("\n Error: package %u not exist => exiting...", PackageID );
	#endif

	return ERR_FS_PACKAGE_NOT_EXIST;
       }

       // --- CREATE BITMAP ---
       // Logical has off 0 in bitmap => X-Logical is off in bitmap
       BMP far *Bmp=new BMP( Number );

       if( !Bmp ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

       //--- TRY SWAP ---

       // now search on swap for interv. which would P/C over my
       // SwapFrom      ... where begin to search
       // SwapDescrFind ... what was find

       SwapFrom = SwapDescrFind = (SwapDescrItem far* )Pack->SwapQueue.Head->Next;

       while( SwapDescrFind ) // while something found
       {
	L=Logical;
	H=Logical+(dword)Number-1;	// , -1 BECAUSE <x,..

	SwapDescrFind
	=
	Pack->SwapQueue.FindIsCPOver( L, H, SwapFrom );

	if( !SwapDescrFind ) break; // nothing found

	// something found

	// set bits in bitmap
	Bmp->SetInterval( L-Logical, H-Logical );

	#ifdef DEBUG
	 printf("\n Loading %luB from Swap == < %lu, %lu >...", (H-L+1)*512l, L, H );
	#endif

	// read data from swap to user buffer ( use descriptor )

	Number2=(word)H-(word)L+1u;

	LogicalSwap=SwapDescrFind->LogicalInSwap + L-Logical;

	  if( LoadSectorThrough( Pack->SwapsTo->Device,
				 Pack->SwapsTo->Party,
				 LogicalSwap,
				 Number2,
				 Buffer
			       )
	    )
	  {
	   #ifdef SEE_ERROR
	    printf("\n Error reading from SWAP sec LogPartySec %lu, device 0x%x, party %u... ",
		   LogicalSwap, Pack->SwapsTo->Device, Pack->SwapsTo->Party );
	   #endif

	   delete Bmp;

	   return ERR_FS_FATAL_ERROR;
	  }

	  #ifdef DEBUG
	   printf("\n LOAD from SWAP int. LogSec: %lu, lng %u to SwapLogSec %lu ( orig %lu )...",
		  Logical, Number2, LogicalSwap, SwapDescrFind->LogicalInSwap );
	  #endif

	// try the rest
	SwapFrom = (SwapDescrItem far* )SwapDescrFind->Next;

       } // while in swap descriptors queue


       //--- TRY CACHE ---

       // now search in cache for interv. which would P/C over my

       CacheFrom = CacheDescrFind = (CacheDescrItem far* )Pack->CacheQueue.Head->Next;

       while( CacheDescrFind ) // while something found
       {
	L=Logical;
	H=Logical+(dword)Number-1;	// , -1 BECAUSE <x,..

	CacheDescrFind
	=
	Pack->CacheQueue.FindIsCPOver( L, H, CacheFrom );

	if( !CacheDescrFind ) break; // nothing found

	// something found

	// set bits in bitmap
	Bmp->SetInterval( L-Logical, H-Logical );

	#ifdef DEBUG
	 printf("\n Loading %luB from cache == < %lu, %lu >...", (H-L+1)*512l, L, H );
	#endif

	// copy data from cache to user buffer
	for( i=(H-L+1)*512l-1l; i>=0l; i-- )
	 ((byte far*)Buffer)[ (L-Logical)*512l+i ]
	 =
	 CacheDescrFind->CacheObject
	  [ (L-CacheDescrFind->FirstLogicalSector)*512l+i ];

	// try the rest
	CacheFrom = (CacheDescrItem far* )CacheDescrFind->Next;

       } // while


       // --- DIRECT LOAD OF REST FROM DISK and try put it to cache ---

       #ifdef DEBUG
	printf("\n REST - load it...");
       #endif

       From=0;	// search from beg of Bmp

       while( Bmp->FindHole( From, FirstBit, LNumber ) )
       {
	From=FirstBit+LNumber+1l; 	// next time search after hole
	L=(long)FirstBit;	       	// L is sec offset in buffer
	Number=(word)LNumber;

	// load data to user buffer
	if( LoadSectorThrough( Device, Party, Logical+L, Number, Buffer ) )
	{
	 #ifdef SEE_ERROR
	  printf("\n Error reading sec LogHDSec %lu, device 0x%x, party %u... ", Logical+L, Device, Party );
	 #endif

	 return ERR_FS_FATAL_ERROR;
	}

	#ifdef DEBUG
	 printf("\n Sector read from LogHDSec %lu, device 0x%x, party %u... ", Logical+L, Device, Party );
	 printf("\n  Logical %lu, Logical+L %lu", Logical, Logical+L );
	#endif


       // if there is free capacity, put it to cache ( copy from user buf to cache )
       if( CacheMan->MemRest >= Number*512 && CacheMan->CacheOn )
       {
	//--- SAVE REST DATA TO CACHE ---

	// yes there is some free space
	CacheMan->MemRest -= Number*512;
	#ifdef DEBUG
	 printf("\n CacheObj will be created, MemRest will be: %lu", CacheMan->MemRest);
	#endif

	// create new cache descriptor in current package
	CacheDescr=new CacheDescrItem;

	if( !CacheDescr ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	CacheDescr->FirstLogicalSector=Logical+L; // shift begin
	CacheDescr->IntervalLng=Number;
	CacheDescr->Dirty=FALSE;
	CacheDescr->CacheObject=(byte far *)farmalloc((dword)Number*512lu);

	if( !CacheDescr->CacheObject ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	// copy loaded data from user buffer to cache

	for( i=LNumber*512l-1l; i>=0l; i-- )
	 CacheDescr->CacheObject[i]
	 =
	 ((byte far*)Buffer)[ L*512l+i ]; // L je moc velke chtelo by to L-Logical ne?

	Pack->CacheQueue.Insert( CacheDescr );

	#ifdef DEBUG
	 printf("\n Interval <%lu, %lu> inserted to cache, L %lu... ",
	       Logical+L,
	       Logical+L+(dword)(Number)-1ul, L );
	#endif

       }
       #ifdef DEBUG
	else
	  printf("\n Cache full or caching OFF...");
       #endif

       } // while searching holes

       // --- NOW EVERYTHING IS LOADED ---

       delete Bmp;

       return ERR_FS_NO_ERROR;

  default:
       // FPACK_NOTHING => direct load to callers buffer
       // doesn't try to put it to package - it is done using FPACK_ADD

       // IT IS DANGEROUS AND USEFULL BECAUSE DON'T CHECKS DIRTY SECS
       // IN CACHE/SWAP BUT CALLER TO MUZE CHTIT, NA NECO SE TO MUZE HODIT
       // => use it for loading sectors independently on cache content

       #ifdef DEBUG
	printf("\n\n Load through interval < %lu, %lu > LoadSec...", Logical, Logical+(dword)Number-1lu );
       #endif

       if( LoadSectorThrough( Device, Party, Logical, Number, Buffer ) )
       {
	#ifdef SEE_ERROR
	 printf("\n Error reading sec LogHDSec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	#endif

	return ERR_FS_FATAL_ERROR;
       }

       #ifdef DEBUG
	printf("\n Sector read from LogHDSec %lu, device 0x%x, party %u... ", Logical, Device, Party );
       #endif



       return ERR_FS_NO_ERROR;
 }

}

//------------------------------------------------------------------------------------

word CacheManSaveSector( byte Device,     byte Party,
			 dword Logical,   word Number,
			 word &PackageID, word Flag, void far *Buffer )
// - saves block of sectors
{
 PackageQueueItem far *Pack;
 CacheDescrItem   far *CacheDescr, far *CacheDescrFind, far *CacheFrom;
 SwapDescrItem    far *SwapDescr, far *SwapDescrFind, far *SwapFrom;
 DataPartysItem   far *DataDev;
 dword            LogicalSwap,
		  L, H;
 long		  i, From, FirstBit, LNumber;
 word		  GetNumber,
		  ErrorCode,
		  Number2;

 Device&=0x0F;


 switch( Flag )
 {
  case FPACK_CREAT:
       // - when creating new package neither cache nor swap is searched
       // - create new package
       // - if there is free capacity, put it to cache ( copy from
       //   user buf to cache )
       // - if not enough cache memory => try swap
       // - if swap full => NOT CONSISTENT WRITE DIRECTLY TO FILE

       #ifdef DEBUG
	printf("\n\n Creating new package for interval < %lu, %lu > SaveSec...", Logical, Logical+(dword)Number-1lu );
       #endif

       // create new package
       Pack=new PackageQueueItem;

       if( !Pack ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

       Pack->PackageID=PackageID=CacheMan->PackIDs.GetID(); // for now
       #ifdef DEBUG
	printf(" ID=%d...",Pack->PackageID );
       #endif
       Pack->Device =Device;
       Pack->Party  =Party;       // where secs from package live
       Pack->Dirty  =TRUE;	  // because of write

       DataDev=DataPartys->FindDevParty( Device, Party );

       if( !DataDev )
       {
	#ifdef SEE_ERROR
	 printf("\n Error: Dev %u Party %u not in system...", Device, Party );
	#endif

	return ERR_FS_FATAL_ERROR;
       }

       Pack->SwapsTo=DataDev->Swap;
				  // found where swaps

       if( !Pack->SwapsTo )
       {
	#ifdef SEE_ERROR
	 printf("\n Error: no record about where swaps for this party..." );
	#endif

	return ERR_FS_FATAL_ERROR;
       }

       // add package to priority queue
       CacheMan->PackQueue.Insert( Pack );

       // if there is free capacity, put it to cache ( copy from user buf to cache )
       if( CacheMan->MemRest >= Number*512 && CacheMan->CacheOn )
       {
	// yes there is space
	CacheMan->MemRest -= Number*512;
	#ifdef DEBUG
	 printf("\n CacheObj will be created, MemRest.Data will be: %lu", CacheMan->MemRest);
	#endif

	// create new cache descriptor in current package
	CacheDescr=new CacheDescrItem;

	if( !CacheDescr ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	CacheDescr->FirstLogicalSector=Logical;
	CacheDescr->IntervalLng=Number;
	CacheDescr->Dirty=TRUE;
	CacheDescr->CacheObject=(byte far *)farmalloc((dword)Number*512lu);

	if( !CacheDescr->CacheObject ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	// copy data from user buffer
	for( i=(long)Number*512l-1l; i>=0l; i-- )
	 CacheDescr->CacheObject[i]=((byte far*)Buffer)[i];

	Pack->CacheQueue.Insert( CacheDescr );

	#ifdef DEBUG
	 printf("\n Interval <%lu, %lu> inserted to cache... ", Logical, Logical+(dword)(Number)-1ul);
	#endif
       }
       else // else not enough cache memory rests => try swap
       {
	#ifdef DEBUG
	 printf("\n Cache is full or caching OFF, trying swap...");
	#endif

	// last possibility for consistent writes => search place on swap

	if( CacheMan->SwapOn )
	{
	 // no search => write it to swap ( FPACK_NOTHING )

	 ErrorCode=CacheManAllocateSector( Pack->SwapsTo->Device,
					   Pack->SwapsTo->Party,
					   0, Number,
					   LogicalSwap,     GetNumber,
					   Pack->PackageID, FPACK_NOTHING
					 );

	 if( ErrorCode || !LogicalSwap ) // unsuccessfull alloc on swap => direct
	 {
	  // swap full or corrupted => NOT CONSISTENT WRITE DIRECTLY TO FILE
	  #ifdef DEBUG
	   printf("\n Swap party is full...");
	  #endif

	  if( SaveSectorThrough( Device, Party, Logical, Number, Buffer ) )
	  {
	   #ifdef SEE_ERROR
	    printf("\n Error writing sec LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	   #endif

	   return ERR_FS_FATAL_ERROR;
	  }

	  #ifdef DEBUG
	   printf("\n NON CONSISTENT sector write to LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	  #endif

	  return ERR_FS_NO_ERROR;
	 }
	 else // space allocated
	 {
	  // create record in swap

	  #ifdef DEBUG
	   printf("\n Swap record will be created, Swapping on dev %u party %u...",
		  Pack->SwapsTo->Device,
		  Pack->SwapsTo->Party
		 );
	  #endif

	  // create new swap descriptor in current package
	  SwapDescr=new SwapDescrItem;

	  if( !SwapDescr ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	  SwapDescr->FirstLogicalSector=Logical;
	  SwapDescr->IntervalLng       =Number;
	  SwapDescr->LogicalInSwap     =LogicalSwap;


	  Pack->SwapQueue.Insert( SwapDescr );

	  // so write it
	  if( SaveSectorThrough( Pack->SwapsTo->Device,
				 Pack->SwapsTo->Party,
				 LogicalSwap, Number, Buffer
			       )
	    )
	  {
	   #ifdef SEE_ERROR
	    printf("\n Error writing to SWAP sec LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	   #endif

	   return ERR_FS_FATAL_ERROR;
	  }

	  #ifdef DEBUG
	   printf("\n SWAPPING: interval LogSec: %lu, length %u to SwapLogSec %lu...", Logical, Number, LogicalSwap );
	  #endif


	  return ERR_FS_NO_ERROR;
	 }
	}
	else // swap full (limit) => NOT CONSISTENT WRITE DIRECTLY TO FILE
	{
	 #ifdef DEBUG
	  printf("\n Swap full or swapping OFF...");
	 #endif

	 if( SaveSectorThrough( Device, Party, Logical, Number, Buffer ) )
	 {
	  #ifdef SEE_ERROR
	   printf("\n Error writing sec LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	  #endif

	  return ERR_FS_FATAL_ERROR;
	 }

	 #ifdef DEBUG
	  printf("\n NON CONSISTENT sector write to LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	 #endif



	 return ERR_FS_NO_ERROR;
	}
       }


       return ERR_FS_NO_ERROR;

  case FPACK_ADD:
       // - package already exists, find it and search it cache+swap for data
       // - create bitmap ( bitmap: bit means sector )
       //
       // (i) swap -  from buf save to swap if intervals compl. or part. over
       //	      each other, mark in bmp
       //             ( go throug whole buffer )
       // (ii) cache - each NOTDIRTY cache object P/C over save+Dirty+bmp
       //            - each DIRTY    cache object P/C over save+bmp
       // (iii) rest - if free space in cache - create, if free space on swp
       //              write it to swap, else UNCOSISTENT WRITE

       #ifdef DEBUG
	printf("\n\n Adding to package %u interval < %lu, %lu > SaveSec...", PackageID, Logical, Logical+(dword)Number-1lu );
       #endif

       // search for package
       Pack=CacheMan->PackQueue.FindPackageID( PackageID );

       if( !Pack )
       {
	#ifdef SEE_ERROR
	 printf("\n Error: package %u not exist => exiting...", PackageID );
	#endif

	return ERR_FS_PACKAGE_NOT_EXIST;
       }


       // --- CREATE BITMAP ---
       // Logical has off 0 in bitmap => X-Logical is off in bitmap
       BMP far *Bmp=new BMP( Number );

       if( !Bmp ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

       //--- TRY SWAP ---

       // now search on swap for interv. which would P/C over my
       // SwapFrom      ... where begin to search
       // SwapDescrFind ... what was find

       SwapFrom = SwapDescrFind = (SwapDescrItem far* )Pack->SwapQueue.Head->Next;

       while( SwapDescrFind ) // while something found
       {
	L=Logical;
	H=Logical+(dword)Number-1;	// , -1 BECAUSE <x,..

	SwapDescrFind
	=
	Pack->SwapQueue.FindIsCPOver( L, H, SwapFrom );

	if( !SwapDescrFind ) break; // nothing found

	// something found

	// set bits in bitmap
	Bmp->SetInterval( L-Logical, H-Logical );

	#ifdef DEBUG
	 printf("\n Saving %luB to Swap == < %lu, %lu >...", (H-L+1)*512l, L, H );
	#endif

	// write data from user buffer to swap ( space already allocated
	// only rewrite it )
	// use descriptor

	Number2=(word)H-(word)L+1u;

	LogicalSwap=SwapDescrFind->LogicalInSwap + L-Logical;

	  if( SaveSectorThrough( Pack->SwapsTo->Device,
				 Pack->SwapsTo->Party,
				 LogicalSwap,
				 Number2,
				 Buffer
			       )
	    )
	  {
	   #ifdef SEE_ERROR
	    printf("\n Error writing to SWAP sec LogPartySec %lu, device 0x%x, party %u... ",
		   LogicalSwap, Pack->SwapsTo->Device, Pack->SwapsTo->Party );
	   #endif

	   delete Bmp;

	   return ERR_FS_FATAL_ERROR;
	  }

	  #ifdef DEBUG
	   printf("\n SWAPPED int. LogSec: %lu, lng %u to SwapLogSec %lu ( orig %lu )...",
		  Logical, Number2, LogicalSwap, SwapDescrFind->LogicalInSwap );
	  #endif

	// try the rest
	SwapFrom = (SwapDescrItem far* )SwapDescrFind->Next;

       } // while



       //--- TRY CACHE ---

       // now search in cache for interv. which would P/C over my

       CacheFrom = CacheDescrFind = (CacheDescrItem far* )Pack->CacheQueue.Head->Next;

       while( CacheDescrFind ) // while something found
       {
	L=Logical;
	H=Logical+(dword)Number-1;	// , -1 BECAUSE <x,..

	CacheDescrFind
	=
	Pack->CacheQueue.FindIsCPOver( L, H, CacheFrom );

	if( !CacheDescrFind ) break; // nothing found

	// something found

	// set bits in bitmap
	Bmp->SetInterval( L-Logical, H-Logical );

	#ifdef DEBUG
	 printf("\n Saving %luB to cache == < %lu, %lu >...", (H-L+1)*512l, L, H );
	#endif

	// copy data from user buffer ( save them to cache )
	for( i=(H-L+1)*512l-1l; i>=0l; i-- )
	 CacheDescrFind->CacheObject
	  [ (L-CacheDescrFind->FirstLogicalSector)*512l+i ]
	 =
	 ((byte far*)Buffer)[ (L-Logical)*512l+i ];

	// mark it dirty
	 CacheDescrFind->Dirty=TRUE;

	// try the rest
	CacheFrom = (CacheDescrItem far* )CacheDescrFind->Next;

       } // while


       //--- SAVE REST DATA TO CACHE if full try SWAP                 ---
       //--- if swap full SAVE REST DATA UNCONSISTENTLY TO DATA PARTY ---

       #ifdef DEBUG
	printf("\n REST: saving - somewhere...");
       #endif

       From=0;	// search from beg of Bmp

       while( Bmp->FindHole( From, FirstBit, LNumber ) )
       {
	From=FirstBit+LNumber+1l; 	// next time search after hole
	L=(long)FirstBit;	       	// L is sec offset in buffer
	Number=(word)LNumber;


       // if there is free capacity, put it to cache ( copy from user buf to cache )
       if( CacheMan->MemRest >= Number*512 && CacheMan->CacheOn )
       {
	//--- SAVE REST DATA TO CACHE ---

	// yes there is some free space
	CacheMan->MemRest -= Number*512;
	#ifdef DEBUG
	 printf("\n CacheObj will be created, MemRest will be: %lu", CacheMan->MemRest);
	#endif

	// create new cache descriptor in current package
	CacheDescr=new CacheDescrItem;

	if( !CacheDescr ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	CacheDescr->FirstLogicalSector=Logical+L; // shift begin
	CacheDescr->IntervalLng=Number;
	CacheDescr->Dirty=TRUE;
	CacheDescr->CacheObject=(byte far *)farmalloc((dword)Number*512lu);

	if( !CacheDescr->CacheObject ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	// copy data from user buffer
	for( i=LNumber*512l-1l; i>=0l; i-- )
	 CacheDescr->CacheObject[i]
	 =
	 ((byte far*)Buffer)[ L*512l+i ];

	Pack->CacheQueue.Insert( CacheDescr );

	#ifdef DEBUG
	 printf("\n Interval <%lu, %lu> inserted to cache... ",
		Logical+L,
		Logical+L+(dword)(Number)-1ul);
	#endif

       }
       else // else not enough cache memory rests => try swap
       {

	//--- SAVE REST DATA TO SWAP ---

	#ifdef DEBUG
	 printf("\n Cache is full or caching OFF, trying swap...");
	#endif

	// last possibility for consistent writes => search place on swap

	if( CacheMan->SwapOn )
	{
	 // no search => write it to swap

	 ErrorCode=CacheManAllocateSector( Pack->SwapsTo->Device,
					   Pack->SwapsTo->Party,
					   0, Number,
					   LogicalSwap,
					   GetNumber,
					   Pack->PackageID, FPACK_NOTHING
					 );

	 if( ErrorCode || !LogicalSwap ) // unsuccessfull alloc on swap => direct
	 {
	  // swap full or corrupted => NOT CONSISTENT WRITE DIRECTLY TO FILE
	  #ifdef DEBUG
	   printf("\n Swap party is full...");
	  #endif


	  if( SaveSectorThrough( Device, Party, Logical+L, Number, Buffer ) )
	  {
	   #ifdef SEE_ERROR
	    printf("\n Error writing sec LogPartySec %lu, device 0x%x, party %u... ",
		   Logical, Device, Party );
	   #endif

	   delete Bmp;

	   return ERR_FS_FATAL_ERROR;
	  }


	  #ifdef SEE_ERROR
	   printf("\n NON CONSISTENT sector write to LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	   printf("\n Logical %lu, Logical+L %lu", Logical, Logical+L );
	  #endif

	  delete Bmp;

	  return ERR_FS_NO_ERROR;
	 }
	 else // space allocated
	 {
	  // create record in swap

	  #ifdef DEBUG
	   printf("\n Swap record will be created, Swapping on dev %u party %u...",
		  Pack->SwapsTo->Device,
		  Pack->SwapsTo->Party
		 );
	  #endif

	  // create new swap descriptor in current package
	  SwapDescr=new SwapDescrItem;

	  if( !SwapDescr ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	  SwapDescr->FirstLogicalSector=Logical+L; // shift beg ( it's source )
	  SwapDescr->IntervalLng=Number;
	  SwapDescr->LogicalInSwap=LogicalSwap;    // ( it's destination )

	  Pack->SwapQueue.Insert( SwapDescr );

	  // so write it
	  if( SaveSectorThrough( Pack->SwapsTo->Device,
				 Pack->SwapsTo->Party,
				 LogicalSwap,
				 Number,
				 Buffer
			       )
	    )
	  {
	   #ifdef SEE_ERROR
	    printf("\n Error writing to SWAP sec LogPartySec %lu, device 0x%x, party %u... ",
		   Logical, Pack->SwapsTo->Device, Pack->SwapsTo->Party );
	   #endif

	   delete Bmp;

	   return ERR_FS_FATAL_ERROR;
	  }

	  #ifdef DEBUG
	   printf("\n SWAPPING: interval LogSec: %lu, length %u to SwapLogSec %lu...", Logical, Number, LogicalSwap );
	  #endif

	  delete Bmp;

	  return ERR_FS_NO_ERROR;
	 }
	}
	else // swap full (limit) => NOT CONSISTENT WRITE DIRECTLY TO FILE
	{

	 //--- SAVE REST DATA UNCONSISTENTLY TO DATA PARTY ---

	 #ifdef DEBUG
	  printf("\n Swap full or swapping OFF...");
	 #endif

	 if( SaveSectorThrough( Device, Party, Logical+L, Number, Buffer ) )
	 {
	  #ifdef SEE_ERROR
	   printf("\n Error writing sec LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	  #endif

	  delete Bmp;

	  return ERR_FS_FATAL_ERROR;
	 }

	 #ifdef DEBUG
	  printf("\n NON CONSISTENT sector write to LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	  printf("\n  Logical %lu, Logical+L %lu", Logical, Logical+L );
	 #endif

	 delete Bmp;

	 return ERR_FS_NO_ERROR;
	}
       }


       } // while searching holes

       // --- NOW EVERYTHING IS SOMEWHERE SAVED ---

       delete Bmp;

       return ERR_FS_NO_ERROR;

  default:
       // it's FPACK_NOTHING and others...

       //--- SAVE DATA UNCONSISTENTLY TO DATA PARTY ---

       #ifdef DEBUG
	printf("\n Write through interval < %lu, %lu >...", Logical, Logical+(dword)Number-1lu );
       #endif

       if( SaveSectorThrough( Device, Party, Logical, Number, Buffer ) )
       {
	#ifdef SEE_ERROR
	 printf("\n Error writing sec LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	#endif

	return ERR_FS_FATAL_ERROR;
       }

       #ifdef DEBUG
	printf("\n NON CONSISTENT sector write to LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
       #endif

       return ERR_FS_NO_ERROR;
 }

}


//------------------------------------------------------------------------------------


word CacheManLoadToCache( byte  Device,     byte Party,
			  dword Logical,    word Number,
			  word  &PackageID, word Flag   )
// - loads block of sectors only to cache not to user buffer
// - loads wanted sectors to cache, if cache full or out of limit
//   return no error ( it is not in cache and call of this function
//   is done only because of forsage )
{
 PackageQueueItem far *Pack;
 CacheDescrItem   far *CacheDescr, far *CacheDescrFind, far *CacheFrom;
 SwapDescrItem    far *SwapDescrFind, far *SwapFrom;
 DataPartysItem   far *DataDev;

 dword            L, H;

 long		  From, FirstBit, LNumber;


 Device&=0x0F;


 switch( Flag )
 {
  case FPACK_CREAT:
       // if creating new package neither cache nor swap is searched
       // => everything directly load to cache

       #ifdef DEBUG
	printf("\n\n Creating new package for interval < %lu, %lu > LoadToCache...", Logical, Logical+(dword)Number-1lu );
       #endif

       // create new package
       Pack=new PackageQueueItem;

       if( !Pack ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

       Pack->PackageID=PackageID=CacheMan->PackIDs.GetID(); // for now
       #ifdef DEBUG
	printf(" ID=%d...",Pack->PackageID );
       #endif
       Pack->Device =Device;
       Pack->Party  =Party;
       Pack->Dirty  =FALSE;

       DataDev=DataPartys->FindDevParty( Device, Party );
       Pack->SwapsTo=DataDev->Swap;

       if( !Pack->SwapsTo )
       {
	#ifdef SEE_ERROR
	 printf("\n Error: no record in WhereSwaps for this party..." );
	#endif

	return ERR_FS_FATAL_ERROR;
       }

       // add package to priority queue
       CacheMan->PackQueue.Insert( Pack );

       //--- TRY PUT IT TO CACHE ---
       // if there is free capacity, put it to cache

       if( CacheMan->MemRest >= Number*512u && CacheMan->CacheOn )
       {
	// yes there is space
	CacheMan->MemRest -= Number*512u;
	#ifdef DEBUG
	 printf("\n CacheObj will be created, MemRest will be: %lu", CacheMan->MemRest );
	#endif

	// create new cache descriptor in current package
	CacheDescr=new CacheDescrItem;

	if( !CacheDescr ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	CacheDescr->FirstLogicalSector=Logical;
	CacheDescr->IntervalLng=Number;
	CacheDescr->Dirty=FALSE;
	CacheDescr->CacheObject=(byte far *)farmalloc((dword)Number*512lu);

	if( !CacheDescr->CacheObject ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	// load data to cache object - data are loaded directly
	if( LoadSectorThrough( Device, Party, Logical, Number, CacheDescr->CacheObject ) )
	{
	 #ifdef SEE_ERROR
	  printf("\n Error reading sec LogHDSec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	 #endif

	 return ERR_FS_FATAL_ERROR;
	}

	#ifdef DEBUG
	 printf("\n Sector read from LogHDSec %lu, device 0x%x, party %u... ", Logical, Device, Party );
	#endif

	Pack->CacheQueue.Insert( CacheDescr );

	#ifdef DEBUG
	 printf("\n Interval <%lu, %lu> inserted to cache... ", Logical, Logical+(dword)(Number)-1ul);
	#endif
       }
       #ifdef DEBUG
       else
	printf("\n Cache is full or caching OFF...");
       #endif
       // else not enough cache memory rests

       // swap has no sense -> caller want to put data to cache

       return ERR_FS_NO_ERROR;

   case FPACK_ADD:
       // - package already exists, find it and search it cache+swap for data
       // - create bitmap ( bitmap: bit means sector )
       //
       // (i) swap   - mark it in bmp ( go through whole buffer )
       //
       //
       // (ii) cache - each cache object P/C over mark in bmp
       // (iii) rest - load rest, if free space in cache - create cache obj
       //              ( swap has no sense - does not wanted it )

       #ifdef DEBUG
	printf("\n\n Adding to package %u interval < %lu, %lu > LoadToCache...", PackageID, Logical, Logical+(dword)Number-1lu );
       #endif

       // search for package
       Pack=CacheMan->PackQueue.FindPackageID( PackageID );

       if( !Pack )
       {
	#ifdef SEE_ERROR
	 printf("\n Error: package %u not exist => exiting...", PackageID );
	#endif

	return ERR_FS_PACKAGE_NOT_EXIST;
       }

       // --- CREATE BITMAP ---
       // Logical has off 0 in bitmap => X-Logical is off in bitmap
       BMP far *Bmp=new BMP( Number );


       //--- TRY SWAP ---

       // now search on swap for interv. which would P/C over my
       // SwapFrom      ... where begin to search
       // SwapDescrFind ... what was find

       SwapFrom = SwapDescrFind = (SwapDescrItem far* )Pack->SwapQueue.Head->Next;

       while( SwapDescrFind ) // while something found
       {
	L=Logical;
	H=Logical+(dword)Number-1;	// , -1 BECAUSE <x,..

	SwapDescrFind
	=
	Pack->SwapQueue.FindIsCPOver( L, H, SwapFrom );

	if( !SwapDescrFind ) break; // nothing found

	// something found

	// set bits in bitmap
	Bmp->SetInterval( L-Logical, H-Logical );

	#ifdef DEBUG
	 printf("\n Marked in bitmap %luB on Swap == < %lu, %lu >...", (H-L+1)*512l, L, H );
	#endif

	// try the rest
	SwapFrom = (SwapDescrItem far* )SwapDescrFind->Next;

       } // while in swap descriptors queue


       //--- TRY CACHE ---

       // now search in cache for interv. which would P/C over my

       CacheFrom = CacheDescrFind = (CacheDescrItem far* )Pack->CacheQueue.Head->Next;

       while( CacheDescrFind ) // while something found
       {
	L=Logical;
	H=Logical+(dword)Number-1;	// , -1 BECAUSE <x,..

	CacheDescrFind
	=
	Pack->CacheQueue.FindIsCPOver( L, H, CacheFrom );

	if( !CacheDescrFind ) break; // nothing found

	// something found

	// set bits in bitmap
	Bmp->SetInterval( L-Logical, H-Logical );

	#ifdef DEBUG
	 printf("\n Marked in bitmap %luB in cache == < %lu, %lu >...", (H-L+1)*512l, L, H );
	#endif

	// try the rest
	CacheFrom = (CacheDescrItem far* )CacheDescrFind->Next;

       } // while


       // --- DIRECT LOAD OF REST FROM DISK and try put it to cache ---

       #ifdef DEBUG
	printf("\n REST - load it...");
       #endif

       From=0;	// search from beg of Bmp

       while( Bmp->FindHole( From, FirstBit, LNumber ) )
       {
	From=FirstBit+LNumber+1l; 	// next time search after hole
	L=(long)FirstBit;	       	// L is sec offset in buffer
	Number=(word)LNumber;

       // if there is free capacity, put it to cache ( copy from user buf to cache )
       if( CacheMan->MemRest >= Number*512 && CacheMan->CacheOn )
       {
	//--- SAVE REST DATA TO CACHE ---

	// yes there is some free space
	CacheMan->MemRest -= Number*512;
	#ifdef DEBUG
	 printf("\n CacheObj will be created, MemRest will be: %lu", CacheMan->MemRest);
	#endif

	// create new cache descriptor in current package
	CacheDescr=new CacheDescrItem;

	if( !CacheDescr ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	CacheDescr->FirstLogicalSector=Logical+L; // shift begin
	CacheDescr->IntervalLng=Number;
	CacheDescr->Dirty=FALSE;
	CacheDescr->CacheObject=(byte far *)farmalloc((dword)Number*512lu);

	if( !CacheDescr->CacheObject ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	// load data to cache object
	if( LoadSectorThrough( Device, Party, Logical+L, Number, CacheDescr->CacheObject ) )
	{
	  #ifdef SEE_ERROR
	   printf("\n Error reading sec LogHDSec %lu, device 0x%x, party %u... ", Logical+L, Device, Party );
	  #endif

	 return ERR_FS_FATAL_ERROR;
	}

	#ifdef DEBUG
	 printf("\n Sector read from LogHDSec %lu, device 0x%x, party %u... ", Logical+L, Device, Party );
	 printf("\n  Logical %lu, Logical+L %lu", Logical, Logical+L );
	#endif


	Pack->CacheQueue.Insert( CacheDescr );

	#ifdef DEBUG
	 printf("\n Interval <%lu, %lu> inserted to cache, L %lu... ",
		Logical+L,
		Logical+L+(dword)(Number)-1ul, L );
	#endif

       }
       #ifdef DEBUG
	else
	  printf("\n Cache full or caching OFF...");
       #endif

       } // while searching holes

       // --- NOW EVERYTHING IS LOADED ---

       delete Bmp;

       return ERR_FS_NO_ERROR;

  default:
       // FPACK_NOTHING => is HAS NO SENSE

       return ERR_FS_NO_ERROR;
 }

}


//------------------------------------------------------------------------------------
#undef DEBUG

word LoadSectorThrough( byte Device,   byte Party,
			dword Logical, word Number, void far *Buffer )
// - load logical sector(s) from party, logical nr counted from bg of party
// - writes only upto 128 sectors==65535 bytes!
// - DODELAT aby vzdy nenatahoval BR
{
 MasterBootRecord BR;

 Device&=0x0F;

 if( LoadBoot( Device, Party, &BR ) )
 {
  #ifdef SEE_ERROR
   printf("\n Error reading BR in LoadSectorThrough..." );
  #endif

  return ERR_FS_FATAL_ERROR;
 }

 // points to the first bitmap sec in party
 dword Sector
       =
       BR.BPB.HiddenSectors
 //    +BR.BPB.ReservedSectors - DON'T ADD I COUNT SECTORS FROM BEG OF PARTY!
       +Logical;

 if( ReadLogicalSector( Device, Sector, Buffer, Number ) )
 {
  #ifdef SEE_ERROR
   printf("\n Error reading sec LogHDSec %lu, device 0x%x, party %u... ", Sector, Device, Party );
  #endif

  return ERR_FS_FATAL_ERROR;
 }

 #ifdef DEBUG
  printf("\n Sector READ THROUGH from LogHDSec %lu, device 0x%x, party %u... ", Sector, Device, Party );
 #endif

 return ERR_FS_NO_ERROR;
}

//------------------------------------------------------------------------------------

word SaveSectorThrough( byte Device,   byte Party,
			dword Logical, word Number, void far *Buffer )
// - writes logical sector(s) to party, logical nr counted from bg of party
// - writes only upto 128 sectors==65535 bytes!
// - DODELAT aby vzdy nenatahoval BR
{
 MasterBootRecord BR;

 Device&=0x0F;

 if( LoadBoot( Device, Party, &BR ) )
 {
  #ifdef SEE_ERROR
   printf("\n Error reading BR in SaveSectorThrough..." );
  #endif

  return ERR_FS_FATAL_ERROR;
 }

 // points to the first bitmap sec in party
 dword Sector
       =
       BR.BPB.HiddenSectors
 //    +BR.BPB.ReservedSectors - DON'T ADD I COUNT SECTORS FROM BEG OF PARTY!
       +Logical;

 if( WriteLogicalSector( Device, Sector, Buffer, Number ) )
 {
  #ifdef SEE_ERROR
   printf("\n Error writing sec LogHDSec %lu, device 0x%x, party %u... ", Sector, Device, Party );
  #endif

  return ERR_FS_FATAL_ERROR;
 }

 #ifdef DEBUG
  printf("\n Sector WRITTEN THROUGH to LogHDSec %lu, device 0x%x, party %u... ", Sector, Device, Party );
 #endif

 return ERR_FS_NO_ERROR;
}

//- EOF -----------------------------------------------------------------------------------
