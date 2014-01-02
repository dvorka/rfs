#include "format.h"

#define DEBUG


//- FSFormat -----------------------------------------------------------------------------------

word FSFormat( byte Device, byte PartyID, byte Type )
/*
  - if DataParty==FALSE => swap party is created

  Format:
    - check BR
    - check size ( minimal/maximal size for file system )
    - create system info file in mem
      - nr of groups on volume
      - nr of sectors on volume
      - nr of sectors in last group ( not whole )
      - striping table
    - create groups
    - fill groups bitmaps
    - create and fill group info system file in mem
    - found bad blocks
    - write report to bad blocks system file in mem

    - create ROOT directory
    - create dir ->file system files.system<-
      - create subdir ->transactions.system<- for transactions
	- create file ->logfiles list.system<- list of logfiles
      - create file ->STRIPING TABLE.system<- ( flush it to disk )
      - create file ->GROUP info.system<- ( flush it to disk )
      - create file ->BAD SECs.system<- ( flush it to disk )
      - create file ->BAD GROUPSs.system<- ( flush it to disk ) has bad bitmap sectors
*/

{
 MasterBootRecord BR;
 dword		  GroupStatisticSectors;

 // load partition boot record
 #ifdef DEBUG
  printf("\n\n Party %d on device 0x%x will be FORMATED...", PartyID, Device );
  printf("\n Loading BR...");
 #endif

 if( LoadBoot( Device, PartyID, &BR ) )
 {
  #ifdef DEBUG
   printf("\n Error reading BR..." );
  #endif
  return ERR_FS_FATAL_ERROR;
 }
 else
 {
  #ifdef DEBUG
   printf(" OK..." );
  #endif
 }

 // fill format specific structures ( use them for something useful... )
 BR.BPB.CATCopies	        =0;	// filled with format - not used
 BR.BPB.RootRecords	        =0;	// filled with format - not used
 BR.BPB.SizeOfCat		=0; 	// filled with format - not used

 BR.BPB.RootFNODEAddr		=0; 	// filled with format
 // BR.BPB.NrOfGroups		=0; 	// init in call CreateNrOfGroups


 BR.BPB.DirtyFlag		=PARTY_IS_CLEAN;


 // fill magic word

 // far copy
 byte i,
      MagicData[]={RFS_MAGIC_WORD_DATA_PARTY},
      MagicSwap[]={RFS_MAGIC_WORD_SWAP_PARTY};

 if( Type==RFS_DATA )
 {
  for(i=0; i<=7; i++ ) BR.OSID.ID[i]=MagicData[i];
 }
 else
 {
  for(i=0; i<=7; i++ ) BR.OSID.ID[i]=MagicSwap[i];
 }


 // create groups

 TheSecondBoot SecondBoot;	// statistic of party => the second boot

 SecondBoot.MagicBegin= SecondBoot.MagicEnd  = 0xCDAB;
 // PartyStat.FreeSecs initialized in CreateNrOfGroups
 SecondBoot.Label[0]  =0;
 strcpy( SecondBoot.Label, "Party label not used!" );

 // - BR.BPB.HiddenSectors == first logical sector of party
 // - use NrOfGroups to change nr. of reserved sectors
 //   ( The second boot + group statistic added here )

 // change hidden sectors
 BR.BPB.ReservedSectors += 1u;	// reserving sector for the second boot


 CreateNrOfGroups(
		   Device,
		   (dword)((dword)BR.BPB.HiddenSectors+(dword)BR.BPB.ReservedSectors),
		   (dword)((dword)BR.BPB.TotalSectors-(dword)BR.BPB.ReservedSectors),
		   BR.BPB.NrOfGroups,
		   SecondBoot.FreeSecs,	 // free space on party in secs
		   GroupStatisticSectors // nr of secs with group statistic
		 );


 SecondBoot.StatSectors=GroupStatisticSectors;

 // reserving sectors for group statistic
 BR.BPB.ReservedSectors += (word)GroupStatisticSectors;
				// nr. of sectors used for group statistic,
				// each groups needs two bytes:
				// in this is nr of free sectors in group

 // copy BPB
 movmem((const void *)&(BR.BPB), (void *)&(SecondBoot.BPB), sizeof(BIOSParameterBlock) );


 // Write The Second Boot

 // HiddenSectors == first logical sector of party because of logic. address
 if( WriteLogicalSector( Device,
			 BR.BPB.HiddenSectors+1lu,// stat is 2. sec in party
			 MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) ) )
		      )
 {
  #ifdef SEE_ERROR
   printf("\n Error writing The Second Boot..." );
  #endif

 }


 #ifdef DEBUG
  printf("\n Party STATISTIC saved:"
	 "\n  Free     : %lu sectors"
	 "\n  it is    : %lu bytes"
	 "\n  Label    : %s"
	 ,
	 SecondBoot.FreeSecs,
	 SecondBoot.FreeSecs*512lu,
	 SecondBoot.Label );
 #endif


 if( SaveBoot( Device, PartyID, &BR ) )
 {
  #ifdef DEBUG
   printf(" error writing MBR..." );
  #endif
  return ERR_FS_FATAL_ERROR;
 }

 Device&=0x0F;

 // Creating of root dir here...
 CreateRootDir( Device, PartyID, BR.BPB.RootFNODEAddr );

 // copy BPB
 movmem((const void *)&(BR.BPB), (void *)&(SecondBoot.BPB), sizeof(BIOSParameterBlock) );


 if( SaveBoot( Device, PartyID, &BR ) )
 {
  #ifdef DEBUG
   printf(" error writing MBR..." );
  #endif
  return ERR_FS_FATAL_ERROR;
 }



 // Write The Second Boot
 if( WriteLogicalSector( Device,
			 BR.BPB.HiddenSectors+1lu,// stat is 2. sec in party
			 MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) ) )
		      )
 {
  #ifdef SEE_ERROR
   printf("\n Error writing The Second Boot..." );
  #endif
 }



 #ifdef DEBUG
  printf("\n Format results:" );
  LoadBoot( Device, PartyID, &BR );
  printf("\n Format OK..." );
 #endif

 return ERR_FS_NO_ERROR;
}


//- EOF -----------------------------------------------------------------------------------



