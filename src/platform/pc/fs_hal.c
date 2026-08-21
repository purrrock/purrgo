#include "purrgo/fs_hal.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>

purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode) {
    const char* c_mode = "wb";
    if (mode == FS_READ) {
        c_mode = "rb";
    } else if (mode == FS_WRITE_APPEND) {
        c_mode = "ab";
    }
    // Приведение типа указателя FILE* ОС к платформонезависимому purrgo_file_t*
    return (purrgo_file_t*)fopen(filepath, c_mode);
}

uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) {
    if (!file || !buffer) return 0;
    /*
     * Явное приведение аргументов и возвращаемого значения (size_t <-> uint32_t).
     * Стандартные функции fread/fwrite возвращают size_t (64-бит на x64).
     * На платформе STM32 FatFs оперирует типом UINT (32-бит).
     * Приведение типов гарантирует идентичное поведение кода на ПК и микроконтроллере.
     */
    return (uint32_t)fread(buffer, 1, (size_t)size, (FILE*)file);
}

uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, uint32_t size) {
    if (!file || !data) return 0;
    return (uint32_t)fwrite(data, 1, (size_t)size, (FILE*)file);
}

bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) {
    if (!file) return false;
    return fseek((FILE*)file, (long)offset, SEEK_SET) == 0;
}

void purrgo_fs_sync(purrgo_file_t* file) {
    if (file) fflush((FILE*)file);
}

void purrgo_fs_close(purrgo_file_t* file) {
    if (file) fclose((FILE*)file);
}

purrgo_dir_t* purrgo_fs_opendir(const char* path) {
    // Приведение системного указателя DIR* к платформонезависимому purrgo_dir_t*
    return (purrgo_dir_t*)opendir(path);
}
bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* dirent) {
    if (!dir || !dirent) return false;

    struct dirent* ep = readdir((DIR*)dir);
    if (!ep) return false;

    /*
     * Использование sizeof защищает от переполнения и устраняет
     * зависимость от внешних необъявленных макросов длины пути.
     */
    strncpy(dirent->name, ep->d_name, sizeof(dirent->name) - 1);
    dirent->name[sizeof(dirent->name) - 1] = '\0';

#if defined(_DIRENT_HAVE_D_TYPE) || defined(__MINGW32__)
    /*
     * В MinGW-w64 поле d_type присутствует. 
     * Использование константы DT_DIR (обычно 4) безопаснее "магического числа".
     */
    dirent->is_directory = (ep->d_type == DT_DIR);
#else
    /*
     * Фолбэк для систем без поля d_type в структуре dirent.
     * Реальное определение требует вызова stat(), которому нужен полный путь к файлу.
     * Если полный путь не кэшируется в purrgo_dir_t, безопаснее установить флаг в false.
     */
    dirent->is_directory = false;
#endif

    return true;
}

void purrgo_fs_closedir(purrgo_dir_t* dir) {
    if (dir) closedir((DIR*)dir);
}