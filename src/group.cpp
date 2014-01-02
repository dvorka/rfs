#include "group.h"

//#define DEBUG
//#define SEE_ERROR

extern byte BitMask[16];
extern int  ActiveHDHandle,
	    HDHandle[5];


//- CreateGroup -----------------------------------------------------------------

word CreateGroup( byte Device, dword FirstSector, dword GroupID,
		  dword &SectorsInGroup )
// - FirstSector is first logical sector on disk of group
// - NrOfGroup is number of group
// - length of group: 4097sectors=512*8+bitmap
// - group numbers: 0,1,...
// - even group(0): bitmap before, odd group(1): bitmap after
// - group has 4096 sector
// - bad sector in group
//    - save to BadBlocks.system  DODELAT
//    - mark in bitmap as full
//    - => stat must be returned
{
 byte Percentage;

 word i,      					// group has 4096 sectors
      BitmapByte,
      ByteBit,
      NrOfBadBlocks=0,
      GroupIsEven=!(GroupID%2);

 dword ActuallSector=FirstSector;




 byte far *Bitmap=(byte far *)farmalloc(512); 	// bitmap of group
 if( !Bitmap ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

 for( i=0; i<512; i++ ) Bitmap[i]=0xFF; 	// bit==1 => sector is free

 // even group: bitmap before, odd group: bitmap after => bitmap sector test
 if( GroupIsEven )
 {
  if( VerifyLogicalSector( Device, FirstSector ) )
  {
   #ifdef DEBUG
    printf("\n Bad bitmap sector for group nr. %lu cann\'t be created!", GroupID);
   #endif

    Percentage=100;	// 100% full group


   farfree( Bitmap );
   return ERR_FS_BAD_GROUP_BMP_SECTOR;
  }
 }
 else
 {
  if( VerifyLogicalSector( Device, FirstSector+SECS_IN_GROUP ) )
  {
   #ifdef DEBUG
    printf("\n Bad bitmap sector for group nr. %lu cann\'t be created!", GroupID);
   #endif

    Percentage=100;	// 100% full group

   farfree( Bitmap );

   return ERR_FS_BAD_GROUP_BMP_SECTOR;
  }
 }


 // shift group one sector right if even ( bitmap before )
 if( GroupIsEven ) ActuallSector++;

 for( i=0; i<SECS_IN_GROUP; i++, ActuallSector++ )
 {

  // if sector bad => mark it in bmp as full (0), list it in badsectors.system
  if( VerifyLogicalSector( Device, ActuallSector ) )
  {
   BitmapByte=i/8;
   ByteBit   =i%8;
   Bitmap[BitmapByte]=~((~Bitmap[BitmapByte])|BitMask[ByteBit]); // null bit of bad sector

   NrOfBadBlocks++;


   // DODELAT add sector to "BadGroups.system"
   // ( now somewhere in mem - there is NO dirs and files )
  }
  // else ... everything is ok, sector is free

 }


 if( GroupIsEven )
 {
  if( WriteLogicalSector( Device, FirstSector, (void far *)Bitmap, 1 ) )
  {
   #ifdef SEE_ERROR
    printf("\n Bitmap sector for group nr. %lu not written!", GroupID);
   #endif

   farfree( Bitmap );
   return ERR_FS_BAD_GROUP_BMP_SECTOR;
  }
 }
 else
 {
  if( WriteLogicalSector( Device, FirstSector+SECS_IN_GROUP, (void far *)Bitmap, 1 ) )
  {
   #ifdef SEE_ERROR
    printf("\n Bitmap sector for group nr. %lu not written!", GroupID);
   #endif
   farfree( Bitmap );
   return ERR_FS_BAD_GROUP_BMP_SECTOR;
  }
 }

 float H=((float)NrOfBadBlocks/4096.0f)*100.0f;
 Percentage=(byte)H;

 SectorsInGroup=4096lu-(dword)NrOfBadBlocks;

 printf("\n GroupNum %3lu FirstLSec %7lu TotSecs 4096 OK %lu Bad %d Perc %d\%... ",
	     GroupID, FirstSector, SectorsInGroup, NrOfBadBlocks, Percentage );

 farfree( Bitmap );

 return ERR_FS_NO_ERROR;
}

//- CreateUncompleteGroup -----------------------------------------------------------------

// This function isn't use now because not completed group are not used

/*

word CreateUncompleteGroup( byte Device, dword FirstSector, dword GroupID,
			    dword &SectorsInGroup )
// - EVERYTIME HAS BITMAP ON LEFT SIDE!
// - FirstSector is first logical sector on disk of group
// - NrOfGroup is number of group
// - length of group: 4097sectors=512*8+bitmap
// - group numbers: 0,1,...
// - even group(0): bitmap before, odd group(1): bitmap after
// - group has 4096 sector
{
 #ifdef DEBUG
  byte Percentage;
 #endif

 word i,      					// group has 4096 sectors
      BitmapByte,
      ByteBit,
      NrOfBadBlocks=0;

 dword ActuallSector=FirstSector;



 byte far *Bitmap=(byte far *)farmalloc(512); 	// bitmap of group
 if( !Bitmap )
 {
  printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ );
  exit(0);
 }

 for( i=0; i<512; i++ ) Bitmap[i]=0x00; 	// bit==0 => sector is full

 // bitmap BEFORE everytime => bitmap sector test
 if( VerifyLogicalSector( Device, FirstSector ) )
 {
  #ifdef DEBUG
   printf("\n Bad bitmap sector for group nr. %lu cann\'t be created!", GroupID);

   Percentage=100;
  #endif


  farfree( Bitmap );
  return ERR_FS_BAD_GROUP_BMP_SECTOR;
 }

 for( i=0; i<SectorsInGroup; i++, ActuallSector++ )
 {

  // list it in badsectors.system
  if( VerifyLogicalSector( Device, ActuallSector ) )
  {
   // sector is bad => stays full, list it in badsectors.system
   NrOfBadBlocks++;
  }
  else // ... everything is ok, sector is free
  {
   BitmapByte=i/8;
   ByteBit   =i%8;
   Bitmap[BitmapByte]|=BitMask[ByteBit]; // set bit to 1 => is free
  }
 }


 if( WriteLogicalSector( Device, FirstSector, (void far *)Bitmap, 1 ) )
 {
  #ifdef DEBUG
   printf("\n Bitmap sector for group nr. %lu not written!", GroupID);

   Percentage=100;
  #endif


  farfree( Bitmap );
  return ERR_FS_BAD_GROUP_BMP_SECTOR;
 }

 #ifdef DEBUG
  float H=((float)NrOfBadBlocks/(float)SectorsInGroup)*(float)100;

  Percentage=(byte)H;
 #endif

 #ifdef DEBUG
  printf("\n GroupNum %3lu FirstLSec %7lu TotalSecs %lu",
	     GroupID,           FirstSector,     SectorsInGroup );
  printf(" OK %u", SectorsInGroup-NrOfBadBlocks );
  printf(" Bad %u", NrOfBadBlocks );
  printf(" Perc %d\%... ", Percentage );
 #endif

 farfree( Bitmap );

 return ERR_FS_NO_ERROR;
}

*/

//- CreateGroups -----------------------------------------------------------------

word CreateNrOfGroups( byte  DeviceID, dword FirstSector, dword Sectors,
		       dword  &Created, dword &TotalSectors,
		       dword &GroupStatisticSectors )
// How it is with sectors...
// - interval of sectors which this function gets can fill whole from 0..
//   with group statistic and groups itself
// - later are sectors with statistic included in BR.ReservedSectors
//   for fast direct writing to partition

// Parameter...
// - FirstSector: nr of the first sector which will be used => for stat,
//   after stat section groups begin
// - Sectors: how many sectors use for groups and stat
// - TotalSectors: nr of secs usable for data in all groups together

{
       TotalSectors=0;		// null nr of sectors created

 dword HowMany=Sectors/GROUP_SIZE_SEC,
       Rest   =Sectors%GROUP_SIZE_SEC,
       ID,
       BringsSectors=0,
       SectorOff=FirstSector,   // used in for() which creates groups
       StatSecOff=FirstSector;	// first sector of statistic point to free

 word  StatItem;		// offset in sector, points to free

 word  far *Sector=(word far *)farmalloc(512lu); // must be allocate 512!
 if( !Sector ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

 for( StatItem=0; StatItem<(512/2); StatItem++ )	// null vector
  Sector[StatItem]=0x0000;				// mark group as full

 StatItem=0;




 #ifdef DEBUG
  printf("\n Creating groups on device 0x%x: FirstSec %lu, NrOfSec %lu...", DeviceID, FirstSector, Sectors );
 #endif


 // HowMany says how many groups will be created => I know how many sectors
 // rest => test if rest is big enough for static sectors =>
 // if is not I have to HowMany--

 GroupStatisticSectors=HowMany/(512lu/2lu); 	     // nr of sectors used for statistic
 if( HowMany%512lu ) GroupStatisticSectors++;

 if( Rest < GroupStatisticSectors ) HowMany--; // there was not place for stat
				     // => one group must be canceled

 // shift the first sector of the first group
 SectorOff+=GroupStatisticSectors;

 #ifdef DEBUG
  printf("\n  %lu sector(s) reserved for party group statistic...", GroupStatisticSectors );
 #endif



 // create complete groups
 for( ID=0; ID<HowMany; ID++, SectorOff+=GROUP_SIZE_SEC )
 {
  if( CreateGroup( DeviceID, SectorOff, ID, BringsSectors ) )
  {
   #ifdef SEE_ERROR
    printf("\n Group %d cann't be created...", ID );
   #endif

   // no sector can be used => to statistic write 0 ( group full )
   BringsSectors=0;

   // DODELAT write ID to "BadGroups.system"
   // ( now somewhere in mem - there is NO dirs and files )
  }

  TotalSectors+=BringsSectors;

  // zapsat do statistiky BringsSectors:
  // pocitat si offset ve statistickem sektoru. Kdyz je 255 (0..255)
  // => ulozit ho a vynulovat buffer ( 0 znamena nula volnych sektoru )
  // ve statistickem sektoru jsou WORDY => bude tam 256 polozek
  // tak se nemusim starat o zlomek co zbyde, diky inicializacim nulami
  // pokud nenastavim jinak zustanou grupy jakoby plne

  if( StatItem==(512/2) ) // I am after sector => must be saved and new created
  {
   // save statistic sector
   WriteLogicalSector( DeviceID, StatSecOff, (void far *)Sector, 1 );
   {
    #ifdef SEE_ERROR
     printf("\n Statistic sector %lu not written!", StatSecOff );
    #endif

    // DODELAT include sec to "BadSectors.system"
    // ( now somewhere in mem - there is NO dirs and files )
   }

   StatSecOff++;	// points to free

   for( StatItem=0; StatItem<(512/2); StatItem++ )	// null vector
    Sector[StatItem]=0x0000;				// mark group as full

   StatItem=0;		// points to free
  }

  // put value to stat Sector
  Sector[StatItem++]=BringsSectors;

 }

 // save statistic sector
 if( WriteLogicalSector( DeviceID, StatSecOff, (void far *)Sector, 1 ) )
 {
  #ifdef SEE_ERROR
   printf("\n Statistic sector %lu not written!", StatSecOff );
  #endif

  // DODELAT include sec to "BadSectors.system"
 }
 #ifdef DEBUG
  printf("\n Statistic sector %lu written...", StatSecOff );
 #endif


 printf("\n Successfuly created %lu complete groups containing %lu sectors...", HowMany, TotalSectors );

 Created=HowMany;

 farfree( Sector );

 return ERR_FS_NO_ERROR;

 // --- DO NOT CREATE UNCOMPLETE GROUPS => WILL NOT BE USED NOW! ---

 /*
 // if rest ==1 => ignore 1 sector

 if( Rest>1 )
 {
  #ifdef DEBUG
   printf("\n Uncomplete Group: rest %d sectors...", Rest );
  #endif


  if( CreateUncompleteGroup( DeviceID, SectorOff, ID, Rest ) )
  {
   #ifdef DEBUG
    printf("\n Error occured in %s line %d...", __FILE__, __LINE__ );
   #endif

   return ERR_FS_FATAL_ERROR;
  }


  #ifdef DEBUG
   printf("\n OK...");
  #endif
 }

 Created=HowMany+1; // created HowMany complete groups + 1 not complete
 */
}

//- LoadBitmap -------------------------------------------------------------------------

word LoadBitmap( byte Device,
		 byte PartyID,
		 dword GroupID,
		 MasterBootRecord *BR,
		 void far *Bitmap
	       )
// - groups: 0...
// - ...BR in, Bitmap out
{
 // points to the first bitmap sec in party
 dword FirstSector=BR->BPB.HiddenSectors+(dword)BR->BPB.ReservedSectors;
 dword gdw=GroupID%2lu;
 byte  GroupIsOdd = (byte)gdw;

 if( GroupID<BR->BPB.NrOfGroups ) // some group before last
 {
  FirstSector+=GroupID*GROUP_SIZE_SEC; // count first sec of group

  if( GroupIsOdd ) // bitmap right => move right for bmp sec
   FirstSector+=SECS_IN_GROUP;
 }
 else
 {
  #ifdef SEE_ERROR
   printf("\n Error: GroupID %lu is too big... ", GroupID );
  #endif
  return ERR_FS_FATAL_ERROR;
 }

 if( ReadLogicalSector( Device, FirstSector, Bitmap ) )
 {
  #ifdef SEE_ERROR
   printf("\n Error reading bitmap LogHDSec %lu, device 0x%x, party %u... ", FirstSector, Device, PartyID );
  #endif
  return ERR_FS_FATAL_ERROR;
 }

 #ifdef DEBUG
  printf("\n Bitmap read from sec LogHDSec %lu, device 0x%x, party %u... ", FirstSector, Device, PartyID );
 #endif

 return ERR_FS_NO_ERROR;
}

//- SaveBitmap -------------------------------------------------------------------------

word SaveBitmap( byte Device,
		 byte PartyID,
		 dword GroupID,
		 MasterBootRecord *BR,
		 void far *Bitmap
	       )
// - groups: 0...
// - ...BR in, Bitmap out
{
 // points to the first bitmap sec in party
 dword FirstSector=BR->BPB.HiddenSectors+(dword)BR->BPB.ReservedSectors;
 dword gdw=GroupID%2lu;
 byte  GroupIsOdd = (byte)gdw;

 if( GroupID<BR->BPB.NrOfGroups ) // some group before last
 {
  FirstSector+=GroupID*GROUP_SIZE_SEC; // count first sec of group

  if( GroupIsOdd ) // bitmap right => move right for bmp sec
   FirstSector+=SECS_IN_GROUP;
 }
 else
 {
  #ifdef SEE_ERROR
   printf("\n Error: GroupID %lu is too big... ", GroupID );
  #endif
  return ERR_FS_FATAL_ERROR;
 }

 if( WriteLogicalSector( Device, FirstSector, Bitmap ) )
 {
  #ifdef SEE_ERROR
   printf("\n Error writing bitmap LogHDSec %lu, device 0x%x, party %u... ", FirstSector, Device, PartyID );
  #endif
  return ERR_FS_FATAL_ERROR;
 }

 #ifdef DEBUG
  printf("\n Bitmap saved to LogHDSec %lu, device 0x%x, party %u... ", FirstSector, Device, PartyID );
 #endif

 return ERR_FS_NO_ERROR;
}

//- EOF -------------------------------------------------------------------------

















































