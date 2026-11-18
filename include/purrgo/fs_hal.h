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
/* Текущий размер файла в байтах (с учётом ещё не сброшенных на носитель записей). false - ошибка. */
bool purrgo_fs_size(purrgo_file_t* file, uint32_t* size_out);
/*
 * Обрезает файл (открытый на запись) до size байт; size не больше текущего размера.
 * После вызова позиция записи - в конце файла: следующий purrgo_fs_write() дописывает
 * данные сразу за обрезанным хвостом. Изменение попадёт на носитель при purrgo_fs_sync()
 * или закрытии файла. false - ошибка (файл не обрезан или обрезан не до конца).
 */
bool purrgo_fs_truncate(purrgo_file_t* file, uint32_t size);
/*
 * Подсказка HAL: при промахе кэша секторов читать файл (открытый на чтение)
 * блоками по `sectors` секторов (512 Б), одним многосекторным запросом к SD.
 * Полезно для длинного последовательного чтения (геометрия карты); для
 * случайного доступа (индекс, имена) оставлять 1: блок читается целиком, а
 * нужен обычно один сектор. sectors 0 и 1 - выключено. Реализация вправе
 * ограничить значение сверху или проигнорировать подсказку (PC).
 */
void purrgo_fs_set_read_ahead(purrgo_file_t* file, uint8_t sectors);
/*
 * Адаптивный read-ahead для файла, открытого на чтение: при промахе кэша читается один
 * сектор, а если промах пришёлся ровно на сектор за концом предыдущего прочитанного блока
 * (файл обходится подряд), блок растёт вдвое (2, 4, ... до max_sectors) и начинается с
 * нужного сектора. Случайный доступ остаётся по одному сектору, последовательный ускоряется.
 * Для индекса карты: обход .idx на крупных масштабах идёт подряд, на мелких - прыжками.
 * max_sectors 0 и 1 - выключено. Реализация вправе ограничить значение или проигнорировать (PC).
 */
void purrgo_fs_set_adaptive_read_ahead(purrgo_file_t* file, uint8_t max_sectors);

/*
 * Крупный кэш секторов в памяти, которую выделяет вызывающий (арена режима карты).
 * purrgo_fs_cache_bytes - сколько памяти нужно под sectors секторов (0 - не поддерживается
 * или sectors вне допустимых пределов). purrgo_fs_cache_attach подключает кэш (память
 * выровнена по 8 байт и должна жить до purrgo_fs_cache_detach), содержимое прежнего кэша
 * отбрасывается. detach возвращает встроенный маленький кэш; кэш в арене пропадает.
 * Платформа без такого кэша (PC) возвращает 0 / false и ничего не делает.
 */
size_t purrgo_fs_cache_bytes(uint32_t sectors);
bool purrgo_fs_cache_attach(void* mem, size_t size, uint32_t sectors);
void purrgo_fs_cache_detach(void);
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

/*
 * true - файл или каталог с таким путём существует. Не открывает файл, не занимает слот
 * и не пишет в лог ошибок (в отличие от purrgo_fs_open(FS_READ) для проверки наличия).
 * Любая ошибка доступа трактуется как "не существует".
 */
bool purrgo_fs_exists(const char* path);

purrgo_dir_t* purrgo_fs_opendir(const char* path);
bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* dirent);
void purrgo_fs_closedir(purrgo_dir_t* dir);


const char* purrgo_fs_get_config_path(void);
const char* purrgo_fs_get_maps_path(void);
const char* purrgo_fs_get_tracks_path(void);
/* Каталог пользовательских маршрутов (GPX). */
const char* purrgo_fs_get_routes_path(void);
/* Каталог путевых точек: GPX-файлы с <wpt> для слоя Waypoints. */
const char* purrgo_fs_get_waypts_path(void);
/* Каталог электронных книг (*.PGB, формат tools/book-compiler/PGB-format.md). */
const char* purrgo_fs_get_books_path(void);

#endif // PURRGO_FS_HAL_H
