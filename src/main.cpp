//-----------------------------------------------------------------------------
//
//                            ELFs - file system
//                                Dvorka
//                                 1998
//
//-----------------------------------------------------------------------------

// Run for example it this way:
//                      D:\ fs > output.txt


// #define CREATE_EVERYTHING_NEW

// ACTIONS TO DO
 #define CACHE_TEST
//#define CACHE_EXAMPLE
//#define STRIP_EXAMPLE

#include "cacheman.h"
#include "cachesup.h"
#include "group.h"
#include "fdisk.h"
#include "format.h"
#include "fstypes.h"
#include "hardware.h"
#include "init.h"
#include "recovery.h"


// change size of stack
extern unsigned _stklen = 50000u;


extern CacheManStruct     far *CacheMan;

//------------------------------------------------------------------------------------

int main( void )
{
 dword FarFreeMemoryAtStart=(unsigned long)farcoreleft();

 printf("\n\n                        Reliable file system"
          "\n                                  by"
          "\n                            Jarda & Dvorka"
          "\n"
          "\n                 Compiled at %s, %s\n", __DATE__, __TIME__
       );
 printf(  "                  Starting with far heap: %luB \n", FarFreeMemoryAtStart );

 randomize();







 //- work with file system -

 #ifdef CREATE_EVERYTHING_NEW
  InitializeSimulation( TRUE, TRUE );   // create everything new

  FSCreatMasterBoot( 0x80, 512, 1 );
  FSCreatMasterBoot( 0x81, 512, 1 );
  FSCreatMasterBoot( 0x82, 512, 1 );

  // data stripe
  FSCreatPartition( 0x80, 1, 0, 500, ACTIVE_PARTITION  );
  // parity stripe
  FSCreatPartition( 0x80, 2, 501, 1023, NONACTIVE_PARTITION  );
  // data stripe
  FSCreatPartition( 0x81, 1, 0, 500, ACTIVE_PARTITION  );
  // swap party
  FSCreatPartition( 0x81, 2, 501, 1023, NONACTIVE_PARTITION  );
  // data stripe
  FSCreatPartition( 0x82, 1, 0, 500, ACTIVE_PARTITION  );
  // swap party
  FSCreatPartition( 0x82, 2, 501, 1023, NONACTIVE_PARTITION  );

  // format parties
  FSFormat( 0x80, 1, RFS_DATA );
  FSFormat( 0x80, 2, RFS_DATA  );
  FSFormat( 0x81, 1, RFS_DATA );
  FSFormat( 0x81, 2, RFS_SWAP );
  FSFormat( 0x82, 1, RFS_DATA );
  FSFormat( 0x82, 2, RFS_SWAP );

 #else
  InitializeSimulation();               // only init handles,...
 #endif



 // init file system ( inside init of cache )
 FSOpenFileSystem( SWAP_ON, CACHE_ON, 10000lu );







  #ifdef CACHE_TEST;

  word     PackageID=0,
           GetNumber=0,
           i;

  dword    Logical,
           Free;

  void far *Buffer=farmalloc(10*512); if( !Buffer ) exit(0);



  // allocates && creates package ( 0x81 or 1 - it has same effect )
  CacheManAllocateSector( 0x81, 1, 0, 1, Logical, GetNumber, PackageID, FPACK_CREAT );

  word j;
  word Number;


  // PackageID set in previous function
  for( i=1; i<=5; i++ )
  {
   // allocate allocates < logical, logical+6 >
   CacheManAllocateSector( 1, 1, 0lu, 7, Logical, GetNumber, PackageID, FPACK_ADD );


   printf("\n Get number: %u", GetNumber );

   // init buffer for save
   for( j=0; j<10*512; j++ ) ((byte far * )Buffer)[j]=0xCC;

   if( random(5) ) // probably add
    CacheManSaveSector( 1, 1, Logical, 7, PackageID, FPACK_ADD, Buffer );
   else
    CacheManSaveSector( 1, 1, Logical, 7, PackageID, FPACK_CREAT, Buffer );

   // load what's written from cache ( something )
   //  Number=(word)random(7) + 1u;
   Number=7;
   CacheManLoadSector( 1, 1, Logical, Number, PackageID, FPACK_ADD, Buffer );


   if( !random(10) ) // probably not commit
   {
    if( random(2) )
     CacheManCommitPackage( PackageID, FPACK_NOTHING ); // commit
    else
    {
     CacheManCommitPackage( PackageID, FPACK_DELETE );  // commit && del pack

     // create new PackID because in alloc and save used!
     CacheManAllocateSector( 1, 1, 0lu, 7, Logical, GetNumber, PackageID, FPACK_CREAT );

     printf("\n Get number: %u", GetNumber );
    }
   }

   // print free space on device
   GetPartyFreeSpace( 0x81, 1, Free );
   GetPartyFreeSpace( 0x81, 2, Free );
   printf("\n\n ----->");
  }



  farfree(Buffer);

  // UndoAlfLog( 0x81, 1, "TEST.ALF" );

  // RecoverUsingCml( "TEST.CML" );

  // RollbackOkaLog( 1, 1, "11TEST.OKA" );

  // RecoverParty( 1, 1 );

 #endif








  #ifdef CACHE_EXAMPLE;

  int      Handle;
  word     PackageID,
           GetNumber=0,
           i,
           j,
           bytes;

  dword    Previous=0,
	   Logical1,
	   Logical2,
	   Logical;

  #define BUFFER_SIZE 512

  void far *Buffer=farmalloc(BUFFER_SIZE);
  if( !Buffer ) { printf("All1 Error..");exit(0); }

  void far *BufferOut=farmalloc(BUFFER_SIZE);
  if( !BufferOut ) { printf("All2 Error..");exit(0); }


 // now read some nice data to buffer
 if ((Handle = open("input.txt", O_CREAT  | O_RDWR | O_BINARY,
                                   S_IWRITE | S_IREAD           )) == -1)
 { printf("Error.."); return ERR_FS_FATAL_ERROR; }

 if( _dos_read( Handle, Buffer, BUFFER_SIZE, &bytes) != 0 )
 { printf("Error.."); return ERR_FS_FATAL_ERROR; }

 close(Handle);

 // --- begin work ------------------------------------------------
//*
  for(i=0; i<512; i++ ) ((byte far *)Buffer)[i]=0xAA;
  CacheManAllocateSector( 1, 1, 0, 1, Logical1,
			  GetNumber, PackageID, FPACK_CREAT );
  CacheManSaveSector( 1, 1, Logical1, 1u, PackageID, FPACK_ADD, Buffer  );

  for(i=0; i<512; i++ ) ((byte far *)Buffer)[i]=0xBB;
  CacheManAllocateSector( 1, 1, 0, 1, Logical2,
			  GetNumber, PackageID, FPACK_ADD );
  CacheManSaveSector( 1, 1, Logical2, 1u, PackageID, FPACK_ADD, Buffer  );
//*/

  CacheManCompleteCommit( FPACK_DELETE );

//*
 for(i=0; i<512; i++ ) ((byte far *)BufferOut)[i]=0xEE;

 CacheManLoadSector( 1, 1, Logical1, 1u, PackageID, FPACK_CREAT, BufferOut  );

 if ((Handle = open("AA", O_CREAT  | O_RDWR | O_BINARY,
				   S_IWRITE | S_IREAD           )) == -1)
 { printf("Error.."); return ERR_FS_FATAL_ERROR; }

 if( _dos_write( Handle, BufferOut, BUFFER_SIZE, &bytes) != 0 )
 { printf("Error.."); return ERR_FS_FATAL_ERROR; }

 close(Handle);

 // ---

 for(i=0; i<512; i++ ) ((byte far *)BufferOut)[i]=0xEE;

 CacheManLoadSector( 1, 1, Logical2, 1u, PackageID, FPACK_ADD, BufferOut  );

 if ((Handle = open("BB", O_CREAT  | O_RDWR | O_BINARY,
				   S_IWRITE | S_IREAD           )) == -1)
 { printf("Error.."); return ERR_FS_FATAL_ERROR; }

 if( _dos_write( Handle, BufferOut, BUFFER_SIZE, &bytes) != 0 )
 { printf("Error.."); return ERR_FS_FATAL_ERROR; }

 close(Handle);
//*/

 // --- end work ------------------------------------------------


 // save buffer content ------------------------------------------------
 if ((Handle = open("output2.txt", O_CREAT  | O_RDWR | O_BINARY,
                                   S_IWRITE | S_IREAD           )) == -1)
 { printf("Error.."); return ERR_FS_FATAL_ERROR; }

 if( _dos_write( Handle, BufferOut, BUFFER_SIZE, &bytes) != 0 )
 { printf("Error.."); return ERR_FS_FATAL_ERROR; }

 close(Handle);






 farfree(Buffer);

 farfree(BufferOut);


 #endif






 #ifdef STRIP_EXAMPLE

  #define BUFFER_SIZE 512

  #define SECTOR      31

  byte far *Buffer=(byte far *)farmalloc(BUFFER_SIZE);
  if( !Buffer ) { printf("All1 Error..");exit(0); }



  printf("\n\n\n Test: ");

  ReadLogicalSector( 1, SECTOR, Buffer, 1 );

  Buffer[0]=0xAB;
  CLASSICWriteLogicalSector( 1, SECTOR, Buffer, 1 );

  ReadLogicalSector( 1, SECTOR, Buffer, 1 );
  ReadLogicalSector( 1, SECTOR, Buffer, 1 );

  farfree(Buffer);

 #endif



































 FSShutdownFileSystem(); // contains complete commit

 //-------------------------







 printf("\n\n Memory statistic:");

 if( FarFreeMemoryAtStart!=((unsigned long) farcoreleft()))
 {
  printf("\n  Deallocation ERROR:");
  Beep(); Beep(); Beep();
 }
 else
  Beep();

 printf(
        "\n  Far free at start: %lu"
        "\n  and now          : %lu"
        "\n\n"
        ,
        FarFreeMemoryAtStart,
        ((unsigned long) farcoreleft())
       );

 printf("\n Bye! \n\n");

 return 0;
}


