#ifndef __FORMAT_H
 #define __FORMAT_H

 #include <string.h>

 #include "fdisk.h"
 #include "fstypes.h"
 #include "group.h"
 #include "hardware.h"

 word  FSFormat( byte Device, byte PartitionID, byte Type=RFS_DATA );


#endif
