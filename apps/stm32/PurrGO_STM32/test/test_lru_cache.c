#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

// Mock definitions
#define FR_OK 0
typedef int FRESULT;
typedef uint32_t FSIZE_t;
typedef uint32_t UINT;
typedef uint8_t BYTE;

typedef struct {
    uint32_t objsize;
} FATFS_OBJ;

typedef struct {
    FSIZE_t fptr;
    FATFS_OBJ obj;
    char path[256];
} FIL;

typedef struct { int dummy; } DIR;
typedef struct { char fname[13]; BYTE fattrib; } FILINFO;

#define f_tell(fp) ((fp)->fptr)
#define f_size(fp) ((fp)->obj.objsize)

static int fake_disk_reads = 0;
static char fake_disk_content[4096];

FRESULT f_lseek(FIL* fp, FSIZE_t ofs) {
    fp->fptr = ofs;
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, UINT btr, UINT* br) {
    fake_disk_reads++;
    if (fp->fptr >= fp->obj.objsize) { *br = 0; return FR_OK; }
    UINT bytes = btr;
    if (fp->fptr + bytes > fp->obj.objsize) { bytes = fp->obj.objsize - fp->fptr; }
    memcpy(buff, fake_disk_content + fp->fptr, bytes);
    fp->fptr += bytes;
    *br = bytes;
    return FR_OK;
}

FRESULT f_open(FIL* fp, const char* path, BYTE mode) {
    strcpy(fp->path, path);
    fp->fptr = 0;
    fp->obj.objsize = 4096; // Fake size
    return FR_OK;
}

FRESULT f_write(FIL* fp, const void* buff, UINT btw, UINT* bw) {
    *bw = btw;
    return FR_OK;
}

FRESULT f_sync(FIL* fp) { return FR_OK; }
FRESULT f_close(FIL* fp) { return FR_OK; }
FRESULT f_opendir(DIR* dp, const char* path) { return FR_OK; }
FRESULT f_readdir(DIR* dp, FILINFO* fno) { return FR_OK; }
FRESULT f_closedir(DIR* dp) { return FR_OK; }

#define PURRGO_LOG(...)

#define FA_READ 1
#define FA_OPEN_EXISTING 2
#define FA_WRITE 3
#define FA_CREATE_ALWAYS 4
#define FA_OPEN_ALWAYS 5
#define FA_OPEN_APPEND 6
#define AM_DIR 0x10

int purrgo_snprintf(char* buf, size_t size, const char* fmt, ...) { return 0; }
void purrgo_logger_write(const char *fmt, ...) {}

// Temporarily include and mock dependencies
#define STM32_TEST_COMPILE

// Modify the code minimally just for compilation
#define FATFS_H_FAKE
#include "../../../../include/purrgo/fs_hal.h"
#include "fs_hal_stm32_test.c"

int main() {
    for (int i = 0; i < 4096; i++) {
        fake_disk_content[i] = i % 256;
    }

    // Test 1: Open a .db file
    purrgo_file_t* db_file = purrgo_fs_open("test.db", FS_READ);
    assert(db_file != NULL);

    uint8_t buf[100];
    fake_disk_reads = 0;
    uint32_t r = purrgo_fs_read(db_file, buf, 10);
    assert(r == 10);
    assert(fake_disk_reads == 1);

    r = purrgo_fs_read(db_file, buf, 10);
    assert(r == 10);
    assert(fake_disk_reads == 1); // cache hit

    purrgo_fs_close(db_file);

    // Test 2: Open a .txt file (any other extension)
    purrgo_file_t* txt_file = purrgo_fs_open("test.txt", FS_READ);
    assert(txt_file != NULL);

    fake_disk_reads = 0;
    r = purrgo_fs_read(txt_file, buf, 10);
    assert(r == 10);
    assert(fake_disk_reads == 1);

    r = purrgo_fs_read(txt_file, buf, 10);
    assert(r == 10);
    assert(fake_disk_reads == 1); // cache hit

    // Test write invalidation
    purrgo_fs_write(txt_file, buf, 10);
    r = purrgo_fs_read(txt_file, buf, 10);
    assert(r == 10);
    assert(fake_disk_reads == 2); // invalidated

    purrgo_fs_close(txt_file);

    printf("All cache tests passed!\n");
    return 0;
}
