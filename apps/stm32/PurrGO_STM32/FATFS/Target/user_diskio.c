/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    user_diskio.c
  * @brief   FatFs USER disk driver adapter for the SD card.
  ******************************************************************************
  *
  * This file connects the CubeMX-generated FatFs USER driver
  * to the actual SD card driver implemented in fatfs_sd.c.
  *
  * The physical SD card driver provides the following functions:
  *
  *   SD_disk_initialize()
  *   SD_disk_status()
  *   SD_disk_read()
  *   SD_disk_write()
  *   SD_disk_ioctl()
  *
  * USER_Driver exposes these functions to FatFs.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/

#include "ff_gen_drv.h"
#include "fatfs_sd.h"

/*
 * CubeMX/FatFs expects a Diskio_drvTypeDef object named USER_Driver.
 *
 * We do not implement another SD driver here.
 * Instead, USER_Driver directly references the functions
 * from fatfs_sd.c.
 */
Diskio_drvTypeDef USER_Driver =
{
    SD_disk_initialize,
    SD_disk_status,
    SD_disk_read,
#if _USE_WRITE == 1
    SD_disk_write,
#endif
#if _USE_IOCTL == 1
    SD_disk_ioctl,
#endif
};