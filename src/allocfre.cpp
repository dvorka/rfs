//----------------------------------------------------------------------------
//
//				Note:
//
// This file is included to CACHEMAN.CPP -> is too long so I separate this
//  			because of faster coding :)
//
//
//----------------------------------------------------------------------------

#define WRITE_TO_OKA	TRUE

word CreatePackage( byte Device, byte Party, word &PackageID );

//- OKA structure ---------------------------------------------------------------

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
//    // calling AllocFreeUsingLog();
// ALF_WRITTEN			     End of record

//
// When rollback of OKA is done, ALF is created using Logical and Number
// => then UndoAlfLog() is called => if system crashes, first are *.ALFs
// rollbacked => system is returned to previous state before changes
// which ALF does and beacause changes are idempotent => i can construct
// after crash the same ALF again and do repair the same way.
//  	If UndoAlfLog() returns success I write to *.OKA file to State
// value "ROLLBACKED" => if system crashes record will not be rollbacked
// again

// sea RECOVERY.H for more information

//- CacheManAllocateSector --------------------------------------------------------

word CacheManAllocateSector( byte  Device,     byte Party,
			     dword Previous,   word Number,
			     dword &Logical,   word &GetNumber,
			     word  &PackageID, word Flag
			   )

// What it does:
// - allocates new block of sectors, returns first sector logical number
//   and lenght of block successfuly allocated
// - Previous==0 => means not used
// - Logical is addr of sector. It's logical nr. of sec in Party,
//   this addressing include bitmap sectors too.
// - Adress:   513*GroupID + SectorOffsetInGroup + BMP shift( if Grp Even )
//
// How it does:
// - first searches group where Previous sector belongs to
// - then:
//   - random shot to groups -> use group stat for search hole ->
//     if it fails using stat search next and next and ... group modulo round
//
// Consistency:
// - allocation is done using special function which logs what does
//   => after crash REDO can be done ( sectors are allocated only in
//   commit packege which does REDO operation always )

// DODELAT
// Error when loading bitmap => load bitmap prida do BadGrp.system
// volajici zkousi v cyklu ( treba 10* ) dokud ==ERR_FS_BAD_GRP_SEC
// pri dalsim volani by se uz do ni nemel trefit
// 	Po prvnim neuspechu se musi vynulovat Previous! aby uz hledal
// nahodne...

// FPACK_CREAT   creates log: "A11111.ALF" - 11111 is PackageID which is
//			      created here too. It is used for package
//                            allocations
//			      => result written to A11111.OKA where
//			      11111 is PackageID ( INSIDE ALF )
// FPACK_ADD     creates log: "A11111.ALF" - 11111 is PackageID
//			      It is used for package allocations
//			      => result appended to A11111.OKA where
//			      11111 is PackageID ( INSIDE ALF )
// FPACK_NOTHING creates log: "A11111.NOT" - 11111 is rand number
//			      it is used for swap and log allocations
//			      => result not written to *.OKA

{
 dword 		  GroupID,
		  H;
 word             FirstBit,
		  FreeSpace,
		  FoundAtBit,
		  PackID;
 dword		  NotSearchedGroups;
 BMP              Bmp(512l*8l);
 MasterBootRecord BR;



 Device&=0x0F;


 #ifdef DEBUG
  printf("\n ALLOC SEC on Dev %u Party %u...", Device, Party );
 #endif



 // create package if wanted
 switch( Flag )
 {
  case FPACK_CREAT:
		    if( CreatePackage( Device, Party, PackageID ) )
		    {
		     GetNumber=0;

		     return ERR_FS_FATAL_ERROR;
		    }

		    PackID=PackageID;

		    break;

  case FPACK_NOTHING:

		    PackID=0;

		    break;
  default:
		    PackID=PackageID;
 }



 if( LoadBoot( Device, Party, &BR ) )
 {
  #ifdef DEBUG
   printf("\n Error reading BR Dev %d Party %d in %s line %d...", Device, Party, __FILE__, __LINE__ );
  #endif

  if( Flag==FPACK_CREAT && PackID )
  {
   #ifdef DEBUG
    printf("\n Deleting package %d and freeing its ID...", PackageID );
   #endif

   CacheMan->PackIDs.FreeID( PackID );

   PackageQueueItem far *Pack=CacheMan->PackQueue.FindPackageID( PackID );

   CacheMan->PackQueue.DeletePack( Pack );
  }


  GetNumber=0;

  return ERR_FS_FATAL_ERROR;
 }



 // --- STEP ONE: USING PREVIOUS ---

 if( Previous )
 {
  #ifdef DEBUG
   printf("\n Search in group of Previous demanded..." );
  #endif

  GroupID = ( Previous - (dword)BR.BPB.ReservedSectors ) / 4097lu;

  if( GroupID > (BR.BPB.NrOfGroups-1lu) ) // group after last -> error
  {
   printf("\n Previous sector too big...");

   GetNumber=0;

   return ERR_FS_FATAL_ERROR;
  }



  if( LoadBitmap( Device, Party, GroupID, &BR, Bmp.Bmp ))
  {
   #ifdef DEBUG
    printf("\n Error loading bitmap LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
   #endif

   GetNumber=0;

   // Bmp dealocated automaticly
   return ERR_FS_BAD_GRP_SEC;
  }


  // now search the hole => somewhere in group ( for now )
  // ( later: search from previous, if full -> search group modulo 512 )
  // Advice: 1. search from Previous % 513 in bitmap
  //         2. search from beg of bitmap ( found hole if exist only in
  //            the first half because in the second is not viz. 1. )

  H=( Previous - (dword)BR.BPB.ReservedSectors ) % 4097lu;

  FirstBit=(word)H;


  if( Bmp.FindAllocSecHole( FirstBit, FoundAtBit, Number ) ) // searches and sets bits
  {
   // place allocated

   // count Logical
   Logical=(dword)FoundAtBit + 			// bit 0..
	   GroupID*4097lu + 			// jump by groups
	   (dword)BR.BPB.ReservedSectors 	// Reserved sectors in party
	   ;

   if( !(GroupID%2lu) ) // group is even => bitmap before
    Logical++;

   // use log to write result to disk ( statgrp set there )
   AllocFreeUsingLog( Device,
		      Party,
		      GroupID,
		      Logical,
		      FoundAtBit,
		      Number,
		      PackID,
		      &BR,
		      Bmp.Bmp,
		      CALLER_IS_ALLOC,
		      WRITE_TO_OKA
		    );

   #ifdef DEBUG
    printf("\n ALLOCated interval LogPartySecs < %lu, %lu >...", Logical, Logical+(dword)Number-1lu );
   #endif


   GetNumber=Number;

   return ERR_FS_NO_ERROR;
  }

  // try the rest of group ( from beginning )
  if( Bmp.FindAllocSecHole( 0u, FoundAtBit, Number ) ) // searches and sets bits
  {
   // place allocated

   // count Logical
   Logical=(dword)FoundAtBit + 			// bit 0..
	   GroupID*4097lu +			// jump by groups
	   (dword)BR.BPB.ReservedSectors	// Reserved sectors in party
	   ;

   if( !(GroupID%2lu) ) // group is even => bitmap before
    Logical++;

   // use log to write result to disk ( statgrp set there )
   AllocFreeUsingLog( Device,
		      Party,
		      GroupID,
		      Logical,
		      FoundAtBit,
		      Number,
		      PackID,
		      &BR,
		      Bmp.Bmp,
		      CALLER_IS_ALLOC,
		      WRITE_TO_OKA
		    );

   #ifdef DEBUG
    printf("\n ALLOCated interval < %lu, %lu >...", Logical, Logical+(dword)Number-1lu );
   #endif

   GetNumber=Number;

   return ERR_FS_NO_ERROR;
  }

 } // if( Previous )



 // --- STEP TWO: RANDOM AND THEN SEQUENTIALLY ---
 int   ri =rand();
 dword rdw=(dword)ri;

 GroupID=rdw % BR.BPB.NrOfGroups;

 #ifdef DEBUG
  printf("\n \# ALLOCSEC SHOT -> Grp %lu of %lu hunted...", GroupID, BR.BPB.NrOfGroups );
 #endif


 if( LoadBitmap( Device, Party, GroupID, &BR, Bmp.Bmp ))
 {
  #ifdef DEBUG
   printf("\n Error loading bitmap LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
  #endif

  GetNumber=0;

  // Bmp dealocated automaticly
  return ERR_FS_FATAL_ERROR;
 }


 // now search the hole

 NotSearchedGroups=BR.BPB.NrOfGroups-1lu; // counting not searched groups


 while( NotSearchedGroups )
 {
  // take a look to statistic

   GetGroupFreeSpace( Device, Party, GroupID, FreeSpace );

   if( FreeSpace > Number ) // it could be there
   {

    if( Bmp.FindAllocSecHole( 0u, FoundAtBit, Number ) ) // searches and sets bits
    {
     // place allocated

     // count Logical
     Logical=(dword)FoundAtBit + 		// bit 0..
	     GroupID*4097lu +			// jump by groups
	     (dword)BR.BPB.ReservedSectors      // Reserved sectors in party
	     ;

     if( !(GroupID%2lu) ) // group is even => bitmap before
      Logical++;

     // use log to write result to disk ( statgrp set there )
     AllocFreeUsingLog( Device,
			Party,
			GroupID,
			Logical,
			FoundAtBit,
			Number,
			PackID,
			&BR,
			Bmp.Bmp,
			CALLER_IS_ALLOC,
			WRITE_TO_OKA
		      );

     #ifdef DEBUG
      printf("\n ALLOCated interval < %lu, %lu >...", Logical, Logical+(dword)Number-1lu );
     #endif

     GetNumber=Number;

     return ERR_FS_NO_ERROR;
    }
   }

  // unfortunately space not found -> try next group
  #ifdef DEBUG
   printf("\n --> Not enough space in Group %lu, trying next...", GroupID );
  #endif

  NotSearchedGroups--;

  // take next group
  GroupID++;
  GroupID%=BR.BPB.NrOfGroups;

  // load its bitmap
  if( LoadBitmap( Device, Party, GroupID, &BR, Bmp.Bmp ))
  {
   #ifdef DEBUG
    printf("\n Error loading bitmap LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
   #endif

   GetNumber=0;

   // Bmp dealocated automaticly
   return ERR_FS_BAD_GRP_SEC;
  }
 }



 // SPACE NOT FOUND IN SYSTEM...
 GetNumber=0;

 // unfortunately space not found -> try next group
 #ifdef DEBUG
  printf("\n ==> NOT ENOUGH SPACE ON DISK %d PARTY %d - wanted int. lng %d too long...", Device, Party, Number );
 #endif


 // Bmp dealocated automaticly

 return ERR_FS_NO_ERROR;

}

//- CacheManFreeSector -----------------------------------------------------------------------------------

word CacheManFreeSector( byte  Device,     byte Party,
			 dword Logical,    word Number,
			 word  &PackageID, word Flag
			)
// - returns sector to system
// - logical is logical nr. of sec. in party
//
// What it does:
//  - count GroupID
//  - load bitmap
//  - null bits in bitmap
//  - consistent write of bitmap

// DODELAT - BITMAPA JE BAD => je to jedno, load bmp to zapise do sys filu
// jen nahoru hodit error?
{
 dword 		  GroupID,
		  H;
 word             FirstBit,
		  LastBit,
		  PackID;

 BMP              Bmp(512l*8l);
 MasterBootRecord BR;



 Device&=0x0F;



 #ifdef DEBUG
  printf("\n FREE SEC on Dev %u Party %u...", Device, Party );
 #endif



 // create package if wanted
 switch( Flag )
 {
  case FPACK_CREAT:
		    if( CreatePackage( Device, Party, PackageID ) )
		     return ERR_FS_FATAL_ERROR;

		    PackID=PackageID;

		    break;

  case FPACK_NOTHING:

		    PackID=0;

		    break;
  default:
		    PackID=PackageID;
 }



 if( LoadBoot( Device, Party, &BR ) )
 {
  #ifdef DEBUG
   printf("\n Error reading BR..." );
  #endif
  return ERR_FS_FATAL_ERROR;
 }



 // --- LOAD BITMAP ---

 GroupID = ( Logical - (dword)BR.BPB.ReservedSectors ) / 4097lu;

 if( GroupID > (BR.BPB.NrOfGroups-1lu) ) // group after last -> error
 {
  printf("\n GroupID too big...");
  return ERR_FS_FATAL_ERROR;
 }


 if( LoadBitmap( Device, Party, GroupID, &BR, Bmp.Bmp ))
 {
  #ifdef DEBUG
   printf("\n Error loading bitmap LogPartySec %lu, device 0x%x, party %u... ", Logical, Device, Party );
  #endif

  // Bmp dealocated automaticly
  return ERR_FS_BAD_GRP_SEC;
 }



 // --- NULL BITS ---

 H=( Logical - (dword)BR.BPB.ReservedSectors ) % 4097lu;

 FirstBit=(word)H;
 LastBit=FirstBit+Number-1l;

 Bmp.ClearInterval( (long)FirstBit, (long)LastBit );


 // --- CONSISTENT WRITE OF BITMAP ---

   // use log to write result to disk ( statgrp set there )
   AllocFreeUsingLog( Device,
		      Party,
		      GroupID,
		      Logical,
		      FirstBit,
		      Number,
		      PackID,
		      &BR,
		      Bmp.Bmp,
		      CALLER_IS_FREE,
		      WRITE_TO_OKA
		    );

 #ifdef DEBUG
  printf("\n FREE SEC interval < %lu, %lu > on Dev %u Party %u...", Logical, Logical+(dword)Number-1lu, Device, Party  );
 #endif

 // Bmp dealocated automaticly
 return ERR_FS_NO_ERROR;

}

//- CreatePackage -----------------------------------------------------------------------------------

word CreatePackage( byte Device, byte Party, word &PackageID )
{
 PackageQueueItem far *Pack;
 DataPartysItem   far *DataDev;

 #ifdef DEBUG
  printf("\n -> Creating new PACKAGE" );
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
	 printf("\n Error: Dev %u Party %u not in system => PACKAGE NOT CREATED...", Device, Party );
	#endif

	delete Pack;

	return ERR_FS_FATAL_ERROR;
       }

 Pack->SwapsTo=DataDev->Swap;

 if( !Pack->SwapsTo )
 {
  #ifdef SEE_ERROR
   printf("\n Error: no record in WhereSwaps for this party..." );
  #endif

  delete Pack;

  return ERR_FS_FATAL_ERROR;
 }

 // add package to priority queue
 CacheMan->PackQueue.Insert( Pack );


 return ERR_FS_NO_ERROR;
}


//- EOF -----------------------------------------------------------------------------------
