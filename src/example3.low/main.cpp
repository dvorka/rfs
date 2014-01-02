//-----------------------------------------------------------------------------
//
//                                 RFS
//                           Low Level Example 3
//                                Dvorka
//                                 1998
//
//-----------------------------------------------------------------------------


/*

 ----------------------------------------------------------------------------

                        VYPIS HLASEK ANEB VIDET JAK FS PRACUJE

        V kazdem *.CPP souboru je hned nazacatku #define DEBUG. Pokud tento
 define odkomentujete zacnou se vypisovat hlasky co se dela. Debug
 hlasek jsou vsude mraky. V podstate jsem program ladil pouze pomoci nich.
 Myslim ze podavaji docela slusnou informaci o tom co se deje. Nejlepsi
 je ODKOMENTOVAT DEBUG V MODULU, JEHOZ PRACE VAS ZAJIMA A VYSTUP PROGRAMU
 SI PRESMEROVAT DO SOUBORU A V KLIDU SI TO PROHLEDNOUT ( *.exe > log
 v DOSu ).

 ----------------------------------------------------------------------------



		!!!  Smazte main.obj ( jinak zustane stary ) !!!


                        EXAMPLE 3: TRANSAKCE, LOGY A RECOVERY SPODNI VRSTVY



        Spusteni tohoto prikladu predpoklada, ze pred nim byl spusten
example1, ktery vytvoril zarizeni na kterych se zde bude pracovat.

        V dokumentaci jste se docetl, jake druhy logu pouziva system
a na co jsou urceny. Tento example je urcen pro predveni oprav.
V dokumentaci si muzete precist jak se provadi recovery systemu pomoci
logu zde uvidite jak se to deje.

        Pokud chcete videt podrobnosti odkomentujte si DEBUG v RECOVERY.CPP
Hodne veci s logy se take dela v commitu a pri alokacich - CACHEMAN.CPP
takze zde odkomentujte DEBUG take.

        Zpusob jak se podivat, jak se system opravuje: odkomentujte DEBUGy,
spustte example normalne ( bez presmerovani ) a kdyz system bezi tak dejte
CTRL-Pause. Potom example spustte znovu s presmerovanym vystupem do souboru
a muzete si prohlednout co a jak se opravovalo, jak se identifikovalo to,
ze se system slozil ( dirty flagy partysen -> "RECOVERY forced" ). Podrobnosti
opet najdete v dokumentaci. Logy se ukladaji bohuzel zatim pouze vne tedy ne
do file systemu. Chcete-li videt jejich obsah, zakomentuje define UNLINK_LOGS.




        Zatim NEskakejte do SETUPu STRIPINGU instrukce prijdou
        v dalsich examplech...



*/

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

 printf("\n\n                        Reliable file system"
          "\n                                  by"
          "\n                                Dvorka"
          "\n"
          "\n                 Compiled at %s, %s\n", __DATE__, __TIME__
       );

 randomize();







 InitializeSimulation();



 FSOpenFileSystem( SWAP_ON, CACHE_ON, 100000lu );






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
  for( i=1; i<=30; i++ )
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







 // contains complete commit
 FSShutdownFileSystem();







 printf("\n Bye! \n\n");

 return 0;
}


