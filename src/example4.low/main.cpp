//-----------------------------------------------------------------------------
//
//                                 RFS
//                           Low Level Example 4
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



                        EXAMPLE 4: STRIPING A ZACHRANA DISKU


        Spusteni tohoto prikladu predpoklada, ze pred nim byl spusten
example1, ktery vytvoril zarizeni na kterych se zde bude pracovat.

        Nyni prisel cas vyzkouset striping.

        Jak jste se docetl v dokumentaci v system se stripuje pres partysny.
Vytvorime si tedy stripovaci sesnu takto:
        1. Spustte example a az se objevi modra vyzva skocte do
           SETUPu STRIPINGu ( Press ESC ).
        2. Navrh na vytvoreni sesny:
           vytvorime si sesnu se dvemi datovymi partysnami a jednou
           paritni. Zmacknete-li "h" vytasi se v setupu help. Vzdy je tam
           uvedeno pismeno, ktere mate stisknout.

           D ( pridani datove partysny ) => pridejte:

             Device:    1
             Party      1

           D ( pridani datove partysny ) => pridejte:

             Device:    2
             Party      1

           P ( pridani paritni partysny ) => pridejte:

             Device:    0
             Party      2

           S vypise Vam jake party se stripuji

           N zapne striping

           W zapisete tabulky

           Q exitne a prepocte paritu.

             POKUD NECHCETE ABY SE PARITA POCITALA A CHCETE VYPNOUT
             STRIPING STISKNETE "O" ( jako OFF )

      nyni se striping nahodi a jede. Pokud chcete vse videt, ve STRIPING.CPP
      odkomentujte DEBUG a uvidite ze se pouzivaji ty spravne funkce.



                        ZACHRANA PARTYSNY.

        Nasleduje postup jak vyzkouset, ze system umi zachranovat DATOVE
partysny ( podrobnosti dokumentace ). Postupujte takto:


        1. Mate provedenu inicializaci sesny z predchoziho prikladu.

        2. Smazte nebo presunte 0x81.hd ( jak se pise v dokumentaci lze
           zachranovat pokud vypadne prave jeden disk a to datovy,
           zachranovat parity je nesmysl ( ta se vypocte )).

        3. Spustte example.

        4. System se nahodi zjisti ze chybi jeden disk a nastartuje se
           zachranovaci utilita. V ni date "r" a specifikujete,
           kam chcete partysnu zachranit. Doporucuji Vam

           R

            Device: 2
            Party:  2

        5. Nasleduje zachrana => vse se vypisuje ( vcetne zachranenych
           udaju ( pro kontrolu ))

        6. Nyni se nahodi konfiguracni utilita, ktera Vam nabidne
           rekonfiguraci paritni sesny.

           S
            ukaze Vam jak se zachranena sesna priradila do party


        7. System se nahodi v normalnim rezimu...

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


