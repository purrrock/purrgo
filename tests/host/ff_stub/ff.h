/*
 * Заглушка FatFs для host-теста stm32/port/storage/fs_hal_stm32.c (кэш секторов, read-ahead).
 * Содержит только то, что использует fs_hal_stm32.c. Реализация (файлы в памяти и счётчики
 * обращений к "SD") - в tests/host/fs_cache_stm32_check.c.
 */
#ifndef FF_STUB_H
#define FF_STUB_H

#include <stdint.h>

typedef unsigned char BYTE;
typedef uint32_t DWORD;
typedef unsigned int UINT;
typedef uint32_t FSIZE_t;
typedef char TCHAR;

typedef enum {
    FR_OK = 0, FR_DISK_ERR, FR_INT_ERR, FR_NOT_READY, FR_NO_FILE, FR_NO_PATH, FR_INVALID_NAME,
    FR_DENIED, FR_EXIST, FR_INVALID_OBJECT, FR_WRITE_PROTECTED, FR_INVALID_DRIVE, FR_NOT_ENABLED,
    FR_NO_FILESYSTEM, FR_MKFS_ABORTED, FR_TIMEOUT, FR_LOCKED, FR_NOT_ENOUGH_CORE,
    FR_TOO_MANY_OPEN_FILES, FR_INVALID_PARAMETER
} FRESULT;

typedef struct {
    struct { void *fs; } obj;
    BYTE flag;
    DWORD *cltbl;
    FSIZE_t fptr;
    FSIZE_t fsize;
    int stub_file;      /* индекс файла в таблице теста */
} FIL;

typedef struct { int unused; } DIR;
typedef struct { BYTE fattrib; char fname[256]; } FILINFO;

#define FA_READ           0x01
#define FA_WRITE          0x02
#define FA_OPEN_EXISTING  0x00
#define FA_CREATE_ALWAYS  0x08
#define FA_OPEN_ALWAYS    0x10
#define FA_OPEN_APPEND    0x30
#define AM_DIR            0x10
#define CREATE_LINKMAP    ((FSIZE_t)0 - 1)

#define f_tell(fp) ((fp)->fptr)
#define f_size(fp) ((fp)->fsize)

FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode);
FRESULT f_close(FIL *fp);
FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br);
FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw);
FRESULT f_lseek(FIL *fp, FSIZE_t ofs);
FRESULT f_sync(FIL *fp);
FRESULT f_unlink(const TCHAR *path);
FRESULT f_rename(const TCHAR *old_name, const TCHAR *new_name);
FRESULT f_stat(const TCHAR *path, FILINFO *fno);
FRESULT f_opendir(DIR *dp, const TCHAR *path);
FRESULT f_readdir(DIR *dp, FILINFO *fno);
FRESULT f_closedir(DIR *dp);

#endif
