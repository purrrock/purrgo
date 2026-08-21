#include "purrgo/fs_hal.h"

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

/*
 * PC-specific implementation of the opaque purrgo_dir_t object.
 *
 * The portable HAL exposes purrgo_dir_t without defining its contents.
 * On the PC we need to keep both:
 *
 *   1. DIR*  - native directory handle used by readdir()/closedir()
 *   2. path  - path of the opened directory
 *
 * The path is required because we do not rely on struct dirent::d_type.
 * Instead, stat() is used to determine whether an entry is a directory.
 */
struct purrgo_dir_s {
    DIR* dir;
    char path[PURRGO_FS_MAX_PATH];
};


purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode) {
    const char* c_mode = "wb";

    if (mode == FS_READ) {
        c_mode = "rb";
    } else if (mode == FS_WRITE_APPEND) {
        c_mode = "ab";
    }

    /*
     * FILE* is an operating-system/CRT-specific type.
     * The portable HAL exposes it as purrgo_file_t*.
     */
    return (purrgo_file_t*)fopen(filepath, c_mode);
}


uint32_t purrgo_fs_read(
    purrgo_file_t* file,
    uint8_t* buffer,
    uint32_t size
) {
    if (!file || !buffer) {
        return 0;
    }

    /*
     * fread() uses size_t, while the PurrGo HAL uses uint32_t.
     *
     * The explicit casts keep the HAL interface independent of the
     * host architecture. On a 64-bit PC size_t is typically 64-bit,
     * while the PurrGo HAL intentionally uses uint32_t.
     */
    return (uint32_t)fread(
        buffer,
        1,
        (size_t)size,
        (FILE*)file
    );
}


uint32_t purrgo_fs_write(
    purrgo_file_t* file,
    const uint8_t* data,
    uint32_t size
) {
    if (!file || !data) {
        return 0;
    }

    return (uint32_t)fwrite(
        data,
        1,
        (size_t)size,
        (FILE*)file
    );
}


bool purrgo_fs_seek(
    purrgo_file_t* file,
    uint32_t offset
) {
    if (!file) {
        return false;
    }

    return fseek(
        (FILE*)file,
        (long)offset,
        SEEK_SET
    ) == 0;
}


void purrgo_fs_sync(purrgo_file_t* file) {
    if (file) {
        fflush((FILE*)file);
    }
}


void purrgo_fs_close(purrgo_file_t* file) {
    if (file) {
        fclose((FILE*)file);
    }
}


/*
 * Open a directory.
 *
 * Unlike the previous implementation, we cannot simply cast DIR*
 * to purrgo_dir_t* because the PC implementation also needs to retain
 * the directory path for stat().
 */
purrgo_dir_t* purrgo_fs_opendir(const char* path) {
    if (!path) {
        return NULL;
    }

    DIR* system_dir = opendir(path);

    if (!system_dir) {
        return NULL;
    }

    /*
     * purrgo_dir_t is opaque in the public HAL.
     * Its actual definition is private to this PC implementation.
     */
    purrgo_dir_t* dir =
        (purrgo_dir_t*)malloc(sizeof(purrgo_dir_t));

    if (!dir) {
        closedir(system_dir);
        return NULL;
    }

    dir->dir = system_dir;

    /*
     * Store the directory path.
     *
     * This path will later be combined with the entry name and passed
     * to stat().
     */
    int written = snprintf(
        dir->path,
        sizeof(dir->path),
        "%s",
        path
    );

    /*
     * Do not keep a truncated path.
     */
    if (written < 0 ||
        (size_t)written >= sizeof(dir->path)) {

        closedir(system_dir);
        free(dir);

        return NULL;
    }

    return dir;
}


/*
 * Read one directory entry.
 *
 * IMPORTANT:
 *
 * We deliberately do NOT use:
 *
 *     ep->d_type
 *
 * because d_type is not guaranteed to be present in struct dirent
 * provided by every platform/toolchain.
 *
 * Instead, the complete path is constructed and stat() is used to
 * determine the actual filesystem object type.
 */
bool purrgo_fs_readdir(
    purrgo_dir_t* dir,
    purrgo_fs_dirent_t* dirent
) {
    if (!dir || !dir->dir || !dirent) {
        return false;
    }

    struct dirent* ep = readdir(dir->dir);

    /*
     * readdir() returns NULL when there are no more entries.
     */
    if (!ep) {
        return false;
    }

    /*
     * Copy the filesystem entry name into the platform-independent
     * PurrGo directory entry structure.
     *
     * The destination buffer has a fixed size, so the copy is explicitly
     * bounded and always terminated with '\0'.
     */
    strncpy(
        dirent->name,
        ep->d_name,
        sizeof(dirent->name) - 1
    );

    dirent->name[sizeof(dirent->name) - 1] = '\0';


    /*
     * Build the complete path:
     *
     *     <opened directory>/<entry name>
     *
     * Example:
     *
     *     ./maps/frankfurt
     */
    char full_path[PURRGO_FS_MAX_PATH];

    int written = snprintf(
        full_path,
        sizeof(full_path),
        "%s/%s",
        dir->path,
        dirent->name
    );

    /*
     * If the complete path does not fit into the HAL buffer,
     * we cannot reliably determine its type.
     *
     * The entry itself is still returned to the caller, but it is
     * explicitly classified as not being a directory.
     */
    if (written < 0 ||
        (size_t)written >= sizeof(full_path)) {

        dirent->is_directory = false;

        return true;
    }


    /*
     * Determine the filesystem object type using stat().
     *
     * This replaces the non-portable ep->d_type access.
     */
    struct stat st;

    if (stat(full_path, &st) == 0) {
        dirent->is_directory = S_ISDIR(st.st_mode);
    } else {
        /*
         * stat() failed, so the object type could not be determined.
         *
         * Do not guess that it is a directory.
         */
        dirent->is_directory = false;
    }

    return true;
}


/*
 * Close a directory opened by purrgo_fs_opendir().
 */
void purrgo_fs_closedir(purrgo_dir_t* dir) {
    if (!dir) {
        return;
    }

    if (dir->dir) {
        closedir(dir->dir);
    }

    free(dir);
}