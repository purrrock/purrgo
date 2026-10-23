#ifndef FAKE_FS_H
#define FAKE_FS_H
#include <stdint.h>
typedef struct {
    int fail_write, fail_sync, fail_close, fail_replace;
    int fail_open_append, fail_open_all;
    uint32_t partial_write_bytes;   /* однократная частичная запись, затем fail_write_next_full */
    int fail_write_next_full;
} fake_fs_faults_t;
extern fake_fs_faults_t g_fault;
void fake_fs_reset(void);
void fake_fs_put(const char* name, const char* text);
void fake_fs_put_bin(const char* name, const uint8_t* data, uint32_t size);
const uint8_t* fake_fs_data(const char* name, uint32_t* size);
int fake_fs_open_handles(void);
int fake_fs_open_calls(void);   /* число вызовов purrgo_fs_open с момента fake_fs_reset() */
int fake_fs_sync_calls(void);   /* число вызовов purrgo_fs_sync с момента fake_fs_reset() */
/* Статистика чтения (сбрасывается fake_fs_reset() и fake_fs_reset_read_stats()). */
uint32_t fake_fs_read_calls(void);      /* число вызовов purrgo_fs_read */
uint32_t fake_fs_max_read_end(void);    /* наибольшая позиция файла, до которой дошло чтение */
void fake_fs_reset_read_stats(void);
#endif
