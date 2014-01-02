#ifndef __INIT_H
 #define __INIT_H


 #include <alloc.h>
 #include <dos.h>
 #include <fcntl.h>
 #include <io.h>
 #include <process.h>
 #include <sys\stat.h>
 #include <stdio.h>

 #include "cacheman.h"
 #include "cachesup.h"
 #include "format.h"
 #include "fstypes.h"
 #include "fserrors.h"
 #include "hardware.h"
 #include "recovery.h"

 #define  INIT_WRITE_TABLE              TRUE
 #define  INIT_DO_NOT_WRITE_TABLE       FALSE
 #define  INIT_CREATE_DEVICES           TRUE
 #define  INIT_DO_NOT_CREATE_DEVICES    FALSE


 word LoadDeviceTable( struct DeviceTabEntry *Device );

 word CreateONEDeviceUsingDeviceTable( byte Index,
				       struct DeviceTabEntry *Table );
 // creates ONE new device ( others devices stays unchanged

 word InitializeSimulation( bool WriteTable=FALSE, bool CreateDevices=FALSE );

 word FSOpenFileSystem( bool SwappingON,
			bool CachingON,  dword CacheMem=20000lu );

      // opens, checks dirty, tra, repairing, inits striping, ...

      // opens devices
      // inits cache

 word FSShutdownFileSystem( void );

#endif
