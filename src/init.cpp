#include "init.h"

//#define DEBUG
#define DVORKAS_LOGS

extern StripSession StripingSession;

extern int  HDHandle[NR_OF_DEVICES],
	    ActiveHDHandle;

extern SwapPartysQueue   far *SwapPartys; // list of swap devices in system
extern DataPartysQueue   far *DataPartys; // list of data devices in system


//- CreateDeviceTable ------------------------------------------------------------------------

word CreateDeviceTable( void )
// creates table of devices in system => its file with NR_OF_DEVICES entries
// => writes it to disk
{
 int 		       handle;
 byte		       i;
 struct DeviceTabEntry Device[NR_OF_DEVICES];

 // 0x80
 Device[0].Presented = TRUE;
 Device[0].Cylinders = 1024;
 Device[0].Sectors   = 17;
 Device[0].Heads     = 2;
 // 0x81
 Device[1].Presented = TRUE;
 Device[1].Cylinders = 1024;
 Device[1].Sectors   = 17;
 Device[1].Heads     = 2;
 // 0x82
 Device[2].Presented = TRUE;
 Device[2].Cylinders = 1024;
 Device[2].Sectors   = 17;
 Device[2].Heads     = 2;
 // 0x83
 Device[3].Presented = FALSE;
 Device[3].Cylinders = 1024;
 Device[3].Sectors   = 17;
 Device[3].Heads     = 2;
 // 0x84
 Device[4].Presented = FALSE;
 Device[4].Cylinders = 1024;
 Device[4].Sectors   = 17;
 Device[4].Heads     = 2;

 #ifdef DEBUG
  printf("\n\n Creating device table...");
 #endif


 if ((handle = open("devices.lst", O_CREAT  | O_RDWR | O_BINARY,
				   S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef DEBUG
   printf("\n Error opening DEVICES.LST - device table...");
  #endif
  return ERR_FS_FATAL_ERROR;
 }


 for( i=0; i<NR_OF_DEVICES; i++ )
 {
  if( write( handle, Device+i, DEVICE_ENTRY_LNG ) != DEVICE_ENTRY_LNG )
  {
   #ifdef DEBUG
    printf("\n Write error: file DEVICES.LST, item nr. %d", i);
   #endif
   return ERR_FS_FATAL_ERROR;
  }
  else
  {
  #ifdef DEBUG
   printf("\n Item %d: pres. %u, cyl %u, sec %u, heads %u ...", i, Device[i].Presented,Device[i].Cylinders,Device[i].Sectors,Device[i].Heads);
  #endif
  }
 }

 #ifdef DEBUG
  printf("\n Device table written to DEVICES.LST...");
 #endif

 close(handle);

 return ERR_FS_NO_ERROR;
}

//- DeviceExist ------------------------------------------------------------------------

bool DeviceExist( byte Device )
{
 if( HDHandle[Device&0x0F]!=(-1) )
  return TRUE;
 else
  return FALSE;
}

//- LoadDeviceTable ------------------------------------------------------------------------

word LoadDeviceTable( struct DeviceTabEntry *Device )
{
 int  handle;
 byte i;

 #ifdef DEBUG
  printf("\n\n Opening device table file DEVICES.LST...");
 #endif

 if ((handle = open("devices.lst", O_RDONLY | O_BINARY,
				   S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef DEBUG
   printf("\n Error - device table not present...");
  #endif
  return ERR_FS_FATAL_ERROR;
 }


 for( i=0; i<NR_OF_DEVICES; i++ )
 {
  if( read( handle, Device+i, DEVICE_ENTRY_LNG ) != DEVICE_ENTRY_LNG )
  {
   #ifdef DEBUG
    printf("\n Read error: file DEVICES.LST, item nr. %d", i);
   #endif
   return ERR_FS_FATAL_ERROR;
  }
  else
  {
  #ifdef DEBUG
   printf("\n Item %d: pres. %u, cyl %u, sec %u, heads %u ...", i, Device[i].Presented,Device[i].Cylinders,Device[i].Sectors,Device[i].Heads);
  #endif
  }
 }

 #ifdef DEBUG
  printf("\n Device table successfuly read...");
 #endif

 close(handle);

 return ERR_FS_NO_ERROR;
}


//- CreateONEDeviceUsingDeviceTable ------------------------------------------------------------------------

word CreateONEDeviceUsingDeviceTable( byte Index, struct DeviceTabEntry *Table )
{
 int   handle;
 dword VolumeSize;

 Index%=NR_OF_DEVICES;			// for security

 // now create volume...

  if( Table[Index].Presented )
  {
   #ifdef DEBUG
    printf("\n Device %d...",Index);
   #endif

   switch( Index )
   {
    case 0:
	   if ((handle = open("0x80.hd", O_CREAT  | O_RDWR | O_BINARY,
				   S_IWRITE | S_IREAD 		)) == -1)
	   {
	    #ifdef DEBUG
	     printf(" ERROR, unable to create it!");
	    #endif
	    return ERR_FS_FATAL_ERROR;
	   }
	   break;
    case 1:
	   if ((handle = open("0x81.hd", O_CREAT  | O_RDWR | O_BINARY,
				   S_IWRITE | S_IREAD 		)) == -1)
	   {
	    #ifdef DEBUG
	     printf(" ERROR, unable to create it!");
	    #endif
	    return ERR_FS_FATAL_ERROR;
	   }
	   break;
    case 2:
	   if ((handle = open("0x82.hd", O_CREAT  | O_RDWR | O_BINARY,
				   S_IWRITE | S_IREAD 		)) == -1)
	   {
	    #ifdef DEBUG
	     printf(" ERROR, unable to create it!");
	    #endif
	    return ERR_FS_FATAL_ERROR;
	   }
	   break;
    case 3:
	   if ((handle = open("0x83.hd", O_CREAT  | O_RDWR | O_BINARY,
				   S_IWRITE | S_IREAD 		)) == -1)
	   {
	    #ifdef DEBUG
	     printf(" ERROR, unable to create it!");
	    #endif
	    return ERR_FS_FATAL_ERROR;
	   }
	   break;
    case 4:
	   if ((handle = open("0x84.hd", O_CREAT  | O_RDWR | O_BINARY,
				   S_IWRITE | S_IREAD 		)) == -1)
	   {
	    #ifdef DEBUG
	     printf(" ERROR, unable to create it!");
	    #endif
	    return ERR_FS_FATAL_ERROR;
	   }
	   break;

   } // switch

  VolumeSize=512 * (dword)Table[Index].Cylinders*
		   (dword)Table[Index].Sectors*
		   (dword)Table[Index].Heads
		   - 1; // one byte will be written

  lseek( handle, VolumeSize, SEEK_SET );

  if( write( handle, &Index, 1 ) != 1 ) // write one byte
  {
   #ifdef DEBUG
    printf("\n Error creating device %d...", Index);
   #endif
   return ERR_FS_FATAL_ERROR;
  }

  #ifdef DEBUG
   printf(" cyl %u, sec %u, heads %u => %luB...",Table[Index].Cylinders,Table[Index].Sectors,Table[Index].Heads,VolumeSize+1);
  #endif

  close( handle );

  } // if
  else
  {
   #ifdef DEBUG
    printf("\n Device %d not in system...", Index );
   #endif

  return ERR_FS_NO_ERROR;
  }


 #ifdef DEBUG
  printf(" Finished, OK...");
 #endif

 return ERR_FS_NO_ERROR;
}

//- CreateDevicesUsingDeviceTable ------------------------------------------------------------------------

word CreateDevicesUsingDeviceTable( void )
{
 byte		       i;
 struct DeviceTabEntry Device[NR_OF_DEVICES];


 // load DeviceTable
 if( LoadDeviceTable( &Device[0] ) )
  return ERR_FS_FATAL_ERROR;


 #ifdef DEBUG
  printf("\n\n Creating devices:");
 #endif

 // now create volumes...
 for( i=0; i<NR_OF_DEVICES; i++ )
  CreateONEDeviceUsingDeviceTable( i, &Device[0] );


 #ifdef DEBUG
  printf("\n Creating of devices finished...");
 #endif
 return ERR_FS_NO_ERROR;
}

//- InitializeSimulation ------------------------------------------------------------------------

word InitializeSimulation( bool WriteTable, bool CreateDevices )
// dodelat: doresit vybulblani chyby
{
 byte i;

 #ifdef DEBUG
  printf("\n Initialize simulation of hardware for file system...");
 #endif

 if( WriteTable )
  CreateDeviceTable();

 if( CreateDevices )
 {
  if( CreateDevicesUsingDeviceTable() )
  {
   printf("\n Device table corrunpted or not exist => terminating program... \n Bye!");

   exit(0);
  }
 }

 #ifdef DEBUG
  printf("\n\n Init of HD handles...");
 #endif

 // opening files == initilizing handles ( in case FALSE, FALSE too )
 // if file not created => handle is set to 0xFFFF
  HDHandle[0]=open("0x80.hd", O_RDWR | O_BINARY, S_IWRITE | S_IREAD);
  HDHandle[1]=open("0x81.hd", O_RDWR | O_BINARY, S_IWRITE | S_IREAD);
  HDHandle[2]=open("0x82.hd", O_RDWR | O_BINARY, S_IWRITE | S_IREAD);
  HDHandle[3]=open("0x83.hd", O_RDWR | O_BINARY, S_IWRITE | S_IREAD);
  HDHandle[4]=open("0x84.hd", O_RDWR | O_BINARY, S_IWRITE | S_IREAD);

 // init of ActuallHDHandle
 for( i=0; i<NR_OF_DEVICES; i++ )
  if( HDHandle[i]!=(-1) )
  {
   ActiveHDHandle=HDHandle[i];
   break;
  }


 #ifdef DEBUG
  printf("\n => HD0x80=0x%x, HD0x81=0x%x, HD0x82=0x%x, HD0x83=0x%x, HD0x84=0x%x...",
	 HDHandle[0],HDHandle[1],HDHandle[2],HDHandle[3],HDHandle[4]
	);
  printf("\n ActiveHDHandle was set to: 0x%x...", ActiveHDHandle );
  printf("\n End of initilization of simulation...");
 #endif


 if( ActiveHDHandle==(-1) )
 {
  printf("\n No device in system => terminating program... \n Bye!");

  exit(0);
 }

 return ERR_FS_NO_ERROR;
}

//- FSOpenFileSystem ------------------------------------------------------------------------

 word FSOpenFileSystem( bool SwappingOn, bool CachingOn, dword CacheMem )
 // opens, checks dirty, tra, repairing, inits striping, ...
 // - searches for devices in system, on live HD with min ID searches
 //   for active partition ( boot )
 // - here searches for "/file.system/stripe table.system" file and
 //   using this table searches for tables on other devices. If are the
 //   same => switches FS to striping mode. If only four devices active
 //   => panic mode => say user to add NR_OF_DEVICESth HD to system. If four devices
 //   are OK and  fifth not => switches to stripe repairing mode
 //   => copies data and counts parity for NR_OF_DEVICESth device

 // - checks the rest of devices:
 //     	- is there our fs,
 //             - unmounted cleanly => ( possible repairing using log )
 // - set DIRTY flag to all partitions with our file system

{
 word             ErrCode,
		  i,
		  j,
		  k;
 MasterBootRecord MBR,
		  BR;
 byte             OurFs=FALSE;
 byte             MagicData[]={RFS_MAGIC_WORD_DATA_PARTY},
		  MagicSwap[]={RFS_MAGIC_WORD_SWAP_PARTY};
 dword		  Free;
 SwapPartysItem   far *SwapParty, far *HelpSwapParty;
 DataPartysItem   far *DataParty, far *HelpDataParty;
 SwapPartysQueue  far *DirtySwapPartys;
 DataPartysQueue  far *DirtyDataPartys;

 void             far *Sector=farmalloc(512lu);
 if( !Sector ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }


 #ifdef DEBUG
  clrscr();

  printf("\n\n OPENing RELIABLE FILE SYSTEM..." );
 #endif


 // go through handles and test if exist device, if not terminate
 ActiveHDHandle=0xFFFF;

 #ifdef DEBUG
  printf("\n  Searching for RFS partys in system..." );
 #endif

 for( i=0; i<NR_OF_DEVICES; i++ )
  if( DeviceExist(i) )
  {
   ActiveHDHandle=HDHandle[i];
   break;
  }

 if( ActiveHDHandle==(-1) )
 {
  printf("\n No device in system => terminating program... \n Bye!");

  farfree( Sector );
  exit(0);
 }

 // Read MBR of each device in system. Using MBR read each BR of party
 // on device and test if it is party of our file system.
 // Create list of swap partys in system, and list of partys in
 // stripe session

 // striping must be OFF FOR NOW!
 StripingSession.StripingIs=NONACTIVE;



 // create SwapPartys and DataPartys list
 SwapPartys=new SwapPartysQueue;
 DataPartys=new DataPartysQueue;

 DirtySwapPartys=new SwapPartysQueue;
 DirtyDataPartys=new DataPartysQueue;

 // try if device exist
 for( i=0; i<NR_OF_DEVICES; i++ )
 {
  if( DeviceExist(i) )
  {
   // Load MBR
   #ifdef DEBUG
    printf("\n  Reading MBR of device 0x%x ...", 0x80+i );
   #endif

   if( LoadMasterBoot( 0x80+i, &MBR ) )
   {
    #ifdef DEBUG
     printf("\n Error reading MBR Dev %d...", i );
    #endif
/*
    farfree( Sector );

    delete DirtySwapPartys;
    delete DirtyDataPartys;

    return ERR_FS_FATAL_ERROR;
*/
    continue;
   }

   // now read active boot records on existing device ( try partys )
   for( j=1; j<=4; j++ )
   {
    ErrCode=LoadBoot( 0x80+i, j, &BR );
    switch( ErrCode )
    {
     case ERR_FS_NO_ERROR:
	 // party created => test if it'is our FS
	 // identify if it is swap or data partition

	 OurFs=FALSE;

	 // test if it is our DATA PARTY
	 for( k=0; k<7; k++ )
	 {
	  if( BR.OSID.ID[k]!=MagicData[k] ) break;
	 }
	 if( k>=6 ) // it's data party of our file system
	 {
	  OurFs=TRUE;
	  printf("\n  Party ID %d is RFSData,", j );

	  GetPartyFreeSpace( i, j, Free );

	  printf(" free %luB,", Free*512lu );

	  // put party to one of queues
	  if( BR.BPB.DirtyFlag==PARTY_IS_CLEAN )
	  {
	   #ifdef DEBUG
	   printf(" state: CLEAN => OK...", j );
	   #endif

	   // allocate new record in CLEAN devices queue
	   DataParty=(DataPartysItem far *)farmalloc(sizeof(DataPartysItem));
	   if( !DataParty ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	   DataParty->Device=i;
	   DataParty->Party=j;
	   // DODELAT: DataDev->Swap ... initialized later using config file or the second boot

	   DataPartys->Insert( DataParty );
	  }
	  else
	  {
	   #ifdef DEBUG
	    printf(" state: DIRTY => RECOVERY forced...", j );
	   #endif

	   // allocate new record in DIRTY devices queue
	   DataParty=(DataPartysItem far *)farmalloc(sizeof(DataPartysItem));
	   if( !DataParty ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	   DataParty->Device=i;
	   DataParty->Party=j;
	   // DODELAT: DataDev->Swap ... initialized later using config file or the second boot

	   DirtyDataPartys->Insert( DataParty );
	  }
	 }


	 // test if it is our SWAP PARTY
	 for( k=0; k<7; k++ )
	 {
	  if( BR.OSID.ID[k]!=MagicSwap[k] ) break;
	 }
	 if( k>=6 ) // it's swap party of our file system
	 {
	  OurFs=TRUE;
	  printf("\n  Party ID %d is RFSSwap,", j );

	  GetPartyFreeSpace( i, j, Free );

	  printf(" free %luB,", Free*512lu );

	  // put party to one of queues
	  if( BR.BPB.DirtyFlag==PARTY_IS_CLEAN )
	  {
	   #ifdef DEBUG
	   printf(" state: CLEAN => OK...", j );
	   #endif

	   // allocate new record
	   SwapParty=(SwapPartysItem far *)farmalloc(sizeof(SwapPartysItem));
	   if( !SwapParty ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	   SwapParty->Device=i;
	   SwapParty->Party=j;

	   SwapPartys->Insert( SwapParty );
	  }
	  else
	  {
	   #ifdef DEBUG
	    printf(" state: DIRTY => RECOVERY forced...", j );
	   #endif


	   // allocate new record
	   SwapParty=(SwapPartysItem far *)farmalloc(sizeof(SwapPartysItem));
	   if( !SwapParty ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	   SwapParty->Device=i;
	   SwapParty->Party=j;

	   // SwapParty->FreeSpace is set after cleaning

	   DirtySwapPartys->Insert( SwapParty );
	  }
	 }


	 if( !OurFs )
	 {
	  #ifdef DEBUG
	   printf("\n  Party ID %d isn't neither RFSData nor RFSSwap...", j );
	  #endif

	  // try next partition

	  continue;
	 }

	 /*
	 // mark partition as dirty - not important if it is data or swap
	 // when fs opened => each party in system must be marked as DIRTY
	 BR.BPB.DirtyFlag=PARTY_IS_DIRTY;

	 // save changes
	 SaveBoot( 0x80+i, j, &BR );
	 */

	 break;
     case ERR_FS_NOT_USED:
	  #ifdef DEBUG
	   printf("\n  Party ID %d not exist...", j );
	  #endif
	 break;

     default:
	 // dodelat: je to nutne takhle exitnout?

	 #ifdef DEBUG
	  printf("\n Error reading BR Dev %d Party %d...", i, j );
	 #endif

/*
	 farfree( Sector );

	 delete DirtySwapPartys;
	 delete DirtyDataPartys;

	 return ERR_FS_FATAL_ERROR;
*/
	 continue;

    } // switch
   }  // for

  }
 }


 //---------------------------------------------------------------------------------
 //- STRIPING - STRIPING - STRIPING - STRIPING - STRIPING - STRIPING - STRIPING
 //---------------------------------------------------------------------------------

 InitializeStriping( DirtyDataPartys );

 //---------------------------------------------------------------------------------
 //- STRIPING - STRIPING - STRIPING - STRIPING - STRIPING - STRIPING - STRIPING
 //---------------------------------------------------------------------------------



 // mark partitions as dirty ( change flags in boot records )
 // when fs opened => each party in system must be marked as DIRTY
 DataPartys->SetPartysDirty();
 SwapPartys->SetPartysDirty();


 if( !ReadConfigFileStart("RFS.CFG") )
 {
  #ifdef SEE_ERROR
   printf("\n Error: RFS config file not found...");
  #endif
 }


 // --> recovery DATA partys, then recovery SWAP partys

 // --- RECOVERY OF DATA ---

 DataParty=(DataPartysItem far *)(DirtyDataPartys->Head->Next);

 while( DataParty!=DirtyDataPartys->Tail )
 {
  #ifdef DEBUG
   printf("\n\n  -> RECOVERY of Dev %u DATA Party %u...", DataParty->Device, DataParty->Party );
  #endif


  // call recovery utility
  RecoverParty( DataParty->Device, DataParty->Party );


  // mark partition as dirty because is FS opened => partition is dirty => OK


  #ifdef DEBUG
   printf("\n  -> Dev %u Party %u RECOVERed ...", DataParty->Device, DataParty->Party );
  #endif

  // move party to clean partys
  DataParty->Next->Last=DataParty->Last;	// change pointers
  DataParty->Last->Next=DataParty->Next;
  HelpDataParty=(DataPartysItem far *)DataParty->Next; // take next here
						// because value will be
						// destroyed by insert
  DataPartys->Insert( DataParty );		// insert it between clean

  // take next
  DataParty=HelpDataParty;
 } 	// while

 // swap partys recovered now



 // --- RECOVERY OF SWAP ---

 SwapParty=(SwapPartysItem far *)(DirtySwapPartys->Head->Next);

 while( SwapParty!=DirtySwapPartys->Tail )
 {
  #ifdef DEBUG
   printf("\n\n  -> RECOVERY of Dev %u SWAP Party %u...", SwapParty->Device, SwapParty->Party );
  #endif

  // data were moved from swap partitions in log recovery section
  // => format partition to be all swap sectors unallocated
  FSFormat( SwapParty->Device, SwapParty->Party, RFS_SWAP );


  #ifdef DEBUG
   printf("\n  -> Dev %u Party %u RECOVERed ...", SwapParty->Device, SwapParty->Party );
  #endif

  // mark partition as dirty because is FS opened => partition is dirty => OK

  // move party to clean partys
  SwapParty->Next->Last=SwapParty->Last;	// change pointers
  SwapParty->Last->Next=SwapParty->Next;
  HelpSwapParty=(SwapPartysItem far *)SwapParty->Next; // take next here
						// because value will be
						// destroyed by insert
  SwapPartys->Insert( SwapParty );		// insert it between clean

  // take next
  SwapParty=HelpSwapParty;
 } 	// while

 // swap partys recovered now



 // --- FILL WHERE EACH DATA PARTY SWAPS

 // read config file => there is written where swaps
 // if config file not available:
 // take the first from SwapQueue

 printf("\n\n Choose where each DATA party will SWAP:");

 if( SwapPartys->Head->Next==SwapPartys->Tail )
 {
  SwappingOn=FALSE; // no swap party available => off swapping
  printf( "\n No swap device in system => Swapping switched to OFF MODE...");
 }
 else
 {
  SwapParty=(SwapPartysItem far *)(SwapPartys->Head->Next);

  DataParty=(DataPartysItem far *)(DataPartys->Head->Next);

  while( DataParty!=DataPartys->Tail )
  {

   DataParty->Swap = SwapParty;

   printf("\n  -> Dev %u Party %u swaps to Dev %u Party %u ...",
	  DataParty->Device,
	  DataParty->Party,
	  DataParty->Swap->Device,
	  DataParty->Swap->Party
	 );

   // take next
   DataParty=(DataPartysItem far *)(DataParty->Next);

  } // while
 } // if




 // --- STRIPING SESSION ---

 // zapnuti stripingu porovnanim tabulek
 // na kazdem disku bude tabulka ( na vsech discich bude stejna )
 // ve ktere bude seznam partysen, ktere se stripuji a take
 // oznacena paritni partysna
 // ( proto mi zatim bude stacit jeden file ), pozdeji nacist
 // ze vsech a porovnat

 // initialize CacheMan - USE CONFIG FILE TO FILL IT
 // => IF CONFIG FILE NOT EXIST => USE FUNCTION PARAMS
 CacheManInit( CacheMem, CachingOn, SwappingOn );

  // initialize upper layer
 UpperLayerInit( DirtyDataPartys );

 ReadConfigFileFinish();

 farfree( Sector );

 delete DirtySwapPartys;
 delete DirtyDataPartys;

 #ifdef DEBUG
  printf("\n RELIABLE FILE SYSTEM OPENed...\n");
 #endif

 return ERR_FS_NO_ERROR;
}

//- FSOpenFileSystem ------------------------------------------------------------------------

word FSShutdownFileSystem( void )
// - flushes, commits, unmount filesystem, clears DIRTY flag
// - goes through each party on each disk and if there is our fs clear DIRTY flag
{
 #ifdef DEBUG
  printf("\n\n Starting SHUTDOWN of RELIABLE FILE SYSTEM...");
 #endif

 // shutdown upper layer
 UpperLayerShutdown();

 // make complete commit of cache
 CacheManCompleteCommit( FPACK_DELETE );

 // make devices clean ( change flags in boot records )
 DataPartys->SetPartysClean();
 SwapPartys->SetPartysClean();

 // deinitialize cache
 CacheManDeinit();

 // close striping
 ShutdownStriping();

 // system lists
 delete DataPartys;
 delete SwapPartys;


 #ifdef DEBUG
  printf("\n RELIABLE FILE SYSTEM SHUTDOWNed...");
 #endif

 return ERR_FS_NO_ERROR;
}

//- EOF ------------------------------------------------------------------------
