 #include "striping.h"

//#define DEBUG
//#define SEE_ERROR



#define FAST_PARITY



StripSession 		StripingSession;
extern int              HDHandle[5];
extern SwapPartysQueue  far *SwapPartys; // list of swap devices in system
extern DataPartysQueue  far *DataPartys; // list of data devices in system



//--- BuffersTheSame -------------------------------------------------------



bool BuffersAreTheSame( byte far *A, byte far *B, dword Counter )
// Counter jumps by bytes
{
 dword MyCounter;

 for( MyCounter=0; MyCounter<Counter; MyCounter++ )
 {
  if( A[MyCounter]!=B[MyCounter] ) return FALSE;
 }

 return TRUE;
}


//--- StripingConfigure -------------------------------------------------------



word InitStripingLoadAndCheckTables( DataPartysQueue far *DirtyDataPartys,
				     byte &Dead,
				     byte &DeadType
				     )
// - try to load tables
{
 char  	 	TableFileName[8+1+3 +1];
 int		Handle;
 DataPartysItem far *At;
 long		FirstTableLng=0;
 StripItem      far *FirstTable=NULL;
 word		FirstTableItems;
 long		TableLng;
 StripItem      far *Table=NULL;
 bool		TablesTheSame=TRUE;
 word		i;
 StripItem 	far *StripingItem;


 MasterBootRecord BR;

 // number of dead partys is 0 now...
 Dead=0;


 // --- CLEAN AND DIRTY DATA DEVICES ---

 At=(DataPartysItem far *)DataPartys->Head->Next;

 while( TRUE )
 {
  // Klinger's solution -> jump from clean partys to dirty
  // when on tail of clean jump to dirty head

  if( At == (DataPartysItem far *)DataPartys->Tail )
  {
   // jump to dirty devices
   printf("\n  Jumping to dirty data devices...");
   At=(DataPartysItem far *)DirtyDataPartys->Head->Next;

  }

  if( At == (DataPartysItem far *)DirtyDataPartys->Tail )
   break;	// clean already done, dirty done right now

  // construct name

  TableFileName[0]= At->Device+'0'; // first cipher is device
  TableFileName[1]= At->Party+'0';  // second is party
  TableFileName[2]= 0;
  strcat( TableFileName, "STRIP.TAB" );


  if ((Handle = open( TableFileName, O_RDONLY | O_BINARY,
				     S_IWRITE | S_IREAD	)) == -1)
  {
   #ifdef DEBUG
    printf("\n Striping table file %s not exist...", TableFileName );
   #endif

   // take next
   At=(DataPartysItem far *)At->Next;

   continue;
  }

  // table exist -> check if striping is ON
  TableLng=filelength( Handle );

  if( Table ) { farfree( Table ); Table=NULL; }

  Table=(StripItem far *)farmalloc((dword)TableLng);
   if( !Table ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }



  read( Handle, Table, (word)TableLng );



  if( Table[0].Number==0 )
  {
   // striping on this device is OFF
   #ifdef DEBUG
    printf("\n Striping on Dev %d Party %d is OFF...", At->Device, At->Party );
   #endif

   // take next
   At=(DataPartysItem far *)At->Next;

   continue;
  }

  #ifdef DEBUG
   printf("\n Striping on Dev %d Party %d is ON...", At->Device, At->Party );
  #endif

  // porovnani s prvni nahranou
  if( FirstTable==NULL )
  {
   // it's the first table I found => copy it
   #ifdef DEBUG
    printf("\n  Table on Dev %d Party %d is THE FIRST TABLE I found...", At->Device, At->Party );
   #endif

   FirstTableLng=TableLng;

   FirstTable=(StripItem far *)farmalloc((dword)FirstTableLng);
    if( !FirstTable ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

   for( i=0; i<(word)FirstTableLng; i++ )
    ((byte far *)FirstTable)[i]=((byte far *)Table)[i];
  }
  else
  {
   // I have some already => check if tables are the same

   if( FirstTableLng != TableLng )
   {
    // different length
    #ifdef DEBUG
     printf("\n  Table on Dev %d Party %d is DIFFERENT from FirstTable...", At->Device, At->Party );
    #endif

    TablesTheSame=FALSE;

    // take next
    At=(DataPartysItem far *)At->Next;

    continue;
   }

   // check tables content
   if( !BuffersAreTheSame( (byte far *)FirstTable, (byte far *)Table,
			   (dword)TableLng
			 )
     )
   {
    #ifdef DEBUG
     printf("\n  Table on Dev %d Party %d is DIFFERENT from FirstTable...", At->Device, At->Party );
    #endif

    // take next
    At=(DataPartysItem far *)At->Next;

    continue;
   }

   #ifdef DEBUG
    printf("\n  Table on Dev %d Party %d is THE SAME as FirstTable...", At->Device, At->Party );
   #endif
  }


  // take next
  At=(DataPartysItem far *)At->Next;
 }

 if( Table ) { farfree( Table ); Table=NULL; }

 close( Handle );



 if( TablesTheSame==FALSE )
 {
  // ( OR ask user where is the valid table)
  // => configuration utility will be run later and user
  // changes the system...
 }
 // else same == TRUE


 // --- NOW CHECK THE TABLE VALIDITY 			---
 // --- AND TRANSFORM Table TO StripingSessionData      ---


 // jestlize jsem nenasel zadnou tabulku neni co transformovat => kontrola

 FirstTableItems=(word)FirstTableLng;
 FirstTableItems/=sizeof(StripItem);

 for( i=0; i<FirstTableItems; i++ )
 {
  if( LoadBoot( FirstTable[i].Device, FirstTable[i].Party, &BR ) )
  {
   // device not usable => can't be add
   printf("\n This Device/Party is not in system => was NOT add...");

   // count number of dead partys
   Dead++;
   DeadType=FirstTable[i].Type;

   continue;
  }

  // OK device exists
  switch( FirstTable[i].Type )
  {
   case DATA_STRIP:
		   StripingItem=(StripItem far *)farmalloc(sizeof(StripItem));
		    if( !StripingItem ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

		   StripingItem->Device=FirstTable[i].Device;
		   StripingItem->Party=FirstTable[i].Party;
		   StripingItem->Begin=BR.BPB.HiddenSectors;
		   StripingItem->End=BR.BPB.HiddenSectors+BR.BPB.TotalSectors-1lu; // because 0 + 3 = 3, <0,3> == 4 sec
		   StripingItem->Number= StripingItem->End - StripingItem->Begin + 1lu;
		   StripingItem->Type=DATA_STRIP;

		   StripingSession.DataHDs.Insert( StripingItem );

		   break;

   case PARITY_STRIP:

		   StripingSession.ParityHD.Device=FirstTable[i].Device;
		   StripingSession.ParityHD.Party=FirstTable[i].Party;
		   StripingSession.ParityHD.Begin=BR.BPB.HiddenSectors;
		   StripingSession.ParityHD.End=BR.BPB.HiddenSectors+BR.BPB.TotalSectors-1lu; // because 0 + 3 = 3, <0,3> == 4 sec
		   StripingSession.ParityHD.Number=StripingSession.ParityHD.End - StripingSession.ParityHD.Begin + 1lu;
		   StripingSession.ParityHD.Type=PARITY_STRIP;

		   break;
  } // switch

 } // for


 if( FirstTable ) { farfree( FirstTable ); }

 return ERR_FS_NO_ERROR;
}



//--- StripingConfigure -------------------------------------------------------


word StripingConfigure( DataPartysQueue far *DirtyDataPartys )
{
 bool  Exit=FALSE;
 int   iDevice,
       iParty;
 byte  Device,
       Party;
 dword DataPartySize,
       ParityPartySize;

 MasterBootRecord BR;
 StripItem 	  far *StripingItem;

 // init sizes
 ParityPartySize = StripingSession.ParityHD.Number; // is 0 or is set

 if( StripingSession.DataHDs.Head->Next != StripingSession.DataHDs.Tail )
  DataPartySize = ((StripItem far *)(StripingSession.DataHDs.Head->Next))->Number; // all same size
 else
  DataPartySize=0;		// no data device



 textcolor( WHITE );
  cprintf("\r\n\n Striping CONFIGURE utility\n");
 textcolor( LIGHTGRAY );

 printf(
	"\n Actions to do:"
	"\n  N ... oN striping"
	"\n  O ... Off striping"
	"\n  D ... insert Data party to striping session"
	"\n  P ... insert Parity party to striping session"
	"\n  W ... write what's choosen to data and parity partys"
	"\n  F ... Free data party from striping session"
	"\n  E ... freE parity party from striping session"
	"\n  S ... Show who is in striping session"
	"\n  Q ... Quit and write results"
	"\n"
	"\n Choose:"
       );

 while( !Exit )
 {

  switch( getch() )
  {
   case 'N':
   case 'n':
	    // on striping
	    StripingSession.StripingIs=ACTIVE;
	    printf(" ON striping \n  -> memory flag was set...");

	    break;

   case 'O':
   case 'o':
	    // off striping
	    StripingSession.StripingIs=NONACTIVE;
	    printf(" OFF striping \n  -> memory flag was set...");

	    break;

   case 'D':
   case 'd':
	    printf(" Insert data party...");

	    // print what is in system
	    printf("\n Clean data partys:");
	    DataPartys->Show();

	    printf("\n Dirty data partys:");
	    DirtyDataPartys->Show();

	    printf("\n\n Enter device - 0..4:");
	    iDevice=getch(); Device=(byte)iDevice-'0'; printf(" %u", Device );
	    if( Device>=5)
	    {
	     printf("\n Device %d out of interval 0..4", Device );
	     break;
	    }

	    printf("\n Enter party - 1..4:");
	    iParty=getch(); Party=(byte)iParty-'0'; printf(" %u", Party );
	    if( Party<1 || Party>4 )
	    {
	     printf("\n Party ID %d out of interval 1..4", Party );
	     break;
	    }


	    // now I have party and device => add it to striping session
	    StripingItem=(StripItem far *)farmalloc(sizeof(StripItem));
	    if( !StripingItem ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	    StripingItem->Device=Device;
	    StripingItem->Party=Party;

	    if( LoadBoot( Device, Party, &BR ) )
	    {
	     // device not usable => can't be add
	     printf("\n This Device/Party is not in system => was NOT add...");
	     farfree( StripingItem );
	     break;
	    }

	    StripingItem->Begin=BR.BPB.HiddenSectors;
	    StripingItem->End=BR.BPB.HiddenSectors+BR.BPB.TotalSectors-1lu; // because 0 + 3 = 3, <0,3> == 4 sec
	    StripingItem->Number=StripingItem->End - StripingItem->Begin + 1lu;;
	    StripingItem->Type=DATA_STRIP;

	    // check size:
	    //  - every data partys must have the same size
	    //  - data party size <= parity party size + 2
	    if(
		DataPartySize != 0
		&&
		StripingItem->Number != DataPartySize
	      )
	    {
	     printf("\n Error: Party can not be add because has different size...");
	     farfree( StripingItem );
	     break;
	    }
	    if(
		ParityPartySize != 0
		&&
		( StripingItem->Number > (ParityPartySize+2lu) )
	      )
	    {
	     printf("\n Error: Party can not be add because is too big for defined parity party..."
		    "\n        ( required: DataPartySecSize <= ParityPartySecSize+2 )");
	     farfree( StripingItem );
	     break;
	    }

	    if( DataPartySize==0 )
	     DataPartySize=StripingItem->Number;

	    StripingSession.DataHDs.Insert( StripingItem );

	    printf("\n Party was add to session...");

	    break;

   case 'P':
   case 'p':
	    printf(" Insert parity party...");

	    // print what is in system
	    printf("\n Clean data partys:");
	    DataPartys->Show();

	    printf("\n Dirty data partys:");
	    DirtyDataPartys->Show();

	    printf("\n\n Enter device - 0..4:");
	    iDevice=getch(); Device=(byte)iDevice-'0'; printf(" %u", Device );
	    if( Device>=5)
	    {
	     printf("\n Device %d out of interval 0..4", Device );
	     break;
	    }

	    printf("\n Enter party - 1..4:");
	    iParty=getch(); Party=(byte)iParty-'0'; printf(" %u", Party );
	    if( Party<1 || Party>4 )
	    {
	     printf("\n Party ID %d out of interval 1..4", Party );
	     break;
	    }


	    // now I have party and device => add it to striping session
	    StripingSession.ParityHD.Device=Device;
	    StripingSession.ParityHD.Party=Party;

	    if( LoadBoot( Device, Party, &BR ) )
	    {
	     // device not usable => can't be add
	     printf("\n This Device/Party is not in system => was NOT add...");
	     break;
	    }

	    StripingSession.ParityHD.Begin=BR.BPB.HiddenSectors;
	    StripingSession.ParityHD.End=BR.BPB.HiddenSectors+BR.BPB.TotalSectors-1lu; // because 0 + 3 = 3, <0,3> == 4 sec
	    StripingSession.ParityHD.Number=StripingSession.ParityHD.End - StripingSession.ParityHD.Begin + 1lu;;
	    StripingSession.ParityHD.Type=PARITY_STRIP;

	    // check size:
	    //  - every data partys must have the same size
	    //  - data party size <= parity party size + 2
	    if(
		DataPartySize != 0
		&&
		((StripingSession.ParityHD.Number-2lu) < DataPartySize )
	      )
	    {
	     printf("\n Error: Party too small"
		    "\n        ( required: DataPartySecSize <= ParityPartySecSize+2 )...");

	     StripingSession.ParityHD.Device=
	     StripingSession.ParityHD.Party=
	     StripingSession.ParityHD.Number=
	     StripingSession.ParityHD.End=
	     StripingSession.ParityHD.Begin=0lu;

	     break;
	    }


	    printf("\n Party was add to session...");

	    break;

   case 'w':
   case 'W':
	    printf(" Write results...");

	    // to each party from session queue write table
	    printf("\n -> Writing ON tables...");
	    StripingSession.DataHDs.WriteTablesToPartys(); // these function writes to DataHDs and to ParityHD too

	    printf("\n -> Writing OFF tables...");
	    StripingSession.DataHDs.WriteOffTablesToPartys( DirtyDataPartys );

	    // count parity to parity party
	    printf("\n Writing parity to parity party will be done in exit time...");


	    break;

   case 'f':
   case 'F':
	    printf(" Free DATA party...");

	    printf("\n\n Enter device - 0..9:");
	    iDevice=getch(); Device=(byte)iDevice-'0'; printf(" %u", Device );

	    printf("\n Enter party - 1..4:");
	    iParty=getch(); Party=(byte)iParty-'0'; printf(" %u", Party );

	    if( StripingSession.DataHDs.DeleteParty( Device, Party ) )
	    {
	     // device not in session => not deleted
	     printf("\n This Device/Party is not in session => was NOT deleted...");
	     break;
	    }

	    // if empty set size to 0
	    if( StripingSession.DataHDs.Head->Next == StripingSession.DataHDs.Tail )
	     DataPartySize=0;		// no data device

	    printf("\n Deleted...");

	    break;

   case 'e':
   case 'E':
	    printf(" Free PARITY party...");

	    StripingSession.ParityHD.Device=
	    StripingSession.ParityHD.Party=
	    StripingSession.ParityHD.Number=
	    StripingSession.ParityHD.End=
	    StripingSession.ParityHD.Begin=0lu;

	    ParityPartySize=0lu;	// not initialized

	    printf("\n Deleted...");

	    break;

   case 's':
   case 'S':
	    printf(" Show state...");

	    // show data devices
	    printf("\n STRIPING is:			");
	    if( StripingSession.StripingIs==ACTIVE )
	    {
             printf("ON");
	    }
	    else
	    {
	     printf("OFF");
	    }


	    printf("\n DATA Partys:");
	    StripingSession.DataHDs.Show();

	    printf("\n PARITY Party:");
	    if( StripingSession.ParityHD.Number )
	    {
	     printf("\n Dev 0x8%d Party %d LogHDBeg %8lu LogHDEnd %8lu Lng %5u",
			StripingSession.ParityHD.Device,
			StripingSession.ParityHD.Party,
			StripingSession.ParityHD.Begin,
			StripingSession.ParityHD.End,
			StripingSession.ParityHD.Number
		  );
	    }

	    break;

   case '?':
   case 'h':
   case 'H':
	    printf("\n"
			"\n Help:"
			"\n  N ... oN striping"
			"\n  O ... Off Striping and quit"
			"\n  D ... insert Data party to striping session"
			"\n  P ... insert Parity party to striping session"
			"\n  W ... write what's choosen to data and parity partys"
			"\n  F ... Free DATA party from striping session"
			"\n  E ... freE PARITY party from striping session"
			"\n  S ... Show who is in striping session"
			"\n  Q ... Quit and write results"
			"\n"
		  );

	    break;

   case 'Q':
   case 'q':
	    // end of work -> striping is off
	    printf(" Quit and write...");

	    Exit=TRUE;


	    if( StripingSession.StripingIs==ACTIVE )
	    {
	     // existence of partys is done in InitStripingLoadAndCheckTables
	     // and when device is inserted, here check other things
	     printf("\n Checking...");



	     if( StripingSession.ParityHD.Number==0 )
	     {
	      printf("\n\n Error: Striping is ON and parity party not set...");
	      Exit=FALSE; break;
	     }

	     if( StripingSession.DataHDs.Head->Next
		 ==
		 StripingSession.DataHDs.Tail
	       )
	     {
	      printf("\n\n Error: Striping is ON and 0 data devices in session...");
	      Exit=FALSE; break;
	     }

	     // everything should be OK

	     // => write tables
	     printf("\n -> Writing ON tables...");
	     StripingSession.DataHDs.WriteTablesToPartys(); // these function writes to DataHDs and to ParityHD too
	     printf("\n -> Writing OFF tables...");
	     StripingSession.DataHDs.WriteOffTablesToPartys( DirtyDataPartys );

	     // => recount parity
	    #ifdef FAST_PARITY
	     STRIPFastUpdateParityParty();
	    #else
	     STRIPUpdateParityParty();
	    #endif

	    }



	    if( StripingSession.StripingIs==NONACTIVE )
	    {
	     // write OFF striping tables everywhere

	     // delete everything from queue and null strip party info
	     StripingSession.DataHDs.Destroy();
	     StripingSession.ParityHD.Device=
	     StripingSession.ParityHD.Party=
	     StripingSession.ParityHD.Number=
	     StripingSession.ParityHD.End=
	     StripingSession.ParityHD.Begin=0lu;

	     printf("\n -> Writing OFF tables");
	     StripingSession.DataHDs.WriteOffTablesToPartys( DirtyDataPartys );

	     printf("\n -> written...");
	    }

	    continue;
  }

  printf("\n\n Choose:");

 }

 printf("\n\n Striping configure done\n");

 return ERR_FS_NO_ERROR;
}



//--- RecountParity -------------------------------------------------------



word RecountParity( dword far *Parity, dword far *Data, dword HowMany=512lu )
{
 dword Counter;

 for( Counter=0lu; Counter<(HowMany/4lu); Counter++ )
  Parity[Counter] ^= Data[Counter]; 	     	// count parity

 return ERR_FS_NO_ERROR;
}



//--- StripingRescue ----------------------------------------------------------



word StripingRescue( DataPartysQueue far *DirtyDataPartys )
// - one data device is dead => ask user where to rescue data
// - if parity party is dead => it has no sense to rescue it from other
//   partys. Easier and better is recount parity from data partys
//   to parity party
{
 bool Exit=FALSE;
 int  iDevice,
      iParty;
 byte Device,
      Party;

 MasterBootRecord BR;
 StripItem 	  far *StripingItem;


 textcolor( LIGHTRED );
  cprintf("\r\n\n\n Striping RESCUE utility\n");
 textcolor( LIGHTGRAY );

 printf(
	"\n Situation:"
	"\n  1 data party is dead => data CAN BE RESCUED,"
	"\n  reconfiguration of session will be done later."
	"\n"
	"\n  R ... Rescue data to prepared Dev Party"
	"\n  F ... Forget data ( data will be lost )"
	"\n  S ... Show who alive in striping session"
	"\n  H ... Help"
	"\n  Q ... Quit"
	"\n"
	"\n Choose:"
       );

 while( !Exit )
 {

  switch( getch() )
  {
   case 'r':
   case 'R':
	    // ask for device and party where to rescue

	    // recount data from other to new

	    // recount content of parity party

	    printf(" Rescue data to specified party...");

	    // show what is in system
	    printf("\n Clean data partys:");
	    DataPartys->Show();
	    printf("\n Dirty data partys:");
	    DirtyDataPartys->Show();

	    printf("\n\n Enter dest device for rescued data - 0..9:");
	    iDevice=getch(); Device=(byte)iDevice-'0'; printf(" %u", Device );

	    printf("\n Enter dest party for rescued data - 1..4:");
	    iParty=getch(); Party=(byte)iParty-'0'; printf(" %u", Party );

	    if( Party<1 || Party>4 )
	    {
	     printf("\n Party ID %d out of interval 1..4", Party );
	     break;
	    }



	    // check destination device

	    printf("\n Checking...");

	    if( StripingSession.ParityHD.Number==0 )
	    {
	     printf("\n\n Error: parity party not set...");
	     break;
	    }

	    if( StripingSession.DataHDs.Head->Next
		 ==
		StripingSession.DataHDs.Tail
	      )
	    {
	     printf("\n\n Error: 0 data devices in session...");
	     break;
	    }

	    if( LoadBoot( Device, Party, &BR ) )
	    {
	     // device not usable => can't be add
	     printf("\n This Device/Party is not in system => data can NOT be rescued to it...");
	     break;
	    }



	    printf("\n Party is OK ..."
		   "\n Rescue can begin..."
		  );

	    // calling real rescue function
	    #ifdef FAST_PARITY
	     STRIPFastRescueParty( Device, Party );
	    #else
	     STRIPRescueParty( Device, Party );
	    #endif

	    // now I try add party with rescued data to striping session
	    StripingItem=(StripItem far *)farmalloc(sizeof(StripItem));
	    if( !StripingItem ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

	    StripingItem->Device=Device;
	    StripingItem->Party=Party;
	    StripingItem->Begin=BR.BPB.HiddenSectors;
	    StripingItem->End=BR.BPB.HiddenSectors+BR.BPB.TotalSectors-1lu; // because 0 + 3 = 3, <0,3> == 4 sec
	    StripingItem->Number= StripingItem->End - StripingItem->Begin + 1lu;
	    StripingItem->Type=DATA_STRIP;

	    StripingSession.DataHDs.Insert( StripingItem );

	    // parity will be recounted in reconfigure time

	    break;
   case 'f':
   case 'F':
	     printf(" Do not rescue data...");
	     printf("\n  Any action will be done now."
		    "\n  Parity party will be repaired ( recounted ) in config time...");

	     Exit=TRUE;

	     continue;

   case 's':
   case 'S':
	    printf(" Show who is in striping session...");

	    // show data devices
	    printf("\n DATA Partys:");
	    StripingSession.DataHDs.Show();

	    printf("\n PARITY Party:");
	    if( StripingSession.ParityHD.Number )
	    {
	     printf("\n Dev 0x8%d Party %d LogHDBeg %8lu LogHDEnd %8lu Lng %5u",
			StripingSession.ParityHD.Device,
			StripingSession.ParityHD.Party,
			StripingSession.ParityHD.Begin,
			StripingSession.ParityHD.End,
			StripingSession.ParityHD.Number
		  );
	    }

	    break;

   case '?':
   case 'h':
   case 'H':
	    printf("\n"
			"\n Help:"
			"\n  R ... Rescue"
			"\n  F ... Forget data ( data will be lost )"
			"\n  S ... Show who alive in striping session"
			"\n  H ... Help"
			"\n  Q ... Quit"
			"\n"
		  );

	    break;

   case 'Q':
   case 'q':
	    // end of work -> striping is off
	    printf(" Quit...");

	    Exit=TRUE;


	    continue;
  }

  printf("\n\n Choose:");

 }

 printf("\n\n Striping RESCUE done\n");

 return ERR_FS_NO_ERROR;

}


//--- InitializeStriping -------------------------------------------------------



word InitializeStriping( void far *DirtyDataPartys )
// structure StripingSession will be initialized and filled
{
 word i;
 byte Dead=0,
      DeadType=0;


 #ifdef DEBUG
  printf("\n\n STRIPING INITialization...");
 #endif


 // --- LOAD OF EXISTING STRIPE.TABs ---

 InitStripingLoadAndCheckTables( ( DataPartysQueue far *)DirtyDataPartys,
				 Dead,
				 DeadType
			       );

 // --- ANALYZE CHECK RESULTS ---

 if( Dead == 0 )
 {
  // success: tables are OK -> are the same and all devices are live

  #ifdef DEBUG
   printf("\n Striping tables OK -> all devices are live || not exist...");
  #endif


  // check if striping is possible and ON it...
  printf("\n\n Checking ->");

  if( StripingSession.ParityHD.Number==0
       ||
      StripingSession.DataHDs.Head->Next==StripingSession.DataHDs.Tail
    )
  {
   printf(" parity party not set || 0 data devices in session => OFF striping");
   StripingSession.StripingIs=NONACTIVE;
  }
  else
  {
   printf(" OK => striping switched to ACTIVE MODE RIGHT NOW!\n");
   StripingSession.StripingIs=ACTIVE;
  }

  // if user wants can reconfigure system

  // --- WAIT FOR USER PRESSING A KEY => ENTER STRIPING CONFIGURE ---
  textcolor( LIGHTBLUE );
   cprintf("\r\n\n Press ESC to enter STRIPING SETUP\n");
  textcolor( LIGHTGRAY );

  KeybClear();

  for(i=0; i<20; i++ )
  {
   delay(100);

   if( kbhit() )
   {
    if( getch()==27 ) // esc pushed
    {
     KeybClear();

     StripingSession.StripingIs=ACTIVE;
     StripingConfigure( (DataPartysQueue far *)DirtyDataPartys );
    }
   }
  }

  KeybClear();

  return ERR_FS_NO_ERROR;
 }



 if( Dead == 1 && DeadType == DATA_STRIP )
 {
  // only one party is dead => RESCUE is possible!

  StripingRescue( (DataPartysQueue far *)DirtyDataPartys );

  StripingSession.StripingIs=ACTIVE;

  // reconfigure of session demanded
  KeybClear();
  StripingConfigure( (DataPartysQueue far *)DirtyDataPartys );

  return ERR_FS_NO_ERROR;
 }


 // more then 1 party is dead => rescue impossible && reconfiguration demanded
 // or 1 party dead and is parity => parity will be recounted after reconfig

 textcolor( LIGHTRED );
  cprintf("\r\n\n\n Striping RESCUE utility can not be used => more than 1 party is dead \n");
 textcolor( LIGHTGRAY );

 KeybClear();
 StripingConfigure( (DataPartysQueue far *)DirtyDataPartys );



 return ERR_FS_NO_ERROR;
}



//- ShutdownStriping ----------------------------------------------------------------



word ShutdownStriping( void )
{
 #ifdef DEBUG
  printf("\n\n STRIPING SHUTDOWN...");
 #endif

 StripingSession.StripingIs=NONACTIVE;

 StripingSession.DataHDs.Destroy();

 return ERR_FS_NO_ERROR;
}


#undef DEBUG


//- CLASSICWriteLogicalSector ----------------------------------------------------------------



word CLASSICWriteLogicalSector( byte Device, dword Logical, void far *Buffer, byte Number )
// MBR a aktivni BR budou natazeny v pameti
// je to ok, protoze i physical fce umi natahovat take bloky
{
 word   bytes;

 #ifdef DEBUG
  printf("\n  CLASSIC_WRITE sector %lu,", Logical );
 #endif

 lseek( HDHandle[Device&0x0F], Logical*512lu, SEEK_SET );

 if( _dos_write( HDHandle[Device&0x0F], Buffer, Number*512u, &bytes) != 0 )
 {
  #ifdef SEE_ERROR
   printf("\n Write error: logical sector %lu", Logical );
  #endif
  return ERR_FS_WR_LOGICAL_SECTOR;
 }

 #ifdef DEBUG
  printf(" written %d of %d bytes OK", bytes, Number*512u);
 #endif
 return ERR_FS_NO_ERROR;
}



//- CLASSICReadLogicalSector ----------------------------------------------------------------



word CLASSICReadLogicalSector( byte Device, dword Logical, void far *Buffer, byte Number )
// MBR a aktivni BR budou natazeny v pameti
{
 word   bytes;

 #ifdef DEBUG
  printf("\n  CLASSIC_READ sector %lu,", Logical );
 #endif

 lseek( HDHandle[Device&0x0F], Logical*512lu, SEEK_SET );

 if( _dos_read( HDHandle[Device&0x0F], Buffer, Number*512u, &bytes) != 0 )
 {
  #ifdef SEE_ERROR
   printf("\n Read error: logical sector %lu", Logical );
  #endif
  return ERR_FS_RD_LOGICAL_SECTOR;
 }

 #ifdef DEBUG
  printf(" read %d of %d bytes OK", bytes, Number*512u);
 #endif
 return ERR_FS_NO_ERROR;
}



//- STRIPWriteLogicalSector ----------------------------------------------------------------


// #define DEBUG

word STRIPWriteLogicalSector( byte  Device,
			      dword Logical,
			      void  far *Buffer,
			      byte  Number )
// Here you see very strange solutin. Under UNIX I would use
// fork and would alocate whole buffers. Because under DOS
// there is only 600k, I must do it this stupid way...
//
{
 #ifdef DEBUG
  printf("\n  STRIP_WRITE sector %lu on Dev %d... ", Logical, Device );
 #endif

 // on stack because it is much more faster then farmalloc
 byte ParityBuffer[512],
      WorkBuffer[512];

 byte far *UsrBuffer;

 word  i;

 dword Offset,   		// level I am working on ( offset in party )
       OffsetEnd,               // level where to stop
       ParityLogical,
       DataLogical;

 StripItem far *At,
	   far *Dest;



 if( StripingSession.DataHDs.Head->Next == NULL )
 {
  #ifdef DEBUG
   printf("\n No striping data device in system...");
  #endif

  return ERR_FS_NO_ERROR;
 }



 // --- COUNT OFFSETS ---
 // first I must find party I will write to because I need
 // offset in this party...
 // ( it will be everytime found! )
 At=(StripItem far *)StripingSession.DataHDs.Head->Next;

 while( At!=((StripItem far *)(StripingSession.DataHDs.Tail)) )
 {
  if( At->Device==Device
      &&  				// check if logical is in interval
      At->Begin<=Logical            	// which is striped
      &&
      Logical<=At->End
    )
  {
   Dest=At;				// remember node of dest party

   Offset
   =
    Logical
    -
    Dest->Begin;


   OffsetEnd
   =
    Offset
    +
    Number
    -
    1;					// <0,2> -> 3, 0+3=3 but is 4

   break;				// destination found
  }

  // take next
  At=(StripItem far *)At->Next;
 }



 // each sector done separately
 for( ; Offset<=OffsetEnd; Offset++ )
 {
  // clear parity buffer
  for( i=0; i<512; i++ ) ParityBuffer[i]=0;

  // count parity of current level
  At=StripingSession.DataHDs.FindFirstParty();

  do
  {
   if( At != Dest ) // DO NOT COUNT PARITY FROM SEC I WILL WRITE TO
   {
    DataLogical
    =
     At->Begin
     +
     Offset;

    CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

    RecountParity( (dword far *)ParityBuffer, (dword far *)WorkBuffer );
   }
  }
  while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );



  // Recount parity with it what caller wants to write
  UsrBuffer =  (byte far *)Buffer;
   // move pointer in buffer to actuall
  UsrBuffer += 512ul*(Dest->Begin+Offset-Logical);

  RecountParity( (dword far *)ParityBuffer, (dword far *)UsrBuffer );


  // write data to data disk
  DataLogical
  =
   Dest->Begin
   +
   Offset;

  CLASSICWriteLogicalSector( Dest->Device,
			     DataLogical,
			     UsrBuffer,
			     1u
			   );



  // write parity to parity disk
  ParityLogical
  =
   StripingSession.ParityHD.Begin       // begin in parity HD,
   +
   2lu                                  // +1 is BR  +1 is 2BR
   +
   Offset;       		 	// offset in dest party

  CLASSICWriteLogicalSector( StripingSession.ParityHD.Device,
			     ParityLogical,
			     ParityBuffer,
			     1u
			   );


 } // for


 return ERR_FS_NO_ERROR;
}



//- STRIPReadLogicalSector ----------------------------------------------------------------



word STRIPReadLogicalSector( byte  Device,
			     dword Logical,
			     void  far *Buffer,
			     byte  Number
			   )
{
 #ifdef DEBUG
  printf("\n  STRIP_READ sector %lu on Dev %d... ", Logical, Device );
 #endif

 // on stack because it is much more faster then farmalloc
 byte ParityBuffer[512],
      WorkBuffer[512];

 byte far *UsrBuffer;

 word  i;

 dword Offset,   		// level I am working on ( offset in party )
       OffsetEnd,               // level where to stop
       ParityLogical,
       DataLogical;

 StripItem far *At,
	   far *Dest;



 if( StripingSession.DataHDs.Head->Next == NULL )
 {
  #ifdef DEBUG
   printf("\n No striping data device in system...");
  #endif

  return ERR_FS_NO_ERROR;
 }



 // --- COUNT OFFSETS ---
 // first I must find party I will write to because I need
 // offset in this party...
 // ( it will be everytime found! )
 At=(StripItem far *)StripingSession.DataHDs.Head->Next;

 while( At!=((StripItem far *)(StripingSession.DataHDs.Tail)) )
 {
  if( At->Device==Device
      &&  				// check if logical is in interval
      At->Begin<=Logical            	// which is striped
      &&
      Logical<=At->End
    )
  {
   Dest=At;				// remember node of dest party

   Offset
   =
    Logical
    -
    Dest->Begin;


   OffsetEnd
   =
    Offset
    +
    Number
    -
    1;					// <0,2> -> 3, 0+3=3 but is 4

   break;				// destination found
  }

  // take next
  At=(StripItem far *)At->Next;
 }



 // each sector done separately
 for( ; Offset<=OffsetEnd; Offset++ )
 {
  // clear parity buffer
  for( i=0; i<512; i++ ) ParityBuffer[i]=0;

  // count parity of current level
  At=StripingSession.DataHDs.FindFirstParty();

  do
  {
   DataLogical
   =
    At->Begin
    +
    Offset;

   CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

   RecountParity( (dword far *)ParityBuffer, (dword far *)WorkBuffer );

   if( At==Dest )
   {
    // in buffer are data what caller wants => copy them
    UsrBuffer =  (byte far *)Buffer;
     // move pointer in buffer to actuall
    UsrBuffer += 512ul*(Dest->Begin+Offset-Logical);

    for( i=0; i<512; i++ ) UsrBuffer[i]=WorkBuffer[i];
   }
  }
  while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );



  // check parity ( read it to wrk buffer )

  ParityLogical
  =
   StripingSession.ParityHD.Begin       // begin in parity HD,
   +
   2lu                                  // +1 is BR  +1 is 2BR
   +
   Offset;       		 	// offset in dest party

  CLASSICReadLogicalSector( StripingSession.ParityHD.Device,
			    ParityLogical,
			    WorkBuffer,
			    1u
			  );

  if( !BuffersAreTheSame( WorkBuffer, ParityBuffer, 512lu ) )
  {
   // parity not fit => save right parity ( rescue is nonsens )

   CLASSICWriteLogicalSector( StripingSession.ParityHD.Device,
			      ParityLogical,
			      ParityBuffer,
			      1u
			    );
   #ifdef DEBUG
    printf(" Parity corrupted => rewritten to valid...");
   #endif
  }
  #ifdef DEBUG
   else
    printf(" Parity OK...");
  #endif


 } // for


 return ERR_FS_NO_ERROR;
}


//- WriteLogicalSector ----------------------------------------------------------------

#undef DEBUG

word WriteLogicalSector( byte Device, dword Logical, void far *Buffer, byte Number )
// this function is used by upper level
{
 #ifdef DEBUG
  printf("\n WRITEing sector %lu ... ", Logical );
 #endif

 if(
     StripingSession.StripingIs==ACTIVE
     &&
     StripingSession.DataHDs.IsInStripingArea( Device, Logical )
   )
 {
  #ifdef DEBUG
   printf(" STRIPing version...", Logical );
  #endif

  return STRIPWriteLogicalSector( Device, Logical, Buffer, Number );
 }
 else
 {
  return CLASSICWriteLogicalSector( Device, Logical, Buffer, Number );
 }
}



//- ReadLogicalSector ----------------------------------------------------------------



word ReadLogicalSector( byte Device, dword Logical, void far *Buffer, byte Number )
// this function is used by upper level
{
 #ifdef DEBUG
  printf("\n READing sector %lu... ", Logical );
 #endif

 if(
     StripingSession.StripingIs==ACTIVE
     &&
     StripingSession.DataHDs.IsInStripingArea( Device, Logical )
   )
 {
  #ifdef DEBUG
   printf(" STRIPing version...", Logical );
  #endif

  return STRIPReadLogicalSector( Device, Logical, Buffer, Number );
 }
 else
 {
  return CLASSICReadLogicalSector( Device, Logical, Buffer, Number );
 }
}



//- STRIPUpdateParityParty ----------------------------------------------------------------


word STRIPUpdateParityParty( void )
{
 byte  ParityBuffer[512], // on stack because it is much more faster then farmalloc
       WorkBuffer[512];

 word  i;

 dword ParityLogical,
       DataLogical;

 StripItem far *At;



 printf("\n  STRIP_UPDATE_PARITY_DISK Dev %d Party %d... ", StripingSession.ParityHD.Device, StripingSession.ParityHD.Party );


 if( StripingSession.DataHDs.Head->Next == NULL )
 {
  printf("\n No striping data device in system...");

  return ERR_FS_NO_ERROR;
 }

 printf("\n Progress ( Offset ):\n");

 dword Offset; 	// offset in data party


 for( Offset=0;
      Offset<((StripItem far *)StripingSession.DataHDs.Head->Next)->Number;
      Offset++
    )
 {

  At=StripingSession.DataHDs.FindFirstParty();

  // clear parity buffer
  for( i=0; i<512; i++ ) ParityBuffer[i]=0x00;

  // count parity of level
  do
  {
   // load data to count parity from each data device
   DataLogical
   =
    At->Begin
    +
    Offset;

   // load data
   CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

   // recount parity
   RecountParity( (dword far *)ParityBuffer, (dword far *)WorkBuffer );
  }
  while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );



  // write parity to parity disk
  ParityLogical
  =
   StripingSession.ParityHD.Begin            // begin in parity HD,
   +
   2lu                                       // +1 is BR  +1 is 2BR
   +
   Offset;                                   // offset in data partys

  CLASSICWriteLogicalSector( StripingSession.ParityHD.Device,
			     ParityLogical,
			     ParityBuffer,
			     1u
			   );

  printf("\r %8lu", Offset );

 } // for Counter



 return ERR_FS_NO_ERROR;
}


//- STRIPFastUpdateParityParty ----------------------------------------------------------------


#define BUF_SIZE  100lu
#define BUF_SIZEW 100u

word STRIPFastUpdateParityParty( void )
{
 byte  far *ParityBuffer=(byte far *)farmalloc(512lu*BUF_SIZE); // 51200B
  if( !ParityBuffer ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

 byte  far *WorkBuffer=(byte far *)farmalloc(512lu*BUF_SIZE);
  if( !WorkBuffer ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

 word  i;

 dword ParityLogical,
       DataLogical;

 StripItem far *At;



 printf("\n  STRIP_fast_UPDATE_PARITY_DISK Dev %d Party %d... ", StripingSession.ParityHD.Device, StripingSession.ParityHD.Party );


 if( StripingSession.DataHDs.Head->Next == NULL )
 {
  printf("\n No striping data device in system...");

  return ERR_FS_NO_ERROR;
 }

 printf("\n Progress - offset < 0, %lu -1 >:\n", ((StripItem far *)StripingSession.DataHDs.Head->Next)->Number );

 dword Offset; 	// offset in data party


 // first do /BUF_SIZE then do the rest %BUF_SIZE

 // divide by BUF_SIZE - nr of sectors
 dword Whole=((StripItem far *)StripingSession.DataHDs.Head->Next)->Number/BUF_SIZE;

 // --- DO THE WHOLE ---

 for( Offset=0;
      Offset<Whole*BUF_SIZE;
      Offset+=BUF_SIZE
    )
 {

  At=StripingSession.DataHDs.FindFirstParty();

  // clear parity buffer
  for( i=0; i<512*BUF_SIZE; i++ ) ParityBuffer[i]=0x00;

  // count parity of level
  do
  {
   // load data to count parity from each data device
   DataLogical
   =
    At->Begin
    +
    Offset;

   // load data
   CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, BUF_SIZEW );

   // recount parity
   RecountParity( (dword far *)ParityBuffer, (dword far *)WorkBuffer, BUF_SIZE*512lu );
  }
  while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );



  // write parity to parity disk
  ParityLogical
  =
   StripingSession.ParityHD.Begin            // begin in parity HD,
   +
   2lu                                       // +1 is BR  +1 is 2BR
   +
   Offset;                                   // offset in data partys

  CLASSICWriteLogicalSector( StripingSession.ParityHD.Device,
			     ParityLogical,
			     ParityBuffer,
			     BUF_SIZEW
			   );

  printf("\r %8lu", Offset );


 } // for Counter





 // --- DO THE REST ---

 for( Offset=Whole*BUF_SIZE;
      Offset<((StripItem far *)StripingSession.DataHDs.Head->Next)->Number;
      Offset++
    )
 {

  At=StripingSession.DataHDs.FindFirstParty();

  // clear parity buffer
  for( i=0; i<512; i++ ) ParityBuffer[i]=0x00;

  // count parity of level
  do
  {
   // load data to count parity from each data device
   DataLogical
   =
    At->Begin
    +
    Offset;

   // load data
   CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

   // recount parity
   RecountParity( (dword far *)ParityBuffer, (dword far *)WorkBuffer );
  }
  while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );



  // write parity to parity disk
  ParityLogical
  =
   StripingSession.ParityHD.Begin            // begin in parity HD,
   +
   2lu                                       // +1 is BR  +1 is 2BR
   +
   Offset;                                   // offset in data partys

  CLASSICWriteLogicalSector( StripingSession.ParityHD.Device,
			     ParityLogical,
			     ParityBuffer,
			     1u
			   );

  printf("\r %8lu", Offset );


 } // for Counter


 farfree( ParityBuffer );
 farfree( WorkBuffer );

 printf(" -> Danone...");

 return ERR_FS_NO_ERROR;
}



//- STRIPRescueParty ----------------------------------------------------------------



word STRIPRescueParty( byte DestDevice, byte DestParty )
// Presumptions:
//  - StripingSession.DataHDs contains only partys which alives
//    ( party I am rescuing to not included in it DODELAT CHECK )
//  - DestDevice DestParty exists and has the right size
//
// What rescue does:
//  in each first read level parity,
//  then reads whole level and XORs each sec with parity
//  => when everything is read in parity buffer
//  are data which was on lost party
//
//  In the end BR and 2BR are repaired. In original BR are changed
//  only these things which are changed in Format.
//
{
 byte ParityBuffer[512];
 byte WorkBuffer[512];

 word  i;

 dword ParityLogical,
       DataLogical;

 StripItem far *At;

 MasterBootRecord OriginalBR,
		  RescuedBR;

 TheSecondBoot    Original2BR,
		  Rescued2BR;

 dword 		  Offset; 	// offset in data party




 printf("\n  RESCUE_PARTY to Dev %d Party %d... ", DestDevice, DestParty );



 // --- OFF STRIPING FOR NOW ---

 StripingSession.StripingIs=NONACTIVE; // CLASSIC functions must be used



 if( StripingSession.DataHDs.Head->Next == NULL )
 {
  printf("\n No striping data device in system...");

  return ERR_FS_NO_ERROR;
 }



 // --- RESCUE BOOT RECORD ---

 printf("\n Repairing boot record:");

 LoadBoot( DestDevice, DestParty, &OriginalBR );

 printf("\n  Boot record of destination device loaded...");

 // rescue boot

 // read parity from parity party ( BR has offset 0 inside party )
 ParityLogical
 =
  StripingSession.ParityHD.Begin            // begin in parity HD,
  +
  2lu;                                      // +1 is BR  +1 is 2BR

 CLASSICReadLogicalSector( StripingSession.ParityHD.Device,
			   ParityLogical,
			   &RescuedBR,
			   1u
			 );

 printf("\n  Parity for rescue loaded...");

 At=StripingSession.DataHDs.FindFirstParty();

 // XOR parity with alived sectors
 do
 {
  // load data to count parity from each data device
  DataLogical
  =
   At->Begin;		// BR has offset 0

  // load data
  CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

   // recount parity
  RecountParity( (dword far *)&RescuedBR, (dword far *)WorkBuffer );
 }
 while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );


 printf("\n  Coping important informations rescued -> new...");

 // now in RescuedBR is puvodni => copy valid informations

 OriginalBR.BPB.RootFNODEAddr   = RescuedBR.BPB.RootFNODEAddr;
 OriginalBR.BPB.DirtyFlag       = RescuedBR.BPB.DirtyFlag;
 OriginalBR.BPB.NrOfGroups      = RescuedBR.BPB.NrOfGroups;
 OriginalBR.BPB.ReservedSectors = RescuedBR.BPB.ReservedSectors;

 printf("\n   NrOfGroups %lu, ResSecs %u, RootFNODEAddr %lu, DirtyFlag %u...",
	 OriginalBR.BPB.NrOfGroups,
	 OriginalBR.BPB.ReservedSectors,
	 OriginalBR.BPB.RootFNODEAddr,
	 OriginalBR.BPB.DirtyFlag
       );


 printf("\n  Writing repaired Boot Record...");

 SaveBoot( DestDevice, DestParty, &OriginalBR );



 // --- RESCUE THE SECOND BOOT RECORD ---

 printf("\n Repairing the second boot record:");

 ReadLogicalSector( DestDevice,
		    OriginalBR.BPB.HiddenSectors+1lu,// 2BR is 2. sec in party
		    MK_FP( FP_SEG(&Original2BR), FP_OFF(&Original2BR) )
		  );

 printf("\n  The second boot record of destination device loaded...");

 // rescue boot

 // read parity from parity party ( BR has offset 0 inside party )
 ParityLogical
 =
  StripingSession.ParityHD.Begin            // begin in parity HD,
  +
  2lu 	                                    // +1 is BR  +1 is 2BR
  +
  1lu;                                      // BR of rescued

 CLASSICReadLogicalSector( StripingSession.ParityHD.Device,
			   ParityLogical,
			   &Rescued2BR,
			   1u
			 );

 printf("\n  The second boot record parity version loaded...");

 At=StripingSession.DataHDs.FindFirstParty();

 // XOR parity with alived sectors
 do
 {
  // load data to count parity from each data device
  DataLogical
  =
   At->Begin
   +
   1lu;		// 2BR has offset 1

  // load data
  CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

   // recount parity
  RecountParity( (dword far *)&Rescued2BR, (dword far *)WorkBuffer );
 }
 while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );


 printf("\n  Coping important informations rescued -> new...");

 // now in Rescued2BR is puvodni => copy valid informations

 Original2BR.FreeSecs    = Rescued2BR.FreeSecs;
 Original2BR.StatSectors = Rescued2BR.StatSectors;

 printf("\n   FreeSecs %lu, StaSecs %lu...",
	 Original2BR.FreeSecs,
	 Original2BR.StatSectors
       );

 // copy BPB
 movmem((const void *)&(OriginalBR.BPB), (void *)&(Original2BR.BPB), sizeof(BIOSParameterBlock) );

 printf("\n  Writing repaired The Second Boot Record...");

 // write results
 WriteLogicalSector( DestDevice,
		     OriginalBR.BPB.HiddenSectors+1lu,// 2BR is 2. sec in party
		     MK_FP( FP_SEG(&Original2BR), FP_OFF(&Original2BR) )
		   );



 // --- RESCUE THE REST OF PARTY

 printf("\n Repairing the data:");
 printf("\n  Progress ( Offset ):\n");

 At=StripingSession.DataHDs.FindFirstParty();

 // Offset: 0...BR, 1...2BR
 for( Offset=2;
      Offset<((StripItem far *)StripingSession.DataHDs.Head->Next)->Number;
      Offset++
    )
 {

  At=StripingSession.DataHDs.FindFirstParty();

  // clear parity buffer
  for( i=0; i<512; i++ ) ParityBuffer[i]=0x00;

  // count parity of level
  do
  {
   // load data to count parity from each data device
   DataLogical
   =
    At->Begin
    +
    Offset;

   // load data
   CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

   // recount parity
   RecountParity( (dword far *)ParityBuffer, (dword far *)WorkBuffer );
  }
  while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );



  // write parity to parity disk
  ParityLogical
  =
   StripingSession.ParityHD.Begin            // begin in parity HD,
   +
   2lu                                       // +1 is BR  +1 is 2BR
   +
   Offset;                                   // offset in data partys

  CLASSICWriteLogicalSector( StripingSession.ParityHD.Device,
			     ParityLogical,
			     ParityBuffer,
			     1u
			   );

  printf("\r %8lu", Offset );

 } // for Counter



 return ERR_FS_NO_ERROR;
}



//- STRIPFastRescueParty ----------------------------------------------------------------



word STRIPFastRescueParty( byte DestDevice, byte DestParty )
// Presumptions:
//  - StripingSession.DataHDs contains only partys which alives
//    ( party I am rescuing to not included in it DODELAT CHECK )
//  - DestDevice DestParty exists and has the right size
//
// What rescue does:
//  in each first read level parity,
//  then reads whole level and XORs each sec with parity
//  => when everything is read in parity buffer
//  are data which was on lost party
//
//  In the end BR and 2BR are repaired. In original BR are changed
//  only these things which are changed in Format.
//
{
 byte  far *ParityBuffer=(byte far *)farmalloc(512lu*BUF_SIZE); // 51200B
  if( !ParityBuffer ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

 byte  far *WorkBuffer=(byte far *)farmalloc(512lu*BUF_SIZE);
  if( !WorkBuffer ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

 dword  i;

 dword ParityLogical,
       DataLogical;

 StripItem far *At;

 MasterBootRecord OriginalBR,
		  RescuedBR;

 TheSecondBoot    Original2BR,
		  Rescued2BR;

 dword 		  Offset; 	// offset in data party




 printf("\n  RESCUE_PARTY to Dev %d Party %d... ", DestDevice, DestParty );



 // --- OFF STRIPING FOR NOW ---

 StripingSession.StripingIs=NONACTIVE; // CLASSIC functions must be used



 if( StripingSession.DataHDs.Head->Next == NULL )
 {
  printf("\n No striping data device in system...");

  return ERR_FS_NO_ERROR;
 }



 // --- RESCUE BOOT RECORD ---

 printf("\n Repairing boot record:");

 LoadBoot( DestDevice, DestParty, &OriginalBR );

 printf("\n  Boot record of destination device loaded...");

 // rescue boot

 // read parity from parity party ( BR has offset 0 inside party )
 ParityLogical
 =
  StripingSession.ParityHD.Begin            // begin in parity HD,
  +
  2lu;                                      // +1 is BR  +1 is 2BR

 CLASSICReadLogicalSector( StripingSession.ParityHD.Device,
			   ParityLogical,
			   &RescuedBR,
			   1u
			 );

 printf("\n  Parity for rescue loaded...");

 At=StripingSession.DataHDs.FindFirstParty();

 // XOR parity with alived sectors
 do
 {
  // load data to count parity from each data device
  DataLogical
  =
   At->Begin;		// BR has offset 0

  // load data
  CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

   // recount parity
  RecountParity( (dword far *)&RescuedBR, (dword far *)WorkBuffer );
 }
 while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );


 printf("\n  Coping important informations rescued -> new...");

 // now in RescuedBR is puvodni => copy valid informations

 OriginalBR.BPB.RootFNODEAddr   = RescuedBR.BPB.RootFNODEAddr;
 OriginalBR.BPB.DirtyFlag       = RescuedBR.BPB.DirtyFlag;
 OriginalBR.BPB.NrOfGroups      = RescuedBR.BPB.NrOfGroups;
 OriginalBR.BPB.ReservedSectors = RescuedBR.BPB.ReservedSectors;

 printf("\n   NrOfGroups %lu, ResSecs %u, RootFNODEAddr %lu, DirtyFlag %u...",
	 OriginalBR.BPB.NrOfGroups,
	 OriginalBR.BPB.ReservedSectors,
	 OriginalBR.BPB.RootFNODEAddr,
	 OriginalBR.BPB.DirtyFlag
       );


 printf("\n  Writing repaired Boot Record...");

 SaveBoot( DestDevice, DestParty, &OriginalBR );



 // --- RESCUE THE SECOND BOOT RECORD ---

 printf("\n Repairing the second boot record:");

 ReadLogicalSector( DestDevice,
		    OriginalBR.BPB.HiddenSectors+1lu,// 2BR is 2. sec in party
		    MK_FP( FP_SEG(&Original2BR), FP_OFF(&Original2BR) )
		  );

 printf("\n  The second boot record of destination device loaded...");

 // rescue boot

 // read parity from parity party ( BR has offset 0 inside party )
 ParityLogical
 =
  StripingSession.ParityHD.Begin            // begin in parity HD,
  +
  2lu 	                                    // +1 is BR  +1 is 2BR
  +
  1lu;                                      // BR of rescued

 CLASSICReadLogicalSector( StripingSession.ParityHD.Device,
			   ParityLogical,
			   &Rescued2BR,
			   1u
			 );

 printf("\n  The second boot record parity version loaded...");

 At=StripingSession.DataHDs.FindFirstParty();

 // XOR parity with alived sectors
 do
 {
  // load data to count parity from each data device
  DataLogical
  =
   At->Begin
   +
   1lu;		// 2BR has offset 1

  // load data
  CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

   // recount parity
  RecountParity( (dword far *)&Rescued2BR, (dword far *)WorkBuffer );
 }
 while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );


 printf("\n  Coping important informations rescued -> new...");

 // now in Rescued2BR is puvodni => copy valid informations

 Original2BR.FreeSecs    = Rescued2BR.FreeSecs;
 Original2BR.StatSectors = Rescued2BR.StatSectors;

 printf("\n   FreeSecs %lu, StaSecs %lu...",
	 Original2BR.FreeSecs,
	 Original2BR.StatSectors
       );

 // copy BPB
 movmem((const void *)&(OriginalBR.BPB), (void *)&(Original2BR.BPB), sizeof(BIOSParameterBlock) );

 printf("\n  Writing repaired The Second Boot Record...");

 // write results
 WriteLogicalSector( DestDevice,
		     OriginalBR.BPB.HiddenSectors+1lu,// 2BR is 2. sec in party
		     MK_FP( FP_SEG(&Original2BR), FP_OFF(&Original2BR) )
		   );



 // --- RESCUE THE REST OF PARTY



 // --- DO THE WHOLE ---

 printf("\n Repairing the data:");
 printf("\n Progress - offset < 0, %lu -1 >:\n", ((StripItem far *)StripingSession.DataHDs.Head->Next)->Number );

 At=StripingSession.DataHDs.FindFirstParty();

 // first do /BUF_SIZE then do the rest %BUF_SIZE

 // divide by BUF_SIZE - nr of sectors
 dword Whole=(((StripItem far *)StripingSession.DataHDs.Head->Next)->Number+2lu)/BUF_SIZE;

 // Offset: 0...BR, 1...2BR
 for( Offset=2;
      Offset<Whole*BUF_SIZE+2lu;
      Offset+=BUF_SIZE
    )
 {

  At=StripingSession.DataHDs.FindFirstParty();

  // clear parity buffer
  for( i=0; i<512*BUF_SIZE; i++ ) ParityBuffer[i]=0x00;

  // count parity of level
  do
  {
   // load data to count parity from each data device
   DataLogical
   =
    At->Begin
    +
    Offset;

   // load data
   CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, BUF_SIZEW );

   // recount parity
   RecountParity( (dword far *)ParityBuffer, (dword far *)WorkBuffer, BUF_SIZE );
  }
  while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );



  // write parity to parity disk
  ParityLogical
  =
   StripingSession.ParityHD.Begin            // begin in parity HD,
   +
   2lu                                       // +1 is BR  +1 is 2BR
   +
   Offset;                                   // offset in data partys

  CLASSICWriteLogicalSector( StripingSession.ParityHD.Device,
			     ParityLogical,
			     ParityBuffer,
			     BUF_SIZEW
			   );

  printf("\r %8lu", Offset );

 } // for Counter



 // --- DO THE REST ---



 At=StripingSession.DataHDs.FindFirstParty();

 // Offset: 0...BR, 1...2BR
 for( Offset=Whole*BUF_SIZE+2lu;
      Offset<((StripItem far *)StripingSession.DataHDs.Head->Next)->Number;
      Offset++
    )
 {

  At=StripingSession.DataHDs.FindFirstParty();

  // clear parity buffer
  for( i=0; i<512; i++ ) ParityBuffer[i]=0x00;

  // count parity of level
  do
  {
   // load data to count parity from each data device
   DataLogical
   =
    At->Begin
    +
    Offset;

   // load data
   CLASSICReadLogicalSector( At->Device, DataLogical, WorkBuffer, 1u );

   // recount parity
   RecountParity( (dword far *)ParityBuffer, (dword far *)WorkBuffer );
  }
  while( (At=StripingSession.DataHDs.FindNextParty(At)) != NULL );



  // write parity to parity disk
  ParityLogical
  =
   StripingSession.ParityHD.Begin            // begin in parity HD,
   +
   2lu                                       // +1 is BR  +1 is 2BR
   +
   Offset;                                   // offset in data partys

  CLASSICWriteLogicalSector( StripingSession.ParityHD.Device,
			     ParityLogical,
			     ParityBuffer,
			     1u
			   );

  printf("\r %8lu", Offset );

 } // for Counter

 farfree( ParityBuffer );
 farfree( WorkBuffer );

 printf(" -> Danone...");

 return ERR_FS_NO_ERROR;
}



//--- EOF --------------------------------------------------------------------
