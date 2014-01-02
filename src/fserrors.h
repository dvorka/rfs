#ifndef __FSERRORS_H
 #define __FSERRORS_H


 // ERR_FS...
 // some are defined in CACHEMAN.H

 enum FSErrors
 {
  ERR_FS_NO_ERROR,

  ERR_FS_UNDEFINED_ERROR,
  ERR_FS_FATAL_ERROR,

  ERR_FS_RD_LOGICAL_SECTOR,
  ERR_FS_WR_LOGICAL_SECTOR,
  ERR_FS_VERIFY_FAILED,

  ERR_FS_BAD_GROUP_BMP_SECTOR,

  ERR_FS_NOT_USED,

  ERR_FS_BAD_GRP_SEC

 };


 // carry flag is set to 1, if ok is set to 0
 // error code in AH
 enum BIOSInt13Errors
 {
  ERR_BIOS13_NO_ERROR,
  ERR_BIOS13_BAD_COMMAND,
  ERR_BIOS13_BAD_ADDR_MARK,
  ERR_BIOS13_WRITE_PROTECT,
  ERR_BIOS13_SECTOR_ID_BAD_OR_NOT_FOUND,
  ERR_BIOS13_RESET_FILED,
  ERR_BIOS13_DISKETTE_CHANGED_REMOVED,
  ERR_BIOS13_BAD_FIX_DISK_PARAM_TAB,
  ERR_BIOS13_DMA_FAILURE,
  ERR_BIOS13_DMA_OVERRUN,
  ERR_BIOS13_BAD_FIX_DISK_SECT_FLAG,
  ERR_BIOS13_BAD_TRACK_FLAG,
  ERR_BIOS13_INVALID_NR_OF_SECTORS,
  ERR_BIOS13_ADDR_MARK_DETECT,
  ERR_BIOS13_DMA_ARBIT_OUT_OF_RANGE,
  ERR_BIOS13_BAD_CRC,
  ERR_BIOS13_DATA_CORRECTED,
  ERR_BIOS13_CONTROLLED_FAILURE=0x20,
  ERR_BIOS13_BAD_SEEK=0x40,
  ERR_BIOS13_TIME_OUT=0x80,
  ERR_BIOS13_DRIVE_NOT_READY=0xAA,
  ERR_BIOS13_UNDEFINED_ERROR=0xBB,
  ERR_BIOS13_WRITE_FAULT=0xCC,
  ERR_BIOS13_STATUS_ERR_0=0xE0,
  ERR_BIOS13_SENSE_OPERATION_FAILED=0xFF
 };

#endif