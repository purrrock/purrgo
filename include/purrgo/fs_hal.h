#ifndef PURRGO_FS_HAL_H
#define PURRGO_FS_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct purrgo_file_s purrgo_file_t;
typedef struct purrgo_dir_s purrgo_dir_t;

#define PURRGO_FS_MAX_PATH 256

typedef struct {
    char name[PURRGO_FS_MAX_PATH];
    bool is_directory;
} purrgo_fs_dirent_t;

typedef enum {
    FS_READ,            // Добавлен режим чтения
    FS_WRITE_CREATE,
    FS_WRITE_APPEND
} fs_mode_t;

purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode);
/* Возвращает число реально записанных байт; значение меньше size означает ошибку записи. */
uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, uint32_t size);
// Добавлена функция чтения (уже использовалась в прототипе gpx_parser)
uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size);
bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset);
/*
 * Подсказка HAL: при промахе кэша секторов читать файл (открытый на чтение)
 * блоками по `sectors` секторов (512 Б), одним многосекторным запросом к SD.
 * Полезно для длинного последовательного чтения (геометрия карты); для
 * случайного доступа (индекс, имена) оставлять 1: блок читается целиком, а
 * нужен обычно один сектор. sectors 0 и 1 - выключено. Реализация вправе
 * ограничить значение сверху или проигнорировать подсказку (PC).
 */
void purrgo_fs_set_read_ahead(purrgo_file_t* file, uint8_t sectors);
/* true - данные сброшены на носитель. false - ошибка (файл следует закрыть и открыть заново). */
bool purrgo_fs_sync(purrgo_file_t* file);
/* Файл закрывается (слот освобождается) в любом случае; false - данные могли быть потеряны. */
bool purrgo_fs_close(purrgo_file_t* file);
/*
 * Заменяет файл dst файлом src (src перестаёт существовать).
 * Используется для атомарного сохранения: запись во временный файл, sync, replace.
 * Оба файла должны быть закрыты.
 */
bool purrgo_fs_replace(const char* src, const char* dst);

purrgo_dir_t* purrgo_fs_opendir(const char* path);
bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* dirent);
void purrgo_fs_closedir(purrgo_dir_t* dir);


const char* purrgo_fs_get_config_path(void);
const char* purrgo_fs_get_maps_path(void);
const char* purrgo_fs_get_tracks_path(void);

#endif // PURRGO_FS_HAL_H
