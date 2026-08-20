#ifndef EMU_FS_H
#define EMU_FS_H

#include <stdint.h>
#include <stdbool.h>

uint32_t emu_fs_read(
    void* handle,
    void* buffer,
    uint32_t size
);

bool emu_fs_seek(
    void* handle,
    uint32_t offset
);

#endif /* EMU_FS_H */
