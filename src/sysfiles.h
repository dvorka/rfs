#ifndef __SYSFILES_H
 #define __SYSFILES_H

 /*
  group info.system:
   8    magic words "GROUPINF"
   4	nr of groups on volume
   ..
	list of GroupInfo structures
   ..
 */

 struct GroupInfo
 {
  word FreeSpace;	// in sectors
  word MaxGap;		// in sectors
  word MinGap;		// in sectors
  word BadBlockCount;	// in sectors
 }


 /*
  group info.system:
   8    magic words "BADBLOCK"
   4	nr of badblocks on volume
   ..
   4    list of badblocks   [ group, offset ]
   ..
 */





#endif
