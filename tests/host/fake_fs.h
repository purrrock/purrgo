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
const uint8_t* fake_fs_data(const char* name, uint32_t* size);
int fake_fs_open_handles(void);
#endif
