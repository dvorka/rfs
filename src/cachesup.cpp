#include "cachesup.h"


//#define DEBUG

extern StripSession 	StripingSession;
extern DataPartysQueue  far *DataPartys; // list of data devices in system
extern byte 		BitMask[16];

//- class BMP methods --------------------------------------------------------

BMP::BMP( long BitLength )
// if BitLengt/8 == 0 one byte is allocated and set unnecessarly
{

 Bmp=(byte far *)farmalloc(BitLength/8+1lu);

 if( !Bmp )
 {
  printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ );
  exit(0);
 }

 #ifdef DEBUG
  printf("\n BMP created, lng: %d", BitLength );
  printf(" far: %luB", (dword)farcoreleft() );
 #endif

 Length=Free=BitLength;

 // null bits
 long i;
 for( i=0l; i<=(BitLength/8l); i++) Bmp[i]=0x00;

 // set slack
 SetInterval( BitLength, (BitLength/8l+1l)*8l-1l ); // - lower is BitLength because 0..

 return;
}

//--------------------------------------------------------------------------------

void BMP::SetInterval( long FirstBit, long LastBit )
// - FirstBit and LastBit included
// - 0..
{
 long LowByte=FirstBit/8l,             // first byte 0..
      LowBit= (7l-FirstBit%8l) + 1l, // how many bits should be set

      HighByte=LastBit/8l,	  // last byte 0..
      HighBit= LastBit%8l + 1l, // how many bits should be set

      i;

 if( LowByte==HighByte )
 {
  for( i=(7l-FirstBit%8l); i>=(7l-LastBit%8l); i-- )
  {
   Bmp[LowByte]|=BitMask[i];
   --Free;
  }
 }
 else
 {

  // lowest byte
  for( i=0l; i<LowBit; i++ )
  {
   Bmp[LowByte]|=BitMask[i];
   --Free;
  }


  // whole bytes ( inner )
  for( i=LowByte+1l; i<=(HighByte-1l); i++ )
  {
   Bmp[i]=0xFF;
   Free-=8;
  }

  // highest byte
  for( i=0l; i<HighBit; i++ )
  {
   Bmp[HighByte]|=BitMask[7l-i];
   --Free;
  }
 }

}

//--------------------------------------------------------------------------------

void BMP::ClearInterval( long FirstBit, long LastBit )
// - FirstBit and LastBit included
// - 0..
{
 long LowByte=FirstBit/8l,             // first byte 0..
      LowBit= (7l-FirstBit%8l) + 1l, // how many bits should be set

      HighByte=LastBit/8l,	  // last byte 0..
      HighBit= LastBit%8l + 1l, // how many bits should be set

      i;

 if( LowByte==HighByte )
 {
  for( i=(7l-FirstBit%8l); i>=(7l-LastBit%8l); i-- )
  {
   Bmp[LowByte]=(~(~Bmp[LowByte]|BitMask[i]));
   ++Free;
  }
 }
 else
 {

  // lowest byte
  for( i=0l; i<LowBit; i++ )
  {
   Bmp[LowByte]=(~(~Bmp[LowByte]|BitMask[i]));
   ++Free;
  }


  // whole bytes ( inner )
  for( i=LowByte+1l; i<=(HighByte-1l); i++ )
  {
   Bmp[i]=0x00;
   Free+=8;
  }

  // highest byte
  for( i=0l; i<HighBit; i++ )
  {
   Bmp[HighByte]=(~(~Bmp[HighByte]|BitMask[7l-i]));
   ++Free;
  }
 }

}

//--------------------------------------------------------------------------------

byte BMP::BitIsSet( long Bit )
// - bits are counted from 0 ( Bit==6 means test seventh bit )
{
 long WhereByte=Bit/8l,             // first byte 0..
      WhereBit =7l-Bit%8l;   // which bit

 if( Bmp[WhereByte]&BitMask[WhereBit] )
  return TRUE;
 else
  return FALSE;
}

//--------------------------------------------------------------------------------

byte BMP::FindHole( long From, long &FirstBit, long &HoleLength )
// - 0..
// - search From bit included
// - FirstBit is first bit of hole
// - Length is length of hole
{
 #ifdef DEBUG
  printf("\n Searching hole:");
 #endif

 while( From<Length )
 {
  if( !BitIsSet(From) )
  {
   FirstBit=From;
   HoleLength=0;

   while( !BitIsSet( From ) && From<Length ) // add against overflow
   {
    HoleLength++;
    From++;
   }

   #ifdef DEBUG
    printf(" found FirstBit %d ", FirstBit );
    printf(" %d... ", HoleLength );
   #endif

   return TRUE;
  }
  else
  {
   From++;
  }
 }

 #ifdef DEBUG
  printf(" not found...");
 #endif

 return FALSE;
}

//--------------------------------------------------------------------------------

byte BMP::FindAllocSecHole( word From, word &FirstBit, word WantedLength )
// - search for hole if found set bits
// - 0..
// - search From bit included
// - FirstBit is first bit of hole
// - Length is length of hole demanded
// - returns TRUE if hole of width HoleLength found else returns FALSE;
{
 word HoleLength;

 #ifdef DEBUG
  printf("\n Searching hole:");
 #endif

 while( From<Length )
 {
  if( BitIsSet(From) )
  {
   FirstBit=From;
   HoleLength=0;

   while( BitIsSet( From ) && From<(word)Length )
   {
    HoleLength++;
    From++;
   }

   #ifdef DEBUG
    printf("\n   found FirstBit %u ", FirstBit );
    printf(" Lng %u ", HoleLength );
   #endif

   // some hole found is big enough?
   if( HoleLength >= WantedLength )
   {
    long FBit=(long)FirstBit,
	 LBit=(long)FirstBit+((long)WantedLength-1l);

    // OK => set bits in bitmap
    ClearInterval( FBit, LBit );

    #ifdef DEBUG
     printf(" WLng %u, set < %lu, %lu >", WantedLength, FBit, LBit );
    #endif

    return TRUE;
   }

   // try search again...
   #ifdef DEBUG
    printf(" not big enough -> I want %u...", WantedLength );
   #endif
  }

  From++;
 }

 #ifdef DEBUG
  printf(" not found...");
 #endif

 return FALSE;
}

//--------------------------------------------------------------------------------

BMP::~BMP()
{
 #ifdef DEBUG
  printf("\n BMP deleted, lng: %d", Length );
  printf(" far: %luB", (dword)farcoreleft() );
 #endif

 farfree( Bmp );
}

//- class PackageIDMan -------------------------------------------------------------------------------

PackageIDMan::PackageIDMan()
{
 IDs =  new BMP( 8000 );

 // reserve ID 0;
 IDs->SetInterval( 0l, 0l );

 #ifdef DEBUG
  printf("\n PackageIDMan constructor ... ");
 #endif
}

word PackageIDMan::GetID ( void )
{
 long PackageID=0;

 IDs->FindHole( 0l, PackageID, 1l );

 // reserve this ID;
 IDs->SetInterval( PackageID, PackageID );

 #ifdef DEBUG
  printf("\n PackageGetID: %d...", PackageID );
 #endif

 return (word)PackageID;
}

void PackageIDMan::FreeID( word PackageID )
{
 #ifdef DEBUG
  printf("\n PackageFreeID: %d...", (long)PackageID );
 #endif

 long H=(long)PackageID;

 IDs->ClearInterval( H, H );

}

PackageIDMan::~PackageIDMan()
{
 #ifdef DEBUG
  printf("\n PackageIDMan destructor ... ");
 #endif

 delete IDs;
}


//- class PriQueue methods -------------------------------------------------------

PriQueue::PriQueue( void )
{
 Head=(PriQueueItem far *)farmalloc((dword)sizeof(PriQueueItem));

 #ifdef DEBUG
  printf("\n PriQueue() bef. far: %luB",(unsigned long)farcoreleft() );
 #endif


 Tail=(PriQueueItem far *)farmalloc((dword)sizeof(PriQueueItem));

 if( Head==NULL || Tail==NULL )
 {
  printf("\n Not enough far memory in %s line %d...", __FILE__, __LINE__ );
  exit(0);
 }

 // init
 NrOfItems=0;
 Head->Last=Tail->Next=NULL;
 Head->Next=Tail;
 Tail->Last=Head;

 #ifdef DEBUG
  printf(" aft. %luB, Head: 0x%Fp, Tail: 0x%Fp...",(unsigned long)farcoreleft(), Head, Tail );
 #endif
}

//----------------------------------------------------------------------------

void PriQueue::InsertAfterHead( PriQueueItem far *InsertedItem )
// Inserted item is inserted not copied!
{
 if( InsertedItem )
 {
  Head->Next->Last=InsertedItem;

  InsertedItem->Next=Head->Next;
  InsertedItem->Last=Head;

  Head->Next=InsertedItem;

  NrOfItems++;

  #ifdef DEBUG
   printf("\n  Item %Fp inserted,", InsertedItem );
  #endif
 }

 #ifdef DEBUG
  printf(" far mem: %luB...",(unsigned long)farcoreleft() );
 #endif
}

//----------------------------------------------------------------------------

void PriQueue::Delete( PriQueueItem far *DelItem )
// DelItem points to node in queue which should be deleted
{
 if( DelItem && DelItem!=Head && DelItem!=Tail )
 {
  DelItem->Last->Next=DelItem->Next;
  DelItem->Next->Last=DelItem->Last;

  // must be used delete, because program evides new instances
  // and if farfree( DelItem ); is used after return 0 of main
  // it says Null pointer assignment...
  delete DelItem;

  NrOfItems--;

  #ifdef DEBUG
   printf("\n  Item %Fp deleted,", DelItem );
  #endif
 }

 #ifdef DEBUG
  printf(" far mem: %luB...",(unsigned long)farcoreleft() );
 #endif
}

//----------------------------------------------------------------------------

PriQueueItem far *PriQueue::Next( PriQueueItem far *Item )
// if next is tail returns NULL
{
 if( Item->Next==Tail )
  return NULL;
 else
  return
   Item->Next;
}

//----------------------------------------------------------------------------

PriQueueItem far *PriQueue::Last( PriQueueItem far *Item )
// if last is head returns NULL
{
 if( Item->Last==Head )
  return NULL;
 else
  return Item->Last;
}

//----------------------------------------------------------------------------

PriQueueItem far *PriQueue::GetHead( void )
// get node head which contains data ( it's different from Head! )
// if no data node in queue => returns NULL
{
 if( Head->Next==Tail )
  return NULL;
 else
  return Head->Next;
}

//----------------------------------------------------------------------------

PriQueueItem far *PriQueue::GetTail( void )
// if no data node in queue => returns NULL
{
 if( Head->Next==Tail )
  return NULL;
 else
  return Tail->Last;
}

//----------------------------------------------------------------------------

void PriQueue::Destroy( void )
{
 while( Head->Next!=Tail ) Delete( Head->Next );

 NrOfItems=0;

 #ifdef DEBUG
//  printf("\n  Destroy, far mem: %luB...",(unsigned long)farcoreleft() );
 #endif

 return;
}

//----------------------------------------------------------------------------

PriQueue::~PriQueue( void )
{
 Destroy();

 delete Head;// is better than farfree(Head);
 delete Tail;// is better than farfree(Tail);

 #ifdef DEBUG
  printf("\n  After ~PriQueue() far free: %luB...",(unsigned long)farcoreleft() );
 #endif
}

//- DataPartysQueue class methods ---------------------------------------------------------------------------

void DataPartysQueue::Insert( DataPartysItem far *Item  )
// inserts BEFORE TAIL
// struct is taken and is add to list ( not copied! )
{
 #ifdef DEBUG
  printf("\n Inserting to DataPartysQueue...");
 #endif

 if( Item )
 {
  Tail->Last->Next=Item;

  Item->Next=Tail;
  Item->Last=Tail->Last;

  Tail->Last=Item;

  NrOfItems++;

  #ifdef DEBUG
   printf("\n  Item %Fp inserted,", Item );
  #endif
 }

 #ifdef DEBUG
  printf(" far mem: %luB...",(unsigned long)farcoreleft() );
 #endif
}


DataPartysItem far *DataPartysQueue::FindDevParty( byte WantedDevice,
						   byte WantedParty )
{
 #ifdef DEBUG
  printf("\n Searching Data devices...");
 #endif

 DataPartysItem far *At=(DataPartysItem far *)Head->Next;

 while( At!=(DataPartysItem far *)Tail )
 {
  if( At->Device==WantedDevice &&  At->Party==WantedParty )
  {
   #ifdef DEBUG
    printf(" founded... " );
   #endif

   return At;
  }
  else
   At=(DataPartysItem far *)At->Next;
 }

 #ifdef DEBUG
  printf(" not found... ");
 #endif
 return NULL;
}

word DataPartysQueue::SetPartysClean( void )
{
 MasterBootRecord BR;

 #ifdef DEBUG
  printf("\n Setting DirtyFlags in BRs to PARTY_IS_CLEAN...");
 #endif

 DataPartysItem far *At=(DataPartysItem far *)Head->Next;

 while( At!=(DataPartysItem far *)Tail )
 {
  if( LoadBoot( At->Device, At->Party, &BR ) )
  {
   printf("\n Dev %d party %d ERROR when was marked as clean... ", At->Device, At->Party );

   // go to next
   At=(DataPartysItem far *)At->Next;
   continue;
  }

  BR.BPB.DirtyFlag=PARTY_IS_CLEAN;

  if( SaveBoot( At->Device, At->Party, &BR ) )
  {
   printf("\n Dev %d party %d ERROR when was marked as clean... ", At->Device, At->Party );

   // go to next
   At=(DataPartysItem far *)At->Next;
   continue;
  }

  printf("\n Dev %d party %d marked as CLEAN... ", At->Device, At->Party );

  // go to next
  At=(DataPartysItem far *)At->Next;
 }

 #ifdef DEBUG
  printf("\n everything cleaned... ");
 #endif
 return NULL;
}

word DataPartysQueue::SetPartysDirty( void )
{
 MasterBootRecord BR;

 #ifdef DEBUG
  printf("\n Setting DirtyFlags in BRs to PARTY_IS_DIRTY...");
 #endif

 DataPartysItem far *At=(DataPartysItem far *)Head->Next;

 while( At!=(DataPartysItem far *)Tail )
 {
  if( LoadBoot( At->Device, At->Party, &BR ) )
  {
   printf("\n Dev %d party %d ERROR when was marked as dirty... ", At->Device, At->Party );

   // go to next
   At=(DataPartysItem far *)At->Next;
   continue;
  }

  BR.BPB.DirtyFlag=PARTY_IS_DIRTY;

  if( SaveBoot( At->Device, At->Party, &BR ) )
  {
   printf("\n Dev %d party %d ERROR when was marked as dirty... ", At->Device, At->Party );

   // go to next
   At=(DataPartysItem far *)At->Next;
   continue;
  }

  printf("\n Dev %d party %d marked as DIRTY... ", At->Device, At->Party );

  // go to next
  At=(DataPartysItem far *)At->Next;
 }

 #ifdef DEBUG
  printf("\n everything is dirty... ");
 #endif
 return NULL;
}

DataPartysItem far *DataPartysQueue::FindFirstParty( void )
{
 if( Head->Next==Tail )
  return NULL;
 else
  return ((DataPartysItem far *)(Head->Next));
}


DataPartysItem far *DataPartysQueue::FindNextParty(
				      DataPartysItem far *Actuall )
// returns null if there is not
{
 if( Actuall->Next==Tail )
  return NULL;
 else
  return ((DataPartysItem far *)(Actuall->Next));
}

void DataPartysQueue::Show( void )
// shows content of queue
{
 DataPartysItem far *At=(DataPartysItem far *)Head->Next;

 while( At!=(DataPartysItem far *)Tail )
 {
  printf("\n Dev %d ( 0x8%d )   Party %d",
	 At->Device,
	 At->Device,
	 At->Party
	);

  At=(DataPartysItem far *)At->Next;
 }
}

DataPartysQueue::~DataPartysQueue()
{
 // PriQueue::~PriQueue(); will be called automatically
 #ifdef DEBUG
  printf("\n DataPartys destructor ... ");
 #endif

 return;
}

//- SwapPartysQueue class methods ---------------------------------------------------------------------------

void SwapPartysQueue::Insert( SwapPartysItem far *Item  )
// inserts BEFORE TAIL
// struct is taken and is add to list ( not copied! )
{
 #ifdef DEBUG
  printf("\n Inserting to SwapPartysQueue...");
 #endif

 if( Item )
 {
  Tail->Last->Next=Item;

  Item->Next=Tail;
  Item->Last=Tail->Last;

  Tail->Last=Item;

  NrOfItems++;

  #ifdef DEBUG
   printf("\n  Item %Fp inserted,", Item );
  #endif
 }

 #ifdef DEBUG
  printf(" far mem: %luB...",(unsigned long)farcoreleft() );
 #endif
}

SwapPartysItem far *SwapPartysQueue::FindDevParty( byte WantedDevice,
						   byte WantedParty )
{
 #ifdef DEBUG
  printf("\n Searching Swap devices...");
 #endif

 SwapPartysItem far *At=(SwapPartysItem far *)Head->Next;

 while( At!=(SwapPartysItem far *)Tail )
 {
  if( At->Device==WantedDevice &&  At->Party==WantedParty )
  {
   #ifdef DEBUG
    printf(" founded...");
   #endif

   return At;
  }
  else
   At=(SwapPartysItem far *)At->Next;
 }

 #ifdef DEBUG
  printf(" not found... ");
 #endif
 return NULL;
}

word SwapPartysQueue::SetPartysClean( void )
{
 MasterBootRecord BR;

 #ifdef DEBUG
  printf("\n Setting DirtyFlags in BRs to PARTY_IS_CLEAN...");
 #endif

 SwapPartysItem far *At=(SwapPartysItem far *)Head->Next;

 while( At!=(SwapPartysItem far *)Tail )
 {
  // if error - ignore it, in boot time device will be checked
  LoadBoot( At->Device, At->Party, &BR );

  BR.BPB.DirtyFlag=PARTY_IS_CLEAN;

  SaveBoot( At->Device, At->Party, &BR );

  printf("\n Dev %d party %d marked as CLEAN... ", At->Device, At->Party );

  // go to next
  At=(SwapPartysItem far *)At->Next;
 }

 #ifdef DEBUG
  printf("\n everything cleaned... ");
 #endif
 return NULL;
}

word SwapPartysQueue::SetPartysDirty( void )
{
 MasterBootRecord BR;

 #ifdef DEBUG
  printf("\n Setting DirtyFlags in BRs to PARTY_IS_DIRTY...");
 #endif

 SwapPartysItem far *At=(SwapPartysItem far *)Head->Next;

 while( At!=(SwapPartysItem far *)Tail )
 {
  // if error - ignore it, in boot time device will be checked
  LoadBoot( At->Device, At->Party, &BR );

  BR.BPB.DirtyFlag=PARTY_IS_DIRTY;

  SaveBoot( At->Device, At->Party, &BR );

  printf("\n Dev %d party %d marked as DIRTY... ", At->Device, At->Party );

  // go to next
  At=(SwapPartysItem far *)At->Next;
 }

 #ifdef DEBUG
  printf("\n everything is dirty... ");
 #endif
 return NULL;
}

SwapPartysItem far *SwapPartysQueue::FindFirstParty( void )
{
 if( Head->Next==Tail )
  return NULL;
 else
  return ((SwapPartysItem far *)(Head->Next));
}

SwapPartysItem far *SwapPartysQueue::FindNextParty(
				      SwapPartysItem far *Actuall )
// returns null if there is not
{
 if( Actuall->Next==Tail )
  return NULL;
 else
  return ((SwapPartysItem far *)(Actuall->Next));
}

void SwapPartysQueue::Show( void )
// shows content of queue
{
 SwapPartysItem far *At=(SwapPartysItem far *)Head->Next;

 while( At!=(SwapPartysItem far *)Tail )
 {
  printf("\n Dev 0x8%d   Party %d",
	 At->Device,
	 At->Party
	);

  At=(SwapPartysItem far *)At->Next;
 }
}

SwapPartysQueue::~SwapPartysQueue()
{
 // PriQueue::~PriQueue(); will be called automatically
 #ifdef DEBUG
  printf("\n SwapPartys destructor ... ");
 #endif

 return;
}

//- class CacheDescrQueue Methods ---------------------------------------------------------------------------

void CacheDescrQueue::Insert( CacheDescrItem far *Item )
// - inserts after head
// - struct is taken and is add to list ( not copied! )
{
 // use parent function
 InsertAfterHead( (PriQueueItem far *)Item );
}


//----------------------------------------------------------------------------

CacheDescrItem far *CacheDescrQueue::FindIsInsideInterval( dword A, dword B )
// i have interval <a,b>, I am searching for intervals
// <c,d> where c<=a && b<=d. If they overlayed only partly I don't
// interest in it
{
 #ifdef DEBUG
  printf("\n Searching interval encapsulating <%lu, %lu>... ", A, B);
 #endif

 CacheDescrItem far *Node=(CacheDescrItem far *)Head->Next;

 while( Node!=Tail )
 {
  if( Node->FirstLogicalSector <= A &&
      B <= ((Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul) // -1 because <a,
    )
  {
   #ifdef DEBUG
    printf(" found <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
   #endif

   return Node;
  }
  else
  {
   #ifdef DEBUG
    printf("\n not fits <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
   #endif

   Node=(CacheDescrItem far *)Node->Next;
  }
 }

 #ifdef DEBUG
  printf(" not found... ");
 #endif
 return NULL;
}

//----------------------------------------------------------------------------

CacheDescrItem far *CacheDescrQueue::FindIsCPOver(
				      dword &A,
				      dword &B,
				      CacheDescrItem far *From // node where begin to search
						     )
// - hleda uplne nebo castecne prekryti intervalu
// - in A, B returns interval of override
// - From is included in search
{
 #ifdef DEBUG
  printf("\n CACHE: SEARCHing interval Partly/Completely over <%lu, %lu>... ", A, B);
 #endif

 CacheDescrItem far *Node=(CacheDescrItem far *)From;
 dword		L,
		H;


 while( Node!=Tail )
 {
  L=Node->FirstLogicalSector;	// low border of interval in queue
  H=((Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul); // -1 because <a,


  // test of encapsulation L..A..B..H
  if( L<=A && B<=H )
  {
   #ifdef DEBUG
    printf(" found <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
   #endif

   // A, B same because inside < L, H >

   return Node;

  }
  else
  {
   // test A..L..H..B
   if( A<=H && H<=B && A<=L && L<=B )
   {
    #ifdef DEBUG
     printf(" found <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
    #endif

    A=L;
    B=H;

    return Node;
   }
   else
   {
    // test L..A..H..B
    if( A<=H && H<=B )
    {
     #ifdef DEBUG
      printf(" found <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
     #endif

     // A the same
     B=H;

     return Node;
    }
    else
    {
     // test A..L..B..H
     if( A<=L && L<=B )
     {
      #ifdef DEBUG
       printf(" found <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
      #endif

      A=L;
      // B the same

      return Node;
     }
     else
     {
      #ifdef DEBUG
       printf("\n not fits <%lu, %lu>... ", L, H );
      #endif

      Node=(CacheDescrItem far *)Node->Next;

     }// else A..L..B..H
    } // else L..A..H..B
   }  // else A..L..H..B
  }   // else L..A..B..H

 }    // while

 #ifdef DEBUG
  printf(" not found... ");
 #endif

 return NULL;
}

//----------------------------------------------------------------------------

CacheDescrQueue::~CacheDescrQueue()
{
 // PriQueue::~PriQueue(); will be called automatically
 #ifdef DEBUG
  printf("\n CacheDescrQueue destructor... ");
 #endif

 // first i must do my own destroy in case of cache objects,
 // rest will be done by ~PriQueue();
 CacheDescrItem far *Item;

 while( Head->Next!=Tail )
 {
  Item=(CacheDescrItem far *)Head->Next;

  delete Item->CacheObject; // is better than farfree( Item->CacheObject );
			    // because was created using new

  Head->Next=Head->Next->Next;

  delete Item; // is better than farfree( Item );

  #ifdef DEBUG
   printf("\n  Item %Fp deleted,", Item );
  #endif
 }

 #ifdef DEBUG
  printf("\n  Specific CacheDescr nodes destroyed, far mem: %luB...",(unsigned long)farcoreleft() );
 #endif

 // PriQueue::~PriQueue(); will be called automatically

 return;
}

//- class SwapDescrQueue Methods ---------------------------------------------------------------------------

void SwapDescrQueue::Insert( SwapDescrItem far *Item )
// - inserts after head
// - struct is taken and is add to list ( not copied! )
{
 // use parent function
 InsertAfterHead( (PriQueueItem far *)Item );
}

//----------------------------------------------------------------------------

SwapDescrItem far *SwapDescrQueue::FindIsCPOver(
				   dword &A,
				   dword &B,
				   SwapDescrItem far *From // node where begin to search
					       )
// - hleda uplne nebo castecne prekryti intervalu
// - in A, B returns interval of override
// - From is included in search
{
 #ifdef DEBUG
  printf("\n SWAP: SEARCHing interval Partly/Completely over <%lu, %lu>... ", A, B);
 #endif

 SwapDescrItem far *Node=(SwapDescrItem far *)From;
 dword		L,
		H;


 while( Node!=Tail )
 {
  L=Node->FirstLogicalSector;	// low border of interval in queue
  H=((Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul); // -1 because <a,


  // test of encapsulation L..A..B..H
  if( L<=A && B<=H )
  {
   #ifdef DEBUG
    printf(" found <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
   #endif

   // A, B same because inside < L, H >

   return Node;

  }
  else
  {
   // test A..L..H..B
   if( A<=H && H<=B && A<=L && L<=B )
   {
    #ifdef DEBUG
     printf(" found <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
    #endif

    A=L;
    B=H;

    return Node;
   }
   else
   {
    // test L..A..H..B
    if( A<=H && H<=B )
    {
     #ifdef DEBUG
      printf(" found <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
     #endif

     // A the same
     B=H;

     return Node;
    }
    else
    {
     // test A..L..B..H
     if( A<=L && L<=B )
     {
      #ifdef DEBUG
       printf(" found <%lu, %lu>... ", Node->FirstLogicalSector, (Node->FirstLogicalSector+(dword)(Node->IntervalLng))-1ul);
      #endif

      A=L;
      // B the same

      return Node;
     }
     else
     {
      #ifdef DEBUG
       printf("\n not fits <%lu, %lu>... ", L, H );
      #endif

      Node=(SwapDescrItem far *)Node->Next;

     }// else A..L..B..H
    } // else L..A..H..B
   }  // else A..L..H..B
  }   // else L..A..B..H

 }    // while

 #ifdef DEBUG
  printf(" not found... ");
 #endif

 return NULL;
}


//----------------------------------------------------------------------------

SwapDescrQueue::~SwapDescrQueue()
{
 #ifdef DEBUG
  printf("\n SwapDescrQueue destructor ... ");
 #endif

 // PriQueue::~PriQueue(); will be called automatically
 return;
}

//- struct PackegeQueueItem Methods ---------------------------------------------------------------------------

_PackageQueueItem::_PackageQueueItem()
{
 #ifdef DEBUG
  printf("\n PackageQueueItem constructor ... ");
 #endif

 return;
}

//--------------------------------------------------------------------------------

_PackageQueueItem::~_PackageQueueItem()
{
 // CacheQueue.~CacheDescrQueue();
 // SwapQueue.~SwapDescrQueue();
 #ifdef DEBUG
  printf("\n PackageQueueItem destructor ... ");
 #endif

 return;
}

//- class PackegeQueue Methods ---------------------------------------------------------------------------

void PackageQueue::Insert( PackageQueueItem far *Item )
// - inserts after head
// - struct is taken and is add to list ( not copied! )
{
 // use parent function
 InsertAfterHead( (PriQueueItem far *)Item );
}

//----------------------------------------------------------------------------

PackageQueueItem far *PackageQueue::FindPackageID( word ID )
{
 #ifdef DEBUG
  printf("\n Searching Package ID=%u...", ID );
 #endif

 PackageQueueItem far *Node=(PackageQueueItem far *)Head->Next;

 while( Node!=Tail )
 {
  if( Node->PackageID==ID )
  {
   #ifdef DEBUG
    printf(" founded... ");
   #endif

   return Node;
  }
  else
   Node=(PackageQueueItem far *)Node->Next;
 }

 #ifdef DEBUG
  printf(" not found... ");
 #endif
 return NULL;
}

//----------------------------------------------------------------------------

void PackageQueue::DeletePack( PackageQueueItem far *DelItem )
// DelItem points to node in queue which should be deleted
{
 if( DelItem && DelItem!=Head && DelItem!=Tail )
 {
  DelItem->Last->Next=DelItem->Next;
  DelItem->Next->Last=DelItem->Last;

  // must be used delete, because program evides new instances
  // and if farfree( DelItem ); is used after return 0 of main
  // it says Null pointer assignment...
  delete DelItem;

  NrOfItems--;

  #ifdef DEBUG
   printf("\n  PackageQueueItem %Fp deleted,", DelItem );
  #endif
 }

 #ifdef DEBUG
  printf(" far mem: %luB...",(unsigned long)farcoreleft() );
 #endif
}

//----------------------------------------------------------------------------

PackageQueue::~PackageQueue()
{
 #ifdef DEBUG
  printf("\n PackageQueue destructor ... ");
 #endif

 PackageQueueItem far *Item;

 while( Head->Next!=Tail )
 {
  Item=(PackageQueueItem far *)Head->Next;
  Head->Next=Head->Next->Next;

  delete Item;

  #ifdef DEBUG
   printf("\n  Item %Fp deleted,", Item );
  #endif
 }

 #ifdef DEBUG
  printf("\n  Specific pack nodes destroyed, far mem: %luB...",(unsigned long)farcoreleft() );
 #endif

 // PriQueue::~PriQueue(); will be called automatically
 return;
}



//---------------------------------------------------------------------------------
//- STRIPING- STRIPING- STRIPING- STRIPING- STRIPING- STRIPING- STRIPING- STRIPING
//---------------------------------------------------------------------------------



void DataStripsQueue::Insert( StripItem far *Item  )
{
 // use parent function
 InsertAfterHead( (PriQueueItem far *)Item );
}



//---------------------------------------------------------------------------------



StripItem far *DataStripsQueue::FindFirstParty( void )
{
 if( Head->Next==Tail )
  return NULL;
 else
  return ((StripItem far *)(Head->Next));
}



//---------------------------------------------------------------------------------



StripItem far *DataStripsQueue::FindNextParty( StripItem far *Actuall )
// returns null if there is not
{
 if( Actuall->Next==Tail )
  return NULL;
 else
  return ((StripItem far *)(Actuall->Next));
}



//---------------------------------------------------------------------------------



bool DataStripsQueue::IsInStripingArea( byte Device, dword Logical )
// checks if LogicalHD sector is in area which is on Device striped
{
 #ifdef DEBUG
  printf("\n Searching if LogHDSec %lu is in strip area on Dev %d...", Logical, Device );
 #endif

 StripItem far *At=(StripItem far *)Head->Next;

 while( At!=(StripItem far *)Tail )
 {
  if( At->Device==Device
      &&  				// check if logical is in interval
      At->Begin<=Logical            	// which is striped
      &&
      Logical<=At->End
    )
  {
   #ifdef DEBUG
    printf(" founded: is in < %lu, %lu >... ", At->Begin, At->End );
   #endif

   return TRUE;
  }
  else
   At=(StripItem far *)At->Next;
 }

 #ifdef DEBUG
  printf(" not found... ");
 #endif
 return FALSE;

}



//---------------------------------------------------------------------------------



bool DataStripsQueue::IsStriped( byte Device, byte Party )
// checks if LogicalHD sector is in area which is on Device striped
// tests PARITY device too
{
 StripItem far *At=(StripItem far *)Head->Next;

 while( At!=(StripItem far *)Tail )
 {
  if( At->Device==Device
      &&
      At->Party==Party
    )
  {
   #ifdef DEBUG
    printf("  Dev %d Party %d IS striped ( Data )... ", At->Device, At->Party );
   #endif

   return TRUE;
  }
  else
   At=(StripItem far *)At->Next;
 }

 if( StripingSession.ParityHD.Device == Device
     &&
     StripingSession.ParityHD.Party == Party
   )
 {
   #ifdef DEBUG
    printf("  Dev %d Party %d IS striped ( Parity )... ", At->Device, At->Party );
   #endif

   return TRUE;
 }


 #ifdef DEBUG
  printf("  Dev %d Party %d IS NOT striped... ", At->Device, At->Party );
 #endif
 return FALSE;

}



//---------------------------------------------------------------------------------



void DataStripsQueue::Show( void )
// shows content of queue
{
 StripItem far *At=(StripItem far *)Head->Next;

 while( At!=(StripItem far *)Tail )
 {
  printf("\n Dev 0x8%d Party %d LogHDBeg %8lu LogHDEnd %8lu Lng %5u",
	 At->Device,
	 At->Party,
	 At->Begin,
	 At->End,
	 At->Number
	);

  At=(StripItem far *)At->Next;
 }
}



//---------------------------------------------------------------------------------



word DataStripsQueue::DeleteParty( byte Device, byte Party )
// returns 0 if deleted, else TRUE
{
 #ifdef DEBUG
  printf("\n Searching stripe devices...");
 #endif

 StripItem far *At=(StripItem far *)Head->Next;

 while( At!=(StripItem far *)Tail )
 {
  if( At->Device==Device &&  At->Party==Party )
  {
   #ifdef DEBUG
    printf(" founded => delete... " );
   #endif

   Delete( (PriQueueItem far *)At );

   return FALSE;
  }
  else
   At=(StripItem far *)At->Next;
 }

 return TRUE;
}



//---------------------------------------------------------------------------------



word DataStripsQueue::WriteSingleOffTable( byte Device, byte Party )
// writes to device table: "I am not in session..."
{
 StripItem 	Item;
 int   		Handle;
 char  	       	TableFileName[8+1+3 +1];

 TableFileName[0]= Device+'0'; // first cipher is device
 TableFileName[1]= Party+'0';  // second is party
 TableFileName[2]= 0;
 strcat( TableFileName, "STRIP.TAB" );

 unlink( TableFileName );

 // create log file
 if ((Handle = open( TableFileName, O_CREAT  | O_RDWR | O_BINARY,
				    S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't create file %s...", TableFileName );
  #endif

  return ERR_FS_LOG_NOT_CREATED;
 }

 printf("\n  %s created", TableFileName );

 // writing "OFF record"
 Item.Device= 0;
 Item.Party = 0;
 Item.Begin = 0lu;
 Item.End   = 0lu;
 Item.Number= 0lu;
 Item.Type  = 0;

 if( write( Handle, &Item, sizeof(StripItem) ) != sizeof(StripItem) )
  {
   #ifdef SEE_ERROR
    printf("\n Error writing file...");
   #endif

   return ERR_FS_LOG_WR;
  }

 close( Handle );

 return ERR_FS_NO_ERROR;
}



//---------------------------------------------------------------------------------



word DataStripsQueue::WriteOffTablesToPartys
	    ( DataPartysQueue far *DirtyDataPartys )
// - writes off tables to partys which are in system but not in session
{

 // --- CLEAN DATA DEVICES ---
 DataPartysItem far *At=(DataPartysItem far *)DataPartys->Head->Next;

 while( At!=(DataPartysItem far *)DataPartys->Tail )
 {
  // IsStriped searches for data HDs and parity HD too
  if( StripingSession.DataHDs.IsStriped( At->Device, At->Party ) )
  {
   printf("\n  Dev %d Party %d is STRIPED...",
	  At->Device,
	  At->Party
	);
  }
  else
  {
   // write OFF table
   WriteSingleOffTable( At->Device, At->Party );

   printf(" => STRIPING OFF on Dev %d Party %d...",
	  At->Device,
	  At->Party
	);
  }


  At=(DataPartysItem far *)At->Next;
 }

 // --- DIRTY DATA DEVICES ---
 At=(DataPartysItem far *)(DirtyDataPartys->Head->Next);

 while( At!=(DataPartysItem far *)DirtyDataPartys->Tail )
 {
  // IsStriped searches for data HDs and parity HD too
  if( StripingSession.DataHDs.IsStriped( At->Device, At->Party ) )
  {
   printf("\n  DirtyDev %d Party %d is STRIPED...",
	  At->Device,
	  At->Party
	);
  }
  else
  {
   // write OFF table
   WriteSingleOffTable( At->Device, At->Party );

   printf(" => STRIPING OFF on DirtyDev %d Party %d...",
	  At->Device,
	  At->Party
	);
  }


  At=(DataPartysItem far *)At->Next;
 }

 return ERR_FS_NO_ERROR;
}



//---------------------------------------------------------------------------------



word DataStripsQueue::WriteSingleTable( byte Device, byte Party )
{
 char  	       	TableFileName[8+1+3 +1];
 int   		Handle;

 TableFileName[0]= Device+'0'; // first cipher is device
 TableFileName[1]= Party+'0';  // second is party
 TableFileName[2]= 0;
 strcat( TableFileName, "STRIP.TAB" );

 unlink( TableFileName );

 // create log file
 if ((Handle = open( TableFileName, O_CREAT  | O_RDWR | O_BINARY,
				    S_IWRITE | S_IREAD 		)) == -1)
 {
  #ifdef SEE_ERROR
   printf("\n Error: cann't create file %s...", TableFileName );
  #endif

  return ERR_FS_LOG_NOT_CREATED;
 }

 printf("\n  %s created", TableFileName );



 // write DATA devices to file
 StripItem far *At=(StripItem far *)Head->Next;

 while( At!=((StripItem far *)(StripingSession.DataHDs.Tail)) )
 {
  if( write( Handle, At, sizeof(StripItem) ) != sizeof(StripItem) )
  {
   #ifdef SEE_ERROR
    printf("\n Error writing file...");
   #endif

   return ERR_FS_LOG_WR;
  }
  else
   printf("\n   => ADD DATA:   Dev 0x8%d Party %d LogHDBeg %7lu LogHDEnd %7lu Lng %5u",
	   At->Device,
	   At->Party,
	   At->Begin,
	   At->End,
	   At->Number
	 );


  // take next
  At=(StripItem far *)At->Next;
 }



 // write PARITY device to file
  if( write( Handle, &(StripingSession.ParityHD), sizeof(StripItem) ) != sizeof(StripItem) )
  {
   #ifdef SEE_ERROR
    printf("\n Error writing file...");
   #endif

   return ERR_FS_LOG_WR;
  }
  else
   printf("\n   => ADD PARITY: Dev 0x8%d Party %d LogHDBeg %7lu LogHDEnd %7lu Lng %5u",
	   StripingSession.ParityHD.Device,
	   StripingSession.ParityHD.Party,
	   StripingSession.ParityHD.Begin,
	   StripingSession.ParityHD.End,
	   StripingSession.ParityHD.Number
	 );

 close( Handle );

 return ERR_FS_NO_ERROR;
}



//---------------------------------------------------------------------------------



word DataStripsQueue::WriteTablesToPartys( void )
// - writes session queue to partys, where it should be written
// - goes through queue
{

 // write table to DATA devices
 StripItem far *At=(StripItem far *)Head->Next;

 while( At!=((StripItem far *)(StripingSession.DataHDs.Tail)) )
 {
  WriteSingleTable( At->Device, At->Party );

  // take next
  At=(StripItem far *)At->Next;
 }

 // write table to PARITY device ( if valid )
 if( StripingSession.ParityHD.Number!=0 )
 {
  WriteSingleTable( StripingSession.ParityHD.Device,
		    StripingSession.ParityHD.Party
		  );
 }

 return ERR_FS_NO_ERROR;
}



//---------------------------------------------------------------------------------



DataStripsQueue::~DataStripsQueue()
{
 // PriQueue::~PriQueue(); will be called automatically
 #ifdef DEBUG
  printf("\n DataStripsQueue destructor ... ");
 #endif

 return;
}

//- EOF ---------------------------------------------------------------------------
