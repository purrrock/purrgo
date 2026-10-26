#ifndef PURRGO_DISK_H
#define PURRGO_DISK_H

/*
 * Платформенные функции диска FatFs. diskio.c только передаёт им вызовы
 * FatFs (диск один, номер 0); реализует их sd_card.c проекта.
 */

#include "diskio.h"

DSTATUS purrgo_disk_status(void);
DSTATUS purrgo_disk_initialize(void);
DRESULT purrgo_disk_read(BYTE *buff, DWORD sector, UINT count);
DRESULT purrgo_disk_write(const BYTE *buff, DWORD sector, UINT count);
DRESULT purrgo_disk_ioctl(BYTE cmd, void *buff);

#endif /* PURRGO_DISK_H */
