//-----------------------------------------------------------------------------
//
//                                 RFS
//                           Low Level Example 1
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





			EXAMPLE 1: VYTVORENI SYSTEMU


	Tento example by mel byt spusten jako prvni. Vytvori "disky"
a naformatuje je je. Pokud by jste chtel jinou konfiguraci muzete
ji ziskat timto zpusobem. V modulu INIT.CPP od radky 25 nize jsou
tabulky. Zde si muzete specifikovat disk jaky chcete ( pokud se ma
vytvorit musi byt nastaveno Presented==TRUE ). Funkce InitializeSimulation
nahazuje simulaci => vytvari "disky". Prvni veci, kterou dela je to, ze
zapisuje tabulku zarizeni na disk. Tato tabulka "disku" se zapisuje
do souboru DEVICES.LST. Tento zapis se musi provest kdyz chcete uplatnit
zmeny z INIT.CPP. Implicitne se totiz hleda tabulka na disku a ta se take
pouziva. Druhou veci, kterou funkce dela je to, ze vytvori "disky".
( tzn. podle tabulky vytvori soubory se jmeny 0x80.hd, 0x81.hd, ... 0x84.hd
a naseekuje velikost ). Prvnim parametrem funkce InitializeSimulation
se urcuje zda se ma tabulka zapsat: INIT_WRITE_TABLE / INIT_DO_NOT_WRITE_TABLE
a druhym zda se maji zarizeni znovu vytvorit INIT_CREATE_DEVICES /
INIT_DO_NOT_CREATE_DEVICES.

	Po vytvoreni disky se volanim fci z FDISK.CPP/H vytvori MBR,
vytvori se partysny a volanim fce z FORMAT.CPP/H se jednotlive party
naformatuji. Pokud chcete videt co se deje tak bud nahlednete do zdrojaku,
nebo odkoumentujte DEBUG ( viz. vyse ).

	Vytvori se tato konfigurace:

	0x80:
	 party 1: Datova partysna => RFSData, 17017 sektoru ( 8388608B )
	 party 2: Datova partysna => RFSData, 17782 sektoru ( 8388608B )
		  Tato party obsahuje vice sektoru nez party 1 a proto
		  muze byt v pozdejsich examplech pouzivana jako
		  PARITYNI party ( duvody: viz dokumentace )

	0x81:
	 party 1: Datova partysna => RFSData, velikost: 8388608B
	 party 2: Swapovaci partysna => RFSSwap, velikost: 8388608B
		  Tato party je pouzivana pro double writes
		  ( podrobnosti v dokumentaci )

	0x82:
	 party 1: Datova partysna => RFSData, velikost: 8388608B
	 party 1: Datova partysna => RFSData, velikost: 8388608B


	Disky i oblasti jsou stejne z duvodu ze STRIPING funguje
jen nad oblastmi o stejne velikosti ( podrobnosti se doctete v dokumentaci )

	 -------------------------------------------------------
	 -                           			       -
	 - Celkem "disky" 0x80, 0x81, a 0x82 zaberou 53.5 MB   -
	 -                           			       -
	 -------------------------------------------------------


	Zatim NEskakejte do SETUPu STRIPINGU instrukce prijdou
	v dalsich examplech...

*/



#include "fdisk.h"
#include "format.h"
#include "fstypes.h"
#include "init.h"



extern unsigned _stklen = 50000u;



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







  InitializeSimulation( INIT_WRITE_TABLE, INIT_CREATE_DEVICES );   // create everything new

  FSCreatMasterBoot( 0x80, 512, 1 );
  FSCreatMasterBoot( 0x81, 512, 1 );
  FSCreatMasterBoot( 0x82, 512, 1 );

  // data stripe
  FSCreatPartition( 0x80, 1, 0, 500, ACTIVE_PARTITION );
  // parity stripe
  FSCreatPartition( 0x80, 2, 501, 1023, NONACTIVE_PARTITION );
  // data stripe
  FSCreatPartition( 0x81, 1, 0, 500, NONACTIVE_PARTITION );
  // swap party
  FSCreatPartition( 0x81, 2, 501, 1023, NONACTIVE_PARTITION );
  // data stripe
  FSCreatPartition( 0x82, 1, 0, 500, NONACTIVE_PARTITION );
  // data stripe
  FSCreatPartition( 0x82, 2, 501, 1001, NONACTIVE_PARTITION );

  // format parties
  FSFormat( 0x80, 1, RFS_DATA );
  FSFormat( 0x80, 2, RFS_DATA  );
  FSFormat( 0x81, 1, RFS_DATA );
  FSFormat( 0x81, 2, RFS_SWAP );
  FSFormat( 0x82, 1, RFS_DATA );
  FSFormat( 0x82, 2, RFS_DATA );



 FSOpenFileSystem( SWAP_ON, CACHE_ON, 10000lu );


 // nothing to do...


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


