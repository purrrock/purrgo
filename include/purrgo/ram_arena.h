#ifndef PURRGO_RAM_ARENA_H
#define PURRGO_RAM_ARENA_H

/*
 * Общая арена RAM: один большой буфер, которым по очереди пользуются режимы,
 * не работающие одновременно. Режим книги держит в нём книгу целиком (окно данных),
 * режим карты - кэш секторов SD и рабочую память диагностики.
 *
 * Владелец один. Тот, кто просит арену (purrgo_arena_acquire), забирает её у текущего
 * владельца: тот получает вызов revoke и обязан немедленно перестать пользоваться
 * памятью (книга закрывается, кэш секторов отключается). Данные при смене владельца
 * не сохраняются. Многопоточность не поддерживается: всё идёт из основного цикла.
 *
 * Выделение внутри арены - простое линейное (purrgo_arena_alloc): память живёт
 * до смены владельца, освобождать по частям нельзя.
 */

#include <stddef.h>
#include <stdint.h>

/* Размер арены, байт. */
#ifndef PURRGO_ARENA_BYTES
#define PURRGO_ARENA_BYTES (512u * 1024u)
#endif

typedef enum {
    PURRGO_ARENA_NONE = 0,   /* арена свободна */
    PURRGO_ARENA_BOOK,       /* читалка книг */
    PURRGO_ARENA_MAP         /* режим карты: кэш секторов SD и scratch */
} purrgo_arena_user_t;

/* Вызывается, когда арену забирает другой владелец. ctx - значение из acquire. */
typedef void (*purrgo_arena_revoke_fn)(void *ctx);

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Занять арену. Если владелец уже user, ничего не меняется (память и выделения сохраняются).
 * Иначе у прежнего владельца вызывается revoke, арена очищается от выделений и переходит
 * к user. Возвращает начало арены (выровнено по 8 байт), NULL не возвращается.
 */
void *purrgo_arena_acquire(purrgo_arena_user_t user, purrgo_arena_revoke_fn revoke, void *ctx);

/* Отдать арену, если она у user (иначе ничего). revoke при этом не вызывается. */
void purrgo_arena_release(purrgo_arena_user_t user);

/* Текущий владелец. */
purrgo_arena_user_t purrgo_arena_owner(void);

/*
 * Номер сессии владения: растёт при каждой смене владельца. Позволяет понять, что выделенная
 * раньше память могла быть отобрана (указатель сравнивают вместе с номером).
 */
uint32_t purrgo_arena_session(void);

/* Выделить bytes (кратно 8) у владельца user; NULL, если user не владелец или не хватает. */
void *purrgo_arena_alloc(purrgo_arena_user_t user, size_t bytes);

/* Сколько ещё можно выделить у владельца user (0, если user не владелец). */
size_t purrgo_arena_remaining(purrgo_arena_user_t user);

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_RAM_ARENA_H */
