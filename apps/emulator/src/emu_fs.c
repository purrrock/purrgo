#include "emu_fs.h"
#include <purrgo/fs_hal.h>

/*
 * FS adapter для emulator.
 *
 * map parser работает через абстракцию purrgo_fs_t, поэтому ему не
 * требуется знать, что в emulator файлы открываются через host FS.
 */
uint32_t emu_fs_read(
    void* handle,
    void* buffer,
    uint32_t size
) {
    return purrgo_fs_read(
        (purrgo_file_t*)handle,
        (uint8_t*)buffer,
        size
    );
}

bool emu_fs_seek(
    void* handle,
    uint32_t offset
) {
    return purrgo_fs_seek(
        (purrgo_file_t*)handle,
        offset
    );
}
