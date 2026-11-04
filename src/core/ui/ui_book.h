#ifndef PURRGO_UI_BOOK_H
#define PURRGO_UI_BOOK_H

#include "purrgo/gfx_renderer.h"

/*
 * Экраны читалки (book_reader.h). Режим дисплея - 1 bit: рисуются только
 * чёрный и белый.
 *
 * Страница: верхняя строка (8 px) - инверсная строка статуса "страница/всего,
 * проценты", остальные 32 строки по 8 px - текст страницы, 29 символов по 6 px
 * (геометрия PGB 1.0). Подсказок кнопок на экране чтения нет: он целиком занят
 * текстом.
 */

/* Сброс слежения за строками списка (вход на экран). */
void ui_book_list_reset(void);

void ui_render_book_list(gfx_context_t* gfx);
void ui_render_book_read(gfx_context_t* gfx);

#endif /* PURRGO_UI_BOOK_H */
