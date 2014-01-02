#ifndef __CACHESUP_H
 #define __CACHESUP_H


 #include <alloc.h>
 #include <process.h>
 #include <stdio.h>

 #include "fstypes.h"
 #include "fdisk.h"


 //---------------------------------------------------------------------------------

 class far BMP
 {
  public:
   byte far *Bmp;
   long Length;		// in bits, value signed, length of bitmap
   long Free;          	// nr of bits not set, ma smysl pouze kdyz se SetInterval nenastavuje bity jez nastavene

   BMP( long BitLength );
    void SetInterval( long FirstBit, long LastBit );
    void ClearInterval( long FirstBit, long LastBit );
    byte BitIsSet( long Bit );
	 // - bits are counted from 0 ( Bit==6 means test seventh bit )
    byte FindHole( long From, long &FirstBit, long &HoleLength );
    byte FindAllocSecHole( word From, word &FirstBit, word WantedLength );
   ~BMP();
 };

 //---------------------------------------------------------------------------------

 class far PackageIDMan	// manages unic PackageIDs
 {
  private:

   BMP far *IDs;                      // 1000B --> 8000 packages

  public:

   PackageIDMan();
    word GetID ( void );
    void FreeID( word PackageID );
   ~PackageIDMan();
 };

 //---------------------------------------------------------------------------------

 struct far _PriQueueItem
 {
  _PriQueueItem far *Next,
		far *Last;
 };

 typedef struct far _PriQueueItem PriQueueItem;


 class far PriQueue
 {
  public:
   PriQueueItem far *Head;
   PriQueueItem far *Tail;	        // service head and tail are empty
					// nodes
   word 	NrOfItems;		// data items ( Head, Tail not in )

   PriQueue();
    void         InsertAfterHead( PriQueueItem far *InsertedItem  ); // InsertedItem is inserted => not copied!
    void         Delete( PriQueueItem far *DelItem );           // DelItem points to node in queue which should be deleted
    PriQueueItem far *Next( PriQueueItem far *Item );
    PriQueueItem far *Last( PriQueueItem far *Item );
    PriQueueItem far *GetHead( void );
    PriQueueItem far *GetTail( void );
    void         Destroy( void );       // destroys every data node

   ~PriQueue();

 };

 //---------------------------------------------------------------------------------

 struct far _SwapPartysItem : PriQueueItem
 {
  byte  Device;		// 0,..
  byte  Party;          // 1,..
 };

 typedef struct far _SwapPartysItem SwapPartysItem;


 class far SwapPartysQueue : public PriQueue
 {
  public:

   SwapPartysQueue( void ):PriQueue() { return; };
    void           Insert( SwapPartysItem far *Item  ); // inserts after head
							// struct is taken and is add to list ( not copied! )
    SwapPartysItem far *FindDevParty( byte WantedDevice, byte WantedParty );
    word           SetPartysClean( void );
    word           SetPartysDirty( void );

    SwapPartysItem far *FindFirstParty( void ); // returns null if there is not
    SwapPartysItem far *FindNextParty( SwapPartysItem far *Actuall ); // returns null if there is not

    void	   Show( void );

   ~SwapPartysQueue();
 };

 //---------------------------------------------------------------------------------

 struct far _DataPartysItem : PriQueueItem
 {
  byte                Device;	// 0,..
  byte                Party;    // 1,..
  SwapPartysItem far *Swap;	// where this device swaps
 };

 typedef struct far _DataPartysItem DataPartysItem;


 class far DataPartysQueue : public PriQueue
 {
  public:

   DataPartysQueue( void ):PriQueue() { return; };
    void           Insert( DataPartysItem far *Item  ); // inserts after head
							// struct is taken and is add to list ( not copied! )
    DataPartysItem far *FindDevParty( byte WantedDevice, byte WantedParty );
    word           SetPartysClean( void );
    word           SetPartysDirty( void );

    DataPartysItem far *FindFirstParty( void ); // returns null if there is not
    DataPartysItem far *FindNextParty( DataPartysItem far *Actuall ); // returns null if there is not

    void	   Show( void );

   ~DataPartysQueue();
 };


 //- Cache --------------------------------------------------------------------------------

 struct far _CacheDescrItem : PriQueueItem
 {
  dword FirstLogicalSector;
  word  IntervalLng;		// default == 1
  byte  Dirty;
  byte  far *CacheObject;
 };

 typedef struct far _CacheDescrItem CacheDescrItem;


 class far CacheDescrQueue : public PriQueue
 {
  public:

   CacheDescrQueue():PriQueue() { return; };
    void           Insert( CacheDescrItem far *Item );
    // - inserts after head
    // - struct is taken and is add to list ( not copied! )
    CacheDescrItem far *FindIsInsideInterval( dword A, dword B );
    CacheDescrItem far *FindIsCPOver( dword &A, dword &B,
				      CacheDescrItem far *From // node where begin to search
				    );
    // - From is included in search
    // - hleda uplne nebo castecne prekryti intervalu a vraci v A a B prekryti
   ~CacheDescrQueue();
 };


 //- Swap --------------------------------------------------------------------------------

 struct far _SwapDescrItem : PriQueueItem
 {
  dword    FirstLogicalSector;
  word     IntervalLng;		// default == 1
  dword    LogicalInSwap;
 };

 typedef struct far _SwapDescrItem SwapDescrItem;


 class far SwapDescrQueue : public PriQueue
 {
  public:

   SwapDescrQueue():PriQueue() { return; };
    void Insert( SwapDescrItem far *Item );
    // - inserts after head
    // - struct is taken and is add to list ( not copied! )
    SwapDescrItem far *FindIsCPOver( dword &A, dword &B,
				     SwapDescrItem far *From // node where begin to search
				    );
    // - From is included in search
    // - hleda uplne nebo castecne prekryti intervalu a vraci v A a B prekryti
   ~SwapDescrQueue();
 };

 //- Package --------------------------------------------------------------------------------

 struct far _PackageQueueItem : PriQueueItem
 {
  word             PackageID;
  byte             Device;        // 0..
  byte             Party;
  byte             Dirty;	  // somebody dirty in cache queue
  CacheDescrQueue  CacheQueue;
  SwapDescrQueue   SwapQueue;
  SwapPartysItem   far *SwapsTo;

  _PackageQueueItem();
  ~_PackageQueueItem();

 };

 typedef far _PackageQueueItem PackageQueueItem;

 class far PackageQueue : public PriQueue
 {
  public:

   PackageQueue( void ):PriQueue() { return; };
    void             Insert( PackageQueueItem far *Item ); // inserts after head
	 // struct is taken and is add to list ( not copied! )
    PackageQueueItem far*FindPackageID( word ID );
    void             DeletePack( PackageQueueItem far *DelItem );
   ~PackageQueue();
 };

 //---------------------------------------------------------------------------------

 // Main structure of cache => CacheMan

 struct far _CacheManStruct
 {
  dword           MemQuote,	// memory in bytes reserved for cache
		  MemUsed,
		  MemRest;

  byte		  SwapOn;   	// ON/OFF
  byte		  CacheOn;

  PackageIDMan    PackIDs;
  PackageQueue    PackQueue;
 };

 typedef far _CacheManStruct CacheManStruct;


 //---------------------------------------------------------------------------------
 //- STRIPING STRUCTURES - STRIPING STRUCTURES - STRIPING STRUCTURES -
 //---------------------------------------------------------------------------------

 struct _StripItem : PriQueueItem
 {
  byte  Device;
  byte  Party;
  dword Begin; 	// logical HD sector 0..
  dword End; 	// logical HD sector 0..
  dword Number; // End-Begin+1 ( data devices )
  byte  Type;   // PARITY_STRIP || DATA_STRIP - used for table on disk
 };

 typedef struct far _StripItem StripItem;



 class far DataStripsQueue : public PriQueue
 {
  public:

   DataStripsQueue( void ):PriQueue() { return; };
    void      Insert( StripItem far *Item  );

    StripItem far *FindFirstParty( void );
    StripItem far *FindNextParty( StripItem far *Actuall );
    bool      IsInStripingArea( byte Device, dword Logical );
    void      Show( void );
    word      DeleteParty( byte Device, byte Party );

    word      WriteSingleTable( byte Device, byte Party );
    word      WriteTablesToPartys( void );
	      // these two functions would be methods of StripSession
	      // but here is easier implementation

    bool      IsStriped( byte Device, byte Party ); // checks ParityHD too
    word      WriteSingleOffTable( byte Device, byte Party );
    word      WriteOffTablesToPartys( DataPartysQueue far * DirtyDataPartys );

   ~DataStripsQueue();
 };



 struct far _StripSession
 {

  byte StripingIs;		// ON / OFF

  DataStripsQueue DataHDs;	// data HDs queue

  StripItem       ParityHD;     // parity HD

 };

 typedef struct far _StripSession StripSession; // static structure in stripe.cpp

#endif