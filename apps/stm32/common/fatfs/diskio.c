/*-----------------------------------------------------------------------*/
/* Низкоуровневый ввод-вывод диска для FatFs R0.12c (PurrGO, U585).      */
/*                                                                       */
/* Диск один (pdrv = 0). Вызовы передаются платформенным функциям        */
/* purrgo_disk_*() (см. purrgo_disk.h), которые реализует sd_card.c.     */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
#include "purrgo_disk.h"

DSTATUS disk_status(BYTE pdrv)
{
    return (pdrv == 0U) ? purrgo_disk_status() : (DSTATUS)STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    return (pdrv == 0U) ? purrgo_disk_initialize() : (DSTATUS)STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    return (pdrv == 0U) ? purrgo_disk_read(buff, sector, count) : RES_PARERR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    return (pdrv == 0U) ? purrgo_disk_write(buff, sector, count) : RES_PARERR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    return (pdrv == 0U) ? purrgo_disk_ioctl(cmd, buff) : RES_PARERR;
}
