//-----------------------------------------------------------------------------
//
//                                 RFS
//                           Low Level Example 2
//				  Dvorka
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


			EXAMPLE 2A: FUNGOVANI CACHEMANA I


	Spusteni tohoto prikladu predpoklada, ze pred nim byl spusten
example1, ktery vytvoril zarizeni na kterych se zde bude pracovat.

	V dokumentaci jste se docetl jakou fci ma CACHEMAN a jak moc je
dulezity. Tento example ilustruje jeho praci. Za normalniho behu by
bylo hrozne nechat si vypisovat debug hlasky protoze by jich byly mraky
( nicmene pokud chcete, muzete )

	Prace s buffery v tomto prikladu nedava smysu, tento priklad
je zde proto, aby ilustroval funkce cachemana. Proto si ODKOMENTUJTE
#define DEBUG V CACHEMAN.CPP. a vystup si presmerujte. Uvidite, jak
se v cache hledaji prekrivajici intervaly, jak se commituji packages,
jak se cte a zapisuje. Podrobnosti opet ve zdrojacich, ale predevsim
v DOKUMENTACI.



			EXAMPLE 2B: FUNGOVANI CACHEMANA II

	Dalsi priklad na cachemana, ve kterem je videt jak se hleda
castecna shoda intervalu v cache. Bez odkomentovani DEBUGu v CACHEMAN.CPP
a presmerovani vystupu do filu nema priklad smysl.

	Pokud chcete videt jeste vice podrobnosti, odkomentujte DEBUG
v CACHESUP.CPP, kde jsou funkce pro hledani volneho cisla package,
volnych mist v cachedescriptorech a spousta dalsich veci.



	Zatim NEskakejte do SETUPu STRIPINGU instrukce prijdou
	v dalsich examplech...



*/


// #define EXAMPLE2A
// else is done EXAMPLE2B


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
          "\n                                Dvorka"
          "\n"
          "\n                 Compiled at %s, %s\n", __DATE__, __TIME__
       );
 printf(  "                  Starting with far heap: %luB \n", FarFreeMemoryAtStart );

 randomize();







 InitializeSimulation();



 FSOpenFileSystem( SWAP_ON, CACHE_ON, 100000lu );






 #ifdef EXAMPLE2A

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
  for( i=1; i<=20; i++ )
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


 #else


  word     PackageID,
	   GetNumber=0,
	   i;

  dword    Previous=0,
	   Logical;

  void far *Buffer=farmalloc(10*512); if( !Buffer ) exit(0);



   for( i=0; i<10*512; i++ ) ((byte far * )Buffer)[i]=0xCC;

   CacheManAllocateSector( 1, 1, Previous, 9, Logical, GetNumber, PackageID , FPACK_NOTHING );

   for( i=0; i<10*512; i++ ) ((byte far * )Buffer)[i]=0xCC;

   CacheManSaveSector( 1, 1, Logical, 3, PackageID, FPACK_CREAT, Buffer );

   CacheManLoadSector( 1, 1, Logical+6, 3, PackageID, FPACK_ADD, Buffer );

   CacheManLoadSector( 1, 1, Logical+6, 2, PackageID, FPACK_ADD, Buffer );

   CacheManLoadSector( 1, 1, Logical, 9, PackageID, FPACK_ADD, Buffer );

   for( i=0; i<10*512; i++ ) ((byte far * )Buffer)[i]=0xCC;

   // make it dirty
   CacheManSaveSector( 1, 1, Logical+6, 3, PackageID, FPACK_ADD, Buffer );

   CacheManCommitPackage( PackageID, FPACK_NOTHING );


  farfree(Buffer);


 #endif







 // contains complete commit
 FSShutdownFileSystem();







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


