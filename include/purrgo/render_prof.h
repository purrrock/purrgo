#ifndef PURRGO_RENDER_PROF_H
#define PURRGO_RENDER_PROF_H

/*
 * Профилировка кадра карты (диагностика, по умолчанию выключена).
 *
 * Включается заменой 0 на 1 в строке ниже (флаг сборки -D не нужен). В конце каждого
 * purrgo_map_render_viewport() в лог (purrgo_logger_write, уровень логирования не важен)
 * уходит одна строка PROF. Что и как мерить, описано в docs/measurements/README.md (раздел «Профилировка кадра карты»).
 *
 * Статьи (время в мкс, n - число вызовов):
 *   proj  - project_to_screen*() в обходе геометрии и POI;
 *   fill  - заливка полигонов (gfx_fill_compound_polygon*);
 *   lines - рисование линий карты (gfx_draw_line, thick, dashed, dotted, railway);
 *   lbl   - вывод отложенных подписей landuse;
 *   name  - чтение имени объекта из .db (seek + read);
 *   other - остаток кадра: чтение .idx/.mlp с SD, разбор, отсечение, подписи POI.
 * Статьи не вложены друг в друга, поэтому frame = proj + fill + lines + lbl + name + other.
 *
 * Вторая строка PROF2 разбирает остаток `other` (эти статьи входят в него):
 *   sync  - map_sync_files() (открытие/закрытие файлов карты);
 *   mlpi  - map_mlp_iter_init(): seek к объекту и чтение его заголовка, n - число объектов;
 *   mlpn  - map_mlp_iter_next(): чтение и разбор точки (включая дочитывание буфера), n - точек;
 *   rest  - остаток `other`: обход дерева .idx, отсечение, подписи POI, чтение .idx;
 *   sd    - физические чтения SD (промах кэша секторов: seek + f_read), n - число промахов,
 *           max - самое долгое одно чтение; sd НЕ вложен в другие статьи, он пересекается
 *           с name, mlpi, mlpn и rest (какие-то из них ждут SD);
 *   sect  - прочитано секторов с SD (с учётом read-ahead);
 *   hit   - обращений к кэшу секторов без чтения SD;
 *   rd    - вызовов purrgo_fs_read, B - запрошено байт.
 *
 * Трасса обращений к секторам (PROF3, PROF4). Все обращения к кэшу секторов за кадр
 * (попадания и промахи) пишутся в RAM, после кадра печатаются:
 *   PROF3 f=<файл> ra=<read-ahead> acc=.. miss=.. seq=.. near=.. dist=..
 *     acc - обращений к секторам, miss - промахов, seq - промахи в следующий блок после
 *     предыдущего промаха файла, near - промахи не дальше 16 секторов от предыдущего,
 *     dist - различных секторов файла за кадр (минимум промахов при бесконечном кэше);
 *   PROF4 ovf=.. cur=<секторов в кэше> ccur=<команд SD>/<секторов> ccuri8=.. c256=.. c256i8=..
 *         c768=.. c768i8=..
 *     модель LRU-кэша на этой трассе: ccur - размер кэша как сейчас в прошивке (cur), c256 и
 *     c768 - кэш на 256 и 768 секторов, суффикс i8 - для файлов .idx read-ahead 8 секторов
 *     (у остальных как в прошивке); cmd - число команд чтения SD, вторая цифра - секторов.
 *     cmd у ccur должен быть близок к n у sd в PROF2 (проверка модели).
 * Модель считается после кадра и занимает от долей секунды до нескольких секунд, это
 * не входит ни в frame, ни в статьи. Трасса (4 байта на запись, до PURRGO_PROF_TRACE_MAX)
 * лежит не в своём RAM, а в буфере читалки книг (purrgo_book_reader_borrow_ram, 512 КБ):
 * в режиме карты он свободен. Книга при этом закрывается. При переполнении трассы ovf=1
 * и цифры занижены; ovf=1 с нулевым числом записей - буфер получить не удалось.
 *
 * Время берётся из счётчика платформы (stm32: DWT->CYCCNT, pc: timespec_get).
 * Накладные расходы одного замера (ovh, такты) измеряются в конце кадра и вычитаются
 * из статей с большим числом вызовов (proj, name).
 */
#ifndef PURRGO_RENDER_PROF
#define PURRGO_RENDER_PROF 0
#endif

#include <stdbool.h>
#include <stdint.h>

#if PURRGO_RENDER_PROF

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PURRGO_PROF_PROJ = 0,
    PURRGO_PROF_FILL,
    PURRGO_PROF_LINES,
    PURRGO_PROF_LABELS,
    PURRGO_PROF_NAME,
    /* Ниже - статьи внутри `other`, в сумму кадра не входят (см. PROF2). */
    PURRGO_PROF_SYNC,
    PURRGO_PROF_MLP_INIT,
    PURRGO_PROF_MLP_NEXT,
    PURRGO_PROF_SD,
    PURRGO_PROF_COUNT
} purrgo_prof_id_t;

/* Простые счётчики событий (без времени). */
typedef enum {
    PURRGO_PROF_CNT_SD_SECTORS = 0,
    PURRGO_PROF_CNT_CACHE_HIT,
    PURRGO_PROF_CNT_FS_READ,
    PURRGO_PROF_CNT_FS_READ_BYTES,
    PURRGO_PROF_CNT_COUNT
} purrgo_prof_cnt_t;

/* Максимум записей трассы; сама трасса лежит в одолженном буфере читалки (см. ниже). */
#ifndef PURRGO_PROF_TRACE_MAX
#define PURRGO_PROF_TRACE_MAX 131072u
#endif

/* Платформа: монотонный 32-битный счётчик и его частота (тактов в микросекунду). */
void purrgo_prof_platform_enable(void);
uint32_t purrgo_prof_ticks(void);
uint32_t purrgo_prof_ticks_per_us(void);

void purrgo_render_prof_begin_frame(void);
void purrgo_render_prof_add(purrgo_prof_id_t id, uint32_t ticks);
void purrgo_render_prof_count(purrgo_prof_cnt_t id, uint32_t n);

/* Сведения о файле в слоте пула (path == NULL - имя не менять), вызывается при open
 * и при смене read-ahead. cache_entries - размер кэша секторов прошивки. */
void purrgo_render_prof_file_info(uint8_t slot, const char *path, uint8_t read_ahead,
                                  uint32_t cache_entries);
/* Обращение к сектору файла: miss - сектор пришлось читать с SD. slot < 16. */
void purrgo_render_prof_sector(uint8_t slot, uint32_t sector, bool miss);
void purrgo_render_prof_end_frame(int scale, int ok);
/* Кадр прерван без end_frame: перестать писать трассу (буфер снова принадлежит читалке). */
void purrgo_render_prof_abort_frame(void);

#ifdef __cplusplus
}
#endif

#define PURRGO_PROF_BEGIN(var) const uint32_t var = purrgo_prof_ticks()
#define PURRGO_PROF_END(var, id) \
    purrgo_render_prof_add((id), (uint32_t)(purrgo_prof_ticks() - (var)))
#define PURRGO_PROF_COUNT_ADD(id, n) purrgo_render_prof_count((id), (uint32_t)(n))
#define PURRGO_PROF_FILE_INFO(slot, path, ra, entries) \
    purrgo_render_prof_file_info((slot), (path), (ra), (entries))
#define PURRGO_PROF_SECTOR(slot, sector, miss) \
    purrgo_render_prof_sector((slot), (sector), (miss))

#else /* !PURRGO_RENDER_PROF */

#define PURRGO_PROF_BEGIN(var) ((void)0)
#define PURRGO_PROF_END(var, id) ((void)0)
#define PURRGO_PROF_COUNT_ADD(id, n) ((void)0)
#define PURRGO_PROF_FILE_INFO(slot, path, ra, entries) ((void)0)
#define PURRGO_PROF_SECTOR(slot, sector, miss) ((void)0)

#endif

#endif /* PURRGO_RENDER_PROF_H */
