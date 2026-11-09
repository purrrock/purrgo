#ifndef PURRGO_BOOK_READER_H
#define PURRGO_BOOK_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/keyboard.h"
#include "purrgo/pgb.h"

/*
 * Режим чтения электронных книг (BOOKS в главное меню).
 *
 * Отдельный режим работы, более экономичный, чем навигатор:
 *   - вход выключает запись трека (purrgo_logger_stop()) и GNSS
 *     (purrgo_gnss_power_off()); выход включает GNSS снова, трек начнётся
 *     заново по первому фиксу;
 *   - дисплей работает в режиме 1 bit (DISPLAY_MODE_MONO);
 *   - книги лежат в каталоге purrgo_fs_get_books_path() (0:/PURRGO/BOOKS, файлы с расширением .PGB),
 *     формат - tools/book-compiler/PGB-format.md (PGB 1.1: 25 строк текста и страницы-картинки
 *     1 bit), разбор - pgb.h;
 *   - при открытии книга предзагружается в RAM (см. ниже), перелистывание не
 *     обращается к SD;
 *   - позиция чтения (имя файла, file_crc32, страница) хранится в app_config
 *     (BOOK_FILE, BOOK_CRC, BOOK_PAGE в PURRGO.CFG). В RAM она обновляется на
 *     каждом перелистывании, а на SD записывается ТОЛЬКО по тайм-ауту
 *     бездействия PURRGO_BOOK_SAVE_TIMEOUT_MS и при выходе из режима.
 *
 * Экраны: APP_STATE_BOOK_LIST (список книг), APP_STATE_BOOK_READ (страница) и
 * APP_STATE_BOOK_SEARCH_INPUT (экранная клавиатура для строки поиска).
 * Кнопки (K1..K4, короткое / длинное нажатие):
 *
 *   список:  K1 вверх / на 8 вверх       K2 вниз / на 8 вниз
 *            K3 открыть книгу             K4 выход в главное меню
 *   чтение:  K1 страница назад / -10      K2 страница вперёд / +10
 *            K3 переход по процентам      K3 длинное - список книг
 *            K4 выход в главное меню      K4 длинное - поиск по книге
 *   переход: K1 -1% / -10%                K2 +1% / +10%
 *            K3 перейти                   K4 отмена
 *   ввод:    кнопки экранной клавиатуры (keyboard.h): K1 влево, K2 вправо, K3 нажать,
 *            K4 вниз; длинные: K1 стереть, K2 вверх, K3 готово, K4 отмена
 *   поиск:   (совпадение найдено и подсвечено на странице)
 *            K1 предыдущее совпадение     K2 следующее совпадение
 *            K3 новый запрос              K4 закончить поиск (подсветка гаснет)
 *            пока поиск идёт (долгая книга): K4 - остановить
 *
 * Поиск (book_search.h). Запрос вводится на экранной клавиатуре (кнопки K4 длинное
 * на странице, K3 в режиме поиска). Поиск начинается со страницы, открытой на экране
 * (с начала страницы), идёт вперёд до конца книги и затем с начала до этой же страницы;
 * находит текст вне зависимости от регистра, Е и Ё считаются одной буквой, фраза может
 * переходить на следующую строку и страницу, слово - быть разорвано переносом "-".
 * Найденное совпадение выводится инверсией, страница запоминается как позиция чтения.
 * Страницы-картинки пропускаются. Поиск выполняется по частям (PURRGO_BOOK_SEARCH_SLICE_PAGES
 * страниц за вызов purrgo_book_reader_tick()), чтобы кнопки и экран не замирали:
 * книга, целиком лежащая в RAM, просматривается за доли секунды, для длинной книги, окно
 * которой приходится перечитывать с SD, на экране виден прогресс, а K4 останавливает поиск.
 *
 * Предзагрузка. В RAM лежит начало файла (заголовок, метаданные, индекс;
 * проверяется meta_crc32) и непрерывное окно данных страниц. Книга, которая
 * целиком помещается в буфер (сжатая русская проза - около 45 % от текста),
 * загружается вся. Больше - окно размером с остаток буфера, расположенное так,
 * чтобы текущая страница была в начале окна с небольшим запасом назад; при
 * выходе страницы за окно оно перечитывается с SD (одним последовательным
 * чтением). Страницы распаковываются из RAM (LZ4 Block) по запросу.
 */

/* Размер RAM-буфера под книгу (заголовок + индекс + окно данных), байт. */
#ifndef PURRGO_BOOK_RAM_BYTES
#define PURRGO_BOOK_RAM_BYTES (512u * 1024u)
#endif

/* Через сколько мс без перелистывания позиция записывается на SD. */
#ifndef PURRGO_BOOK_SAVE_TIMEOUT_MS
#define PURRGO_BOOK_SAVE_TIMEOUT_MS 60000u
#endif

/* Страниц, просматриваемых поиском за один шаг (за один вызов tick или нажатие кнопки). */
#ifndef PURRGO_BOOK_SEARCH_SLICE_PAGES
#define PURRGO_BOOK_SEARCH_SLICE_PAGES 96u
#endif

/* Прогресс поиска перерисовывается на экране не чаще, чем раз в столько мс. */
#ifndef PURRGO_BOOK_SEARCH_PROGRESS_MS
#define PURRGO_BOOK_SEARCH_PROGRESS_MS 1000u
#endif

/* Книг в списке (лишние файлы каталога не показываются). */
#define PURRGO_BOOK_LIST_MAX     48
/* Ширина названия и автора в списке, символов. */
#define PURRGO_BOOK_TITLE_COLS   26
#define PURRGO_BOOK_AUTHOR_COLS  29

/* Шаг "длинного" перелистывания, страниц, и строк списка. */
#define PURRGO_BOOK_PAGE_JUMP    10
#define PURRGO_BOOK_LIST_JUMP    8

typedef struct {
    char     file[PURRGO_BOOK_NAME_MAX];            /* имя файла в каталоге BOOKS (UTF-8, для открытия) */
    char     title[PURRGO_BOOK_TITLE_COLS + 1];     /* PurrGO-256, готово к выводу */
    char     author[PURRGO_BOOK_AUTHOR_COLS + 1];   /* PurrGO-256, готово к выводу */
    uint32_t page_count;
    uint32_t file_crc32;
    bool     ok;                                    /* заголовок прочитан; false - файл повреждён */
} purrgo_book_item_t;

/* Состояние поиска по книге на экране чтения. */
typedef enum {
    PURRGO_BOOK_FIND_OFF = 0,     /* поиска нет: обычное чтение */
    PURRGO_BOOK_FIND_RUNNING,     /* идёт просмотр страниц (K4 - остановить) */
    PURRGO_BOOK_FIND_FOUND,       /* совпадение найдено: подсвечено, K1/K2 - предыдущее/следующее */
    PURRGO_BOOK_FIND_NOT_FOUND    /* в книге запроса нет */
} purrgo_book_find_t;

/* Что показывать на экране чтения. */
typedef struct {
    bool          open;         /* книга открыта */
    pgb_status_t  error;        /* если !open: причина (PGB_OK - ничего не открывалось) */
    pgb_status_t  page_error;   /* PGB_OK или причина, по которой страница не читается */
    const char   *file;         /* имя файла книги (UTF-8) */
    uint32_t      page;         /* текущая страница, с нуля */
    uint32_t      page_count;
    uint8_t       percent;      /* 0..100, по номеру страницы */
    bool          jump;         /* включён переход по процентам */
    uint8_t       jump_percent;
    uint32_t      jump_page;
    const uint8_t *text;        /* текст страницы (PurrGO-256, строки через LF), NULL - нет */
    size_t        text_len;
    const uint8_t *image;       /* страница-картинка: 1 bit на пиксель, PGB_IMG_ROW_BYTES(image_w) байт на строку,
                                   MSB слева, 1 - чёрный; NULL - не картинка (тогда см. text) */
    uint16_t      image_w;
    uint16_t      image_h;
    uint8_t       find;         /* purrgo_book_find_t */
    uint8_t       find_percent; /* RUNNING: сколько страниц просмотрено, 0..100 */
    const char   *find_query;   /* запрос как набран (PurrGO-256), может быть пустой строкой */
    uint32_t      hl_start;     /* FOUND: подсветка в text - байты [hl_start, hl_start + hl_len) */
    uint32_t      hl_len;       /* 0 - подсветки нет (в том числе на странице без совпадения) */
} purrgo_book_view_t;

/*
 * Вход в режим: останавливает трек и GNSS. Если последняя книга (app_config.book_file)
 * открывается, возвращает APP_STATE_BOOK_READ с сохранённой страницей, иначе
 * собирает список и возвращает APP_STATE_BOOK_LIST.
 */
purrgo_state_t purrgo_book_reader_enter(void);

/*
 * Кнопка в состоянии APP_STATE_BOOK_LIST или APP_STATE_BOOK_READ. В *next_state
 * записывается новое состояние (при выходе - APP_STATE_MENU_MAIN, GNSS уже
 * включён, позиция записана). Возвращает true, если экран нужно перерисовать.
 */
bool purrgo_book_reader_handle_button(purrgo_state_t state, purrgo_btn_t button,
                                      purrgo_state_t *next_state);

/*
 * Вызывается из главного цикла: записывает позицию по тайм-ауту бездействия и
 * делает очередной шаг поиска по книге. true - экран нужно перерисовать (поиск
 * закончился или пора обновить прогресс).
 */
bool purrgo_book_reader_tick(uint32_t now_ms);

/* Записать позицию на SD сейчас, если она ещё не записана. true - записана или записывать нечего. */
bool purrgo_book_reader_flush(void);

/* Режим включён (GNSS и трек остановлены). */
bool purrgo_book_reader_is_active(void);

/* Позиция в RAM не записана на SD. */
bool purrgo_book_reader_position_dirty(void);

/* Данные для отрисовки. */
void purrgo_book_reader_get_view(purrgo_book_view_t *view);
/* Клавиатура ввода запроса (состояние APP_STATE_BOOK_SEARCH_INPUT). */
const purrgo_keyboard_t *purrgo_book_reader_get_keyboard(void);
int  purrgo_book_list_count(void);
int  purrgo_book_list_cursor(void);
const purrgo_book_item_t *purrgo_book_list_get(int index);
/* Книга с индексом - последняя открытая (app_config.book_file). */
bool purrgo_book_list_is_last(int index);

/*
 * Для тестов: подменить RAM-буфер (NULL - буфер по умолчанию, PURRGO_BOOK_RAM_BYTES).
 * Закрывает открытую книгу. Буфер должен жить, пока читалка его использует.
 */
void purrgo_book_reader_set_ram(void *buffer, size_t size);

/* Для тестов: полный сброс состояния без обращений к GNSS, логгеру и SD. */
void purrgo_book_reader_reset(void);

#endif /* PURRGO_BOOK_READER_H */
