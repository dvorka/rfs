//------------------------------------------------------------------------------------
//
// 			File of FS types and helpers
//
//------------------------------------------------------------------------------------


#include "fstypes.h"

//- Help structures, defs, ... -----------------------------------------------------


byte BitMask[16]={ 0x0001, 0x0002, 0x0004, 0x0008,
		   0x0010, 0x0020, 0x0040, 0x0080,
		   0x0100, 0x0200, 0x0400, 0x0800,
		   0x1000, 0x2000, 0x4000, 0x8000
		 };


//- global variables -----------------------------------------------------------------


int  HDHandle[5],	// handles to HDs, if == 0xFFFF => handle not used

     ActiveHDHandle=0xFFFF;	// it's handle to actuall hard, init in init.cpp

bool InStripingMode=FALSE;	// true if fs stripes

struct ActiveDevice ActiveDev;

//- EOF ------------------------------------------------------------------------------
