#include "hardware.h"

//#define DEBUG
//#define SEE_ERROR
#define INIT_SECTORS
// #define SIMULATE_BADBLOCKS

// - dodelat pretypovani u prevodu sectoru l-p ( rozsah dword/word)
// => pouzivat zatim zapis logickych sectoru => hodi se to i na striping
// => ten bude spravne mapovat sectory na disky
// uvnitr fci ...logical sector se pozdeji bude volat ... physical sector
// a tam se take provede uz spravne volani BIOSu
// nebo nejlepsi reseni: zapisem fyzickych sectoru se vubec nezabyvat
// a tvarit se jakoby fce zapisu logickeho sectoru umel BIOS => pracovat
// pouze s logickymi sectory...

// MAXIMAL SIZE OF HD I CAN WRITE TO
// working with BIOS: (cccccccc CcSsssss => c=10, s=6)
//     cylinder       0..1023                    1024
//     sector number  0..63                      64
//     head number    0..256  ( was 15 )         256
//     number sectors 1..256  ( max 1 cylinder )
//
// =>  512*1024*64*256 = 8,589 GB max disk size


extern int              ActiveHDHandle;
extern int              HDHandle[5];

extern byte             BitMask[16];

//- Interrupt ----------------------------------------------------------------


word Interrupt(
                byte IntNum,
                word &AX,
                word &BX,
                word &CX,
                word &DX,
                word &ES,
                word &Flags
              )
// - dodelat cteni vice sectoru najednou
// - vyresit pouzivani MBR a BR
{
 MasterBootRecord       MBR;

 dword    logicalsector;
 word     bytes;
 void far *buf;



 switch( IntNum )
 {
  case 0x08: // parameters of disc
             /*
              in:
               AH 02h
               DL jednotka      0=A, 1=B, 80h 1.hard, 81h 2.hard

              out:
               AH errorlevel, if CF=1 contains AH error level, CF=0 ok
               BL type
               CH cylinders
               CL sectors per track  cccccccc CcSsssss, Cc are higher bits
               DH heads=surfaces
               DL pocet fyz. zarizeni na prvnim zarizeni
             */

             // hard: cyls 1024, head 2, sectors 17
             AX=0;
             BX=0x80;
             CX=1024-1;         // counted from 0
             CX<<=6;
             CX+=17;
             DX=(2<<8)+1;

             break;

  case 0x13: switch( AX>>8 )
             {
              case 0x02:
                        // read sector
                        /*
                         AH 02h
                         AL nr of sectors to read ( max. cylinder )
                         CH cylinder
                         CL sector
                         DH surface
                         DL jednotka    0=A, 1=B, 80h 1.hard, 81h 2.hard
                         ES:BX buffer Seg:Off
                        */

                        if( LoadMasterBoot( (byte)DX&0xF, &MBR ) )
                        {
                         #ifdef SEE_ERROR
                          printf("\n Error reading MBR..." );
                         #endif

                         return ERR_FS_FATAL_ERROR;
                        }

                        buf=MK_FP( ES, BX );

                        // logical 0.. , physical 1..
                        logicalsector
                        =
                        ((DX>>8)+(((CX<<2)&0x0300)+(CX>>8))*MBR.BPB.Surfaces)
                        *
                        MBR.BPB.TrackSectors+(CX&0x003F)-1;

                        lseek( ActiveHDHandle, logicalsector*512, SEEK_SET );

                        if ((_dos_read( ActiveHDHandle, buf, 512*(AX&0xFF), &bytes)) != 0)
                         printf("\n Read error: logical sector %lu");

                        break;
              case 0x03:
                        // write sector
                        /*
                         AH 03h
                         AL nr of sectors to write
                         CH cylinder
                         CL sector
                         DH surface
                         DL jednotka    0=A, 1=B, 80h 1.hard, 81h 2.hard
                         ES:BX buffer Seg:Off
                        */

                        if( LoadMasterBoot( (byte)DX&0xF, &MBR ) )
                        {
                         #ifdef SEE_ERROR
                          printf("\n Error reading MBR..." );
                         #endif

                         return ERR_FS_FATAL_ERROR;
                        }

                        buf=MK_FP( ES, BX );

                        // logical 0.. , physical 1..

                        logicalsector
                        =
                        ((DX>>8)+(((CX<<2)&0x0300)+(CX>>8))*MBR.BPB.Surfaces)
                        *
                        MBR.BPB.TrackSectors+(CX&0x003F)-1;

                        lseek( ActiveHDHandle, logicalsector*512l, SEEK_SET );

                        if((_dos_write( ActiveHDHandle, buf, 512*(AX&0xFF), &bytes)) != 0)
                         printf("\n Write error: logical sector %lu");

                        break;
             }
 }

 return Flags;
}


//- GetHardParameters ----------------------------------------------------------------

word GetHardParameters( byte DeviceID,
                        word &Cylinders,
                        word &TrackSectors,
                        word &Heads )
// - DODELAT NA VOLANI BIOSU

// to je current drivu, obecne drivy se detekuji na BIOS 1. int41, 2. int46
// => musim se podivat do interrupt listu
// FCE NENI KOREKTNE
{
 struct DeviceTabEntry Device[5];

 if( LoadDeviceTable( &Device[0] ) )
  return ERR_FS_FATAL_ERROR;

 Cylinders   =Device[DeviceID&0x0F].Cylinders;
 Heads       =Device[DeviceID&0x0F].Heads;
 TrackSectors=Device[DeviceID&0x0F].Sectors;

 return ERR_BIOS13_NO_ERROR;
}

//- WritePhysicalSector ----------------------------------------------------------------

word WritePhysicalSector( word howmany,  word sector, word surface,
                          word cylinder, void *bufptr, word jednotka )
{
 word Flag= 0,
      AX  = (0x03<<8)+howmany;

 Interrupt( 0x13,
            AX,
            FP_OFF(bufptr),
            (cylinder<<8)+((cylinder>>2)&0x00C0)+sector,
            (surface<<8)+jednotka,
            FP_SEG(bufptr),
            Flag
          );

 if( BitMask[FLAG_CF]&Flag )
  return AX>>8;                                 // some error
 else
  return ERR_BIOS13_NO_ERROR;                   // no error
}

//- ReadPhysicalSector ----------------------------------------------------------------

word ReadPhysicalSector( word howmany,  word sector, word surface,
                         word cylinder, void *bufptr, word jednotka )
{
 word Flag= 0,
      AX  = (0x02<<8)+howmany;

 Interrupt( 0x13,
            AX,
            FP_OFF(bufptr),
            (cylinder<<8)+((cylinder>>2)&0x00C0)+sector,
            (surface<<8)+jednotka,
            FP_SEG(bufptr),
            Flag
          );

 if( BitMask[FLAG_CF]&Flag )
  return AX>>8;                                 // some error
 else
  return ERR_BIOS13_NO_ERROR;                   // no error
}

//- SectorLToP ----------------------------------------------------------------

void  SectorLToP( dword Logical,  word TrackSectors, word Surfaces,
                  word  &Physical, word &Surface, word &Cylinder )
// logical to physical sector
// pretypovani long/...!
{
 Surface =(Logical/TrackSectors)%Surfaces;
 Cylinder=Logical/(TrackSectors*Surfaces);
 Physical=1 + (Logical%TrackSectors);
}

//- SectorPToL ----------------------------------------------------------------

dword SectorPToL( dword Physical, dword TrackSectors, dword Surfaces,
                                  dword Surface,      dword Cylinder )
// returns logical converted from physical sector
// PHYSICALLY:
//  surfaces 0..
//  tracks   0..
//  sectors  1..
{
 return ((Surfaces*TrackSectors)*Cylinder+Surface*TrackSectors+(Physical-1));
}

//- VerifyLogicalSector ----------------------------------------------------------------

word VerifyLogicalSector( byte Device, dword Logical, byte Number )
// MBR a aktivni BR budou natazeny v pameti
// osetrit DODELAT osetreni alokaci
{
 dword    Target  = Logical*512lu;
 word     bytes;
 byte     far *Buffer = (byte far *)farmalloc(512lu);

 if( !Buffer ) { printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ ); exit(0); }

 #ifdef DEBUG
  printf("\n VERIFYing sector %lu ... ", Logical );
 #endif

 lseek( HDHandle[Device&0x0F], Target, SEEK_SET );

 #ifdef INIT_SECTORS
  word i;
  for( i=0; i<512; i++ ) Buffer[i]=0xDD;
 #endif

 if( _dos_write( HDHandle[Device&0x0F], Buffer, Number*512u, &bytes) != 0 )
 {
  #ifdef SEE_ERROR
   printf("\n Write error: logical sector %lu, device %d, handle 0x%x...", Logical, Device, HDHandle[Device&0x0F] );
  #endif
  farfree(Buffer);
  return ERR_FS_WR_LOGICAL_SECTOR;
 }

 #ifdef SIMULATE_BADBLOCKS
  // simulating error
  if( random(100) )
  {
   #ifdef DEBUG
    printf("OK");
   #endif
   farfree(Buffer);
   return ERR_FS_NO_ERROR;
  }
  else
  {
   #ifdef DEBUG
    printf("NOK");
   #endif
   farfree(Buffer);
   return ERR_FS_VERIFY_FAILED;
  }
 #else
  #ifdef DEBUG
   printf("OK");
  #endif
  farfree(Buffer);
  return ERR_FS_NO_ERROR;
 #endif


}

//- Beep -----------------------------------------------------------------------

void Beep( void )
{
 sound(200);
 delay(300);
 nosound();
 delay(150);
}

//- KeybWait -----------------------------------------------------------------------

void KeybWait( void )
{
 while( kbhit() ); getch();
 getch();
}

//- KeybClear -----------------------------------------------------------------------

void KeybClear( void )
{
 while( kbhit() ) getch();
}


//- EOF -----------------------------------------------------------------------




