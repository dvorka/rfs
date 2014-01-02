#include "fdisk.h"

//#define DEBUG
//#define SEE_ERROR

// DEBUG defined lower -> comment it there

extern int   		HDHandle[5];


//- LoadMasterBoot -----------------------------------------------------------------------------------

word LoadMasterBoot( byte Device, MasterBootRecord *pMBR )
// MBR is duplicated in whole track => rescue utility can try
// to read next sector in track. Here not implemented
// ( only duplication and save is done )
{
 Device&=0x0F;

 word ErrLevel= ReadLogicalSector( Device,
				   0,
				   MK_FP( FP_SEG(pMBR), FP_OFF(pMBR) )
				 );

 #ifdef DEBUG
  printf(
	 "\n Loaded MBR content:"
	 "\n  SectorSize       %u"
	 "\n  ClusterSectors   %u"
	 "\n  ReservedSectors  %u"
	 "\n  CATCopies        %u"
	 "\n  RootRecords      %u"
	 "\n  MediaDescr       0x%x"
	 "\n  SizeOfCat        %u"
	 "\n  TrackSectors     %u"
	 "\n  Surfaces         %u"
	 "\n  HiddenSectors    %lu"
	 "\n  TotalSectors     %lu"
	 ,
	 pMBR->BPB.SectorSize,
	 pMBR->BPB.ClusterSectors,
	 pMBR->BPB.ReservedSectors,
	 pMBR->BPB.CATCopies,
	 pMBR->BPB.RootRecords,
	 pMBR->BPB.MediaDescr,
	 pMBR->BPB.SizeOfCat,
	 pMBR->BPB.TrackSectors,
	 pMBR->BPB.Surfaces,
	 pMBR->BPB.HiddenSectors,
	 pMBR->BPB.TotalSectors
	);
 #endif

 // check if master boot is valid
 if( pMBR->PartyTable.ID != 0xAA55 )
 {
  #ifdef SEE_ERROR
   printf("\n Error: LoadMBR - magic 0xAA55 not found => not valid => not loaded... ");
//   getch();
  #endif

  return ERR_FS_FATAL_ERROR;
 }

 return ErrLevel;
}

//- SaveMasterBoot -----------------------------------------------------------------------------------

word SaveMasterBoot( byte Device, MasterBootRecord *pMBR )
// it writes whole the first track - MBR should is duplicated
{
 Device&=0x0F;

 // before save check if master boot is valid
 if( pMBR->PartyTable.ID != 0xAA55 )
 {
  #ifdef SEE_ERROR
   printf("\n Error: SaveMBR - magic 0xAA55 not found => not valid => not saved... ");
//   getch();
  #endif

  return ERR_FS_FATAL_ERROR;
 }

 dword i;

 for( i=0; i<(dword)(pMBR->BPB.TrackSectors); i++ )
  WriteLogicalSector( Device,
		      i,
		      MK_FP( FP_SEG(pMBR), FP_OFF(pMBR) )
		    );

 return ERR_FS_NO_ERROR;
}

//- LoadBoot -----------------------------------------------------------------------------------

word LoadBoot( byte Device, byte PartyID, MasterBootRecord *pBR )
// in futute it should read all track ( MBR should be duplicated )
{
 MasterBootRecord MBR;

 if( LoadMasterBoot( Device, &MBR ) )
 {
  return ERR_FS_FATAL_ERROR;
 }


 // log. nr. of sector with BR: MBR.PartyTable.Party[PartyID].FirstSectorOff

 // Record for this party not used
 if( MBR.PartyTable.Party[PartyID-1].SectorNum == 0 )
  return ERR_FS_NOT_USED;

 if( ReadLogicalSector( Device,
			MBR.PartyTable.Party[PartyID-1].FirstSectorOff,
			MK_FP( FP_SEG(pBR), FP_OFF(pBR) ) )
		      )
  return ERR_FS_FATAL_ERROR;



 #ifdef DEBUG
  printf(
	 "\n Loaded BR content:"
	 "\n  OS ID            %s"
	 "\n  DirtyFlag        %u"
	 "\n  SectorSize       %u"
	 "\n  ClusterSectors   %u"
	 "\n  ReservedSectors  %u"
	 "\n  CATCopies        %u"
	 "\n  RootRecords      %u"
	 "\n  MediaDescr       0x%x"
	 "\n  SizeOfCat        %u"
	 "\n  TrackSectors     %u"
	 "\n  Surfaces         %u"
	 "\n  HiddenSectors    %lu"
	 "\n  TotalSectors     %lu"
	 "\n  RootFNODEAddr    %lu"
	 "\n  NrOfGroups       %lu"
	 ,
	 pBR->OSID.ID,
	 pBR->BPB.DirtyFlag,
	 pBR->BPB.SectorSize,
	 pBR->BPB.ClusterSectors,
	 pBR->BPB.ReservedSectors,
	 pBR->BPB.CATCopies,
	 pBR->BPB.RootRecords,
	 pBR->BPB.MediaDescr,
	 pBR->BPB.SizeOfCat,
	 pBR->BPB.TrackSectors,
	 pBR->BPB.Surfaces,
	 pBR->BPB.HiddenSectors,
	 pBR->BPB.TotalSectors,
	 pBR->BPB.RootFNODEAddr,
	 pBR->BPB.NrOfGroups
	);
 #endif

 // check if boot is valid
 if( pBR->PartyTable.ID != 0xAA55 )
 {
  #ifdef SEE_ERROR
   printf("\n Error: LoadBoot - magic 0xAA55 not found");
  #endif

  // recovery using The Second Boot Record
  TheSecondBoot    SecondBoot;

  // read second boot
  ReadLogicalSector( Device,
		     MBR.PartyTable.Party[PartyID-1].FirstSectorOff+1lu,
		     MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) )
		   );
  // copy BPB
  movmem( (const void *)&(SecondBoot.BPB),
	  (void *)&(pBR->BPB),
	  sizeof(BIOSParameterBlock)
	);

  // copy OSID
  movmem( (const void *)&(SecondBoot.OSID),
	  (void *)&(pBR->OSID),
	  sizeof(MBROSID)
	);

  // put magic
  pBR->PartyTable.ID = 0xAA55;


  SaveBoot( Device, PartyID, pBR );

  #ifdef SEE_ERROR
   printf(" => recovered... ");
  #endif
 }

 return ERR_FS_NO_ERROR;
}

//- SaveBoot -----------------------------------------------------------------------------------

word SaveBoot( byte Device, byte PartyID, MasterBootRecord *pBR )
// in futute it should read all track ( MBR should be duplicated )
{
 MasterBootRecord MBR;

 if( LoadMasterBoot( Device, &MBR ) )
 {
  return ERR_FS_FATAL_ERROR;
 }


 // before save check if boot is valid
 if( pBR->PartyTable.ID != 0xAA55 )
 {
  #ifdef SEE_ERROR
   printf("\n Error: SaveBoot - magic 0xAA55 not found");
  #endif

  // recovery using The Second Boot Record
  TheSecondBoot    SecondBoot;

  // read second boot
  ReadLogicalSector( Device,
		     MBR.PartyTable.Party[PartyID-1].FirstSectorOff+1lu,
		     MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) )
		   );
  // copy BPB
  movmem( (const void *)&(SecondBoot.BPB),
	  (void *)&(pBR->BPB),
	  sizeof(BIOSParameterBlock)
	);

  // copy OSID
  movmem( (const void *)&(SecondBoot.OSID),
	  (void *)&(pBR->OSID),
	  sizeof(MBROSID)
	);

  // put magic
  pBR->PartyTable.ID = 0xAA55;


  SaveBoot( Device, PartyID, pBR );

  #ifdef SEE_ERROR
   printf(" => recovered... ");
  #endif
 }


 // log. nr. of sector with BR: MBR.PartyTable.Party[PartyID-1].FirstSectorOff

 if( WriteLogicalSector( Device,
			 MBR.PartyTable.Party[PartyID-1].FirstSectorOff,
			 MK_FP( FP_SEG(pBR), FP_OFF(pBR) ) )
		       )
  return ERR_FS_FATAL_ERROR;

 return ERR_FS_NO_ERROR;
}

// #define DEBUG

//- GetPartyFreeSpace -----------------------------------------------------------------------------------

word GetPartyFreeSpace( byte Device, byte Party, dword &FreeSpaceSec )
// - free space is in sectors
{
 MasterBootRecord MBR;
 TheSecondBoot    SecondBoot;

 // read MBR
 if( LoadMasterBoot( Device, &MBR ) )
 {
  return ERR_FS_FATAL_ERROR;
 }


 // stat sector is second on party

 // read second boot
 if( ReadLogicalSector( Device,
			MBR.PartyTable.Party[Party-1].FirstSectorOff+1lu,
			MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) ) )
		      )
  return ERR_FS_FATAL_ERROR;

 // get free space in sectors
 FreeSpaceSec=SecondBoot.FreeSecs;

 #ifdef DEBUG
  printf("\n Free space on Dev %u Party %u is : %luB...", Device&0x0F, Party, FreeSpaceSec*512lu );
 #endif

 return ERR_FS_NO_ERROR;
}

//- SetPartyFreeSpace -----------------------------------------------------------------------------------

word SetPartyFreeSpace( byte Device, byte Party, dword FreeSpaceSec )
// - free space is in sectors
{
 MasterBootRecord MBR;
 TheSecondBoot    SecondBoot;

 // read MBR
 if( LoadMasterBoot( Device, &MBR ) )
 {
  return ERR_FS_FATAL_ERROR;
 }


 // stat sector is second on party

 // read second boot
 if( ReadLogicalSector( Device,
			MBR.PartyTable.Party[Party-1].FirstSectorOff+1lu,
			MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) ) )
		      )
  return ERR_FS_FATAL_ERROR;

 // change free space
 SecondBoot.FreeSecs=FreeSpaceSec;

 // write second boot
 if( WriteLogicalSector( Device,
			 MBR.PartyTable.Party[Party-1].FirstSectorOff+1lu,
			 MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) ) )
		       )
  return ERR_FS_FATAL_ERROR;

 #ifdef DEBUG
  printf("\n Free space on Dev %u Party %u is : %luB...", Device&0x0F, Party, FreeSpaceSec*512lu );
 #endif

 return ERR_FS_NO_ERROR;
}

//- GetPartyLabel -----------------------------------------------------------------------------------

word GetPartyLabel( byte Device, byte Party, char *Label )
// - space for string is allocated by user
{
 MasterBootRecord MBR;
 TheSecondBoot    SecondBoot;

 // read MBR
 if( LoadMasterBoot( Device, &MBR ) )
 {
  return ERR_FS_FATAL_ERROR;
 }


 // stat sector is second on party

 // read second boot
 if( ReadLogicalSector( Device,
			MBR.PartyTable.Party[Party-1].FirstSectorOff+1lu,
			MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) ) )
		      )
  return ERR_FS_FATAL_ERROR;

 // get label
 Label[0]=0;
 strcpy( Label, SecondBoot.Label );

 return ERR_FS_NO_ERROR;
}

//- SetPartyLabel -----------------------------------------------------------------------------------

word SetPartyLabel( byte Device, byte Party, char *Label )
// - space for string allocated by caller
{
 MasterBootRecord MBR;
 TheSecondBoot    SecondBoot;

 // read MBR
 if( LoadMasterBoot( Device, &MBR ) )
 {
  return ERR_FS_FATAL_ERROR;
 }


 // stat sector is second on party

 // read second boot
 if( ReadLogicalSector( Device,
			MBR.PartyTable.Party[Party-1].FirstSectorOff+1lu,
			MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) ) )
		      )
  return ERR_FS_FATAL_ERROR;

 // change free space
 SecondBoot.Label[0]=0;
 strcpy( SecondBoot.Label, Label );

 // write second boot
 if( WriteLogicalSector( Device,
			 MBR.PartyTable.Party[Party-1].FirstSectorOff+1lu,
			 MK_FP( FP_SEG(&SecondBoot), FP_OFF(&SecondBoot) ) )
		       )
  return ERR_FS_FATAL_ERROR;


 return ERR_FS_NO_ERROR;
}

//- GetGroupFreeSpace -----------------------------------------------------------------------------------

word GetGroupFreeSpace( byte Device, byte Party, dword GroupID,
			word &FreeSpaceSec )
// - free space is in sectors
{
 MasterBootRecord MBR;

 word     far *Buffer = (word far *)farmalloc(512lu);
 if( !Buffer ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

 // read MBR
 if( LoadMasterBoot( Device, &MBR ) )
 {
  farfree( Buffer );
  return ERR_FS_FATAL_ERROR;
 }

 // stat sector is second on party
 dword GroupSec    = GroupID/512lu,    // 512 records in one sector
       GroupSecOff = GroupID%512lu;

       GroupSec   += MBR.PartyTable.Party[Party-1].FirstSectorOff+2lu;
		     // 1 boot record + 1 second boot
		     // => now points to first grp stat sector
		     // offset in this statistic already counted

 // read group stat sector boot
 if( ReadLogicalSector( Device, GroupSec, Buffer ) )
 {
  farfree( Buffer );
  return ERR_FS_FATAL_ERROR;
 }

 // get free space in sectors
 FreeSpaceSec=Buffer[GroupSecOff];

 #ifdef DEBUG
  printf("\n Free sectors in group %lu is %u...", GroupID, FreeSpaceSec );
 #endif


 farfree( Buffer );
 return ERR_FS_NO_ERROR;
}

//- SetGroupFreeSpace -----------------------------------------------------------------------------------

word SetGroupFreeSpace( byte Device, byte Party, dword GroupID,
			word FreeSpaceSec )
// - free space is in sectors
{
 MasterBootRecord MBR;

 word     far *Buffer = (word far *)farmalloc(512lu);
 if( !Buffer ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

 // read MBR
 if( LoadMasterBoot( Device, &MBR ) )
 {
  farfree( Buffer );
  return ERR_FS_FATAL_ERROR;
 }

 // stat sector is second on party
 dword GroupSec    = GroupID/512lu,    // 512 records in one sector
       GroupSecOff = GroupID%512lu;

       GroupSec   += MBR.PartyTable.Party[Party-1].FirstSectorOff+2lu;
		     // 1 boot record + 1 second boot
		     // => now points to first grp stat sector
		     // offset in this statistic already counted

 // read group stat sector boot
 if( ReadLogicalSector( Device, GroupSec, Buffer ) )
 {
  farfree( Buffer );
  return ERR_FS_FATAL_ERROR;
 }

 // get free space in sectors
 Buffer[GroupSecOff]=FreeSpaceSec;

 #ifdef DEBUG
  printf("\n New free sectors in group %lu is %u...", GroupID, FreeSpaceSec );
 #endif

 // write new values
 if( WriteLogicalSector( Device, GroupSec, Buffer ) )
 {
  farfree( Buffer );
  return ERR_FS_FATAL_ERROR;
 }


 farfree( Buffer );
 return ERR_FS_NO_ERROR;
}

//- FSCreatMasterBoot -----------------------------------------------------------------------------------

word FSCreatMasterBoot( byte Device, word SectorSize, byte ClusterSectors )
{
 MasterBootRecord MBR;
 word             Cylinders,
		  TrackSectors,
		  Surfaces;

 #ifdef DEBUG
  printf("\n\n Creating MBR of device 0x%x...", Device );
 #endif

 // fill MASTER BOOT RECORD
 if( GetHardParameters( Device, Cylinders, TrackSectors, Surfaces ) )
 {
  #ifdef DEBUG
   printf("\n Error: cann't read DEVICES.LST..." );
  #endif
  return ERR_FS_FATAL_ERROR;
 }

 MBR.BPB.SectorSize		=SectorSize;
 MBR.BPB.ClusterSectors		=ClusterSectors;
 MBR.BPB.ReservedSectors        =TrackSectors;	// MBR reserves one track!
 MBR.BPB.CATCopies	        =0;
 MBR.BPB.RootRecords	        =0;
 MBR.BPB.OldSectors             =0;		// means not used
 MBR.BPB.MediaDescr		=0xF8;
 MBR.BPB.SizeOfCat		=0;
 MBR.BPB.TrackSectors		=TrackSectors;
 MBR.BPB.Surfaces		=Surfaces;
 MBR.BPB.HiddenSectors		=0;
 MBR.BPB.TotalSectors           =Surfaces*Cylinders*TrackSectors;
 MBR.PartyTable.ID		=0xAA55;

 // no partitition in system now
 MBR.PartyTable.Party[0].ActiveArea=0;
 MBR.PartyTable.Party[0].BeginSurface=0;
 MBR.PartyTable.Party[0].BeginCylinderAndSector=0;
 MBR.PartyTable.Party[0].OSFlag=0;
 MBR.PartyTable.Party[0].EndSurface=0;
 MBR.PartyTable.Party[0].EndCylinderAndSector=0;
 MBR.PartyTable.Party[0].EndCylinderAndSector=0;
 MBR.PartyTable.Party[0].FirstSectorOff=0;
 MBR.PartyTable.Party[0].SectorNum=0;	// recognize, that party not exist

 // null the REST
 movmem( MBR.PartyTable.Party, MBR.PartyTable.Party+1, 16 );
 movmem( MBR.PartyTable.Party, MBR.PartyTable.Party+2, 16 );
 movmem( MBR.PartyTable.Party, MBR.PartyTable.Party+3, 16 );

 if( SaveMasterBoot( Device, &MBR ) )
 {
  #ifdef DEBUG
   printf(" error writing MBR..." );
  #endif
  return ERR_FS_FATAL_ERROR;
 }

 printf("\n MBR saved, OK...");

 return ERR_FS_NO_ERROR;
}

//- FSCreatPartition -----------------------------------------------------------------------------------

word FSCreatPartition(
		       byte Device,
		       byte PartyID,
		       word FirstCylinder, word  LastCylinder,
		       byte Active
		     )
// - partition has to begin on surface 0
// - partyID  1..4
// - number of party is not important for sequence. Can be 2nd before 1st
// - MBR reserveves the first TRACK on medium

// - cil: korektni natazeni MBR z one device, spravne vyplneni MBR
//        korektni zapsani MBR
//        => neco ( zatim ne moc precizne ) vyplnit v BR,
//        zapsat BR NA SPRAVNE MISTO

{
 #ifdef DEBUG
  printf("\n\n Creating partition %u on device 0x%x...", PartyID, Device );
 #endif

 MasterBootRecord MBR;
 MasterBootRecord BR;

 // load MBR
 #ifdef DEBUG
  printf("\n Loading MBR of device 0x%x...", Device );
 #endif

 if( LoadMasterBoot( Device, &MBR ) )
 {
  #ifdef DEBUG
   printf("\n Error reading MBR..." );
  #endif
  return ERR_FS_FATAL_ERROR;
 }
 else
 {
  #ifdef DEBUG
   printf(" OK..." );
  #endif
 }


 word  FCylinderSectors=0,
       LCylinderSectors=0;

 dword SectorNum  				// count partition sectors
       =
       SectorPToL( MBR.BPB.TrackSectors,	// sector
		   MBR.BPB.TrackSectors,
		   MBR.BPB.Surfaces,
		   MBR.BPB.Surfaces-1,          // surface DEC IT ( 0.. )
		   LastCylinder ) 		// last cylinder
       -
       SectorPToL( 1,				// sector physical => 1
		   MBR.BPB.TrackSectors,
		   MBR.BPB.Surfaces,
		   (PartyID==1?1:0),
		   FirstCylinder ) 		// first
       +1;

 // cccccccc CcSsssss
 if( PartyID==1 )
  FCylinderSectors=(FirstCylinder<<8)+((FirstCylinder>>2)&0x00C0)+1+MBR.BPB.TrackSectors; // MBR adds one track
 else
  FCylinderSectors=(FirstCylinder<<8)+((FirstCylinder>>2)&0x00C0)+1;

 LCylinderSectors
 =
 (LastCylinder<<8)+((LastCylinder>>2)&0x00C0)+MBR.BPB.TrackSectors;

 #ifdef DEBUG
  printf("\n Filling MBR...");
 #endif

 MBR.PartyTable.Party[PartyID-1].ActiveArea		=Active;
 MBR.PartyTable.Party[PartyID-1].BeginSurface		=(PartyID==1?1:0) ; // each time 0, excepth MBR => 1
 MBR.PartyTable.Party[PartyID-1].BeginCylinderAndSector =FCylinderSectors;
 MBR.PartyTable.Party[PartyID-1].OSFlag			=0x51;	// RDWR
 MBR.PartyTable.Party[PartyID-1].EndSurface		=MBR.BPB.Surfaces-1; // each time
 MBR.PartyTable.Party[PartyID-1].EndCylinderAndSector   =LCylinderSectors;
 MBR.PartyTable.Party[PartyID-1].SectorNum		=SectorNum;


 // FirstSectorOff points to first empty sector of partition, is LOGICAL
 if( PartyID==1 )
  MBR.PartyTable.Party[PartyID-1].FirstSectorOff
  =SectorPToL( MBR.BPB.TrackSectors+1 , MBR.BPB.TrackSectors, MBR.BPB.Surfaces,0, FirstCylinder );
 else
  MBR.PartyTable.Party[PartyID-1].FirstSectorOff
  =SectorPToL( 1, MBR.BPB.TrackSectors, MBR.BPB.Surfaces,0, FirstCylinder );


 #ifdef DEBUG
  printf("\n Filling BR...");
 #endif

 // HiddenSectors is nr of sectors before part
 BR.BPB.SectorSize		=MBR.BPB.SectorSize;
 BR.BPB.ClusterSectors		=1;
 BR.BPB.ReservedSectors         =1;  	// 1=BOOT, means reserved inside
					// partition, if boot will be
					// longer => add it here
					// - now there will be group
					// statistic, it is counted
					// in format
 BR.BPB.CATCopies	        =0;	// filled with format
 BR.BPB.RootRecords	        =0;	// filled with format
 BR.BPB.OldSectors              =0;     // OLD nr of secs on HD, ( now unused)
 BR.BPB.MediaDescr		=0xF8;	// fixed device
 BR.BPB.SizeOfCat		=0; 	// filled with format
 BR.BPB.TrackSectors		=MBR.BPB.TrackSectors;
 BR.BPB.Surfaces		=MBR.BPB.Surfaces;
 // logical nr of FirstSectorOff does nr ( log 0.., phy 1.. )
 BR.BPB.HiddenSectors		=MBR.PartyTable.Party[PartyID-1].FirstSectorOff;
 BR.BPB.TotalSectors		=SectorNum;
 BR.BPB.RootFNODEAddr		=0; 	// filled with format
 BR.BPB.NrOfGroups		=0; 	// filled with format
 BR.BPB.DirtyFlag		=FALSE;
 BR.PartyTable.ID		=0xAA55;

 // save MBR
 #ifdef DEBUG
  printf("\n Saving MBR...");
 #endif

 if( SaveMasterBoot( Device, &MBR ) )
 {
  #ifdef DEBUG
   printf(" error writing MBR..." );
  #endif
  return ERR_FS_FATAL_ERROR;
 }


 // save BR
 #ifdef DEBUG
  printf("\n Saving BR...");
 #endif

 if( SaveBoot( Device, PartyID, &BR ) )
 {
  #ifdef DEBUG
   printf(" error writing MBR..." );
  #endif
  return ERR_FS_FATAL_ERROR;
 }


 #ifdef DEBUG
  printf(
	 "\n Device&Partition info:"
	 "\n  Device       0x%x"
	 "\n  TrackSectors %u"
	 "\n  Surfaces     %u"
	 "\n  BR.FirstCyl  %u"
	 "\n  BR.LastCyl   %u"
	 "\n  SectorNum    %lu"
	 "\n  FrstLSectOff %lu"
	 "\n  BegSurf      %u"
	 "\n  EndSurf      %u"
	 "\n  Active       0x%x"
	 "\n  Beg(Cyl)PSec %u"
	 "\n  BR.HidSecs   %lu"
	 "\n  Br.TotSecs    %lu"
	 ,
	 Device,
	 MBR.BPB.TrackSectors,
	 MBR.BPB.Surfaces,
	 FirstCylinder,
	 LastCylinder,
	 MBR.PartyTable.Party[PartyID-1].SectorNum,
	 MBR.PartyTable.Party[PartyID-1].FirstSectorOff,
	 MBR.PartyTable.Party[PartyID-1].BeginSurface,
	 MBR.PartyTable.Party[PartyID-1].EndSurface,
	 MBR.PartyTable.Party[PartyID-1].ActiveArea,
	 MBR.PartyTable.Party[PartyID-1].BeginCylinderAndSector&0x003F,
	 BR.BPB.HiddenSectors,
	 BR.BPB.TotalSectors
	);
 #endif


 // check
 #ifdef DEBUG
  printf("\n Creating check ...");
  LoadMasterBoot( Device, &MBR );
  LoadBoot( Device, PartyID, &BR );

  printf("\n Creating DONE...");
 #endif

 return ERR_FS_NO_ERROR;
}


//- EOF -----------------------------------------------------------------------------------
