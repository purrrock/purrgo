/*
 * Общая арена RAM (purrgo/ram_arena.h): владение, отбор арены у прежнего владельца (revoke),
 * линейное выделение, номер сессии и работа читалки книг через арену.
 */
#include <string.h>
#include "purrgo/book_reader.h"
#include "purrgo/ram_arena.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

static int g_book_revoked, g_map_revoked;
static void book_revoke(void *ctx) { g_book_revoked++; (void)ctx; }
static void map_revoke(void *ctx)  { g_map_revoked++; (void)ctx; purrgo_arena_release(PURRGO_ARENA_MAP); /* revoke может вызвать release */ }

int main(void)
{
    purrgo_book_reader_reset();
    CHECK(purrgo_arena_owner() == PURRGO_ARENA_NONE);
    CHECK(purrgo_arena_alloc(PURRGO_ARENA_MAP, 16) == NULL);      /* без владения выделять нельзя */
    CHECK(purrgo_arena_remaining(PURRGO_ARENA_MAP) == 0);

    /* Первый владелец. */
    uint8_t *base = purrgo_arena_acquire(PURRGO_ARENA_MAP, map_revoke, NULL);
    CHECK(base != NULL && ((uintptr_t)base & 7u) == 0);
    CHECK(purrgo_arena_owner() == PURRGO_ARENA_MAP);
    CHECK(purrgo_arena_remaining(PURRGO_ARENA_MAP) == PURRGO_ARENA_BYTES);
    const uint32_t session = purrgo_arena_session();

    uint8_t *a = purrgo_arena_alloc(PURRGO_ARENA_MAP, 10);        /* округление до 8 */
    uint8_t *b = purrgo_arena_alloc(PURRGO_ARENA_MAP, 100);
    CHECK(a == base && b == base + 16);
    CHECK(purrgo_arena_remaining(PURRGO_ARENA_MAP) == PURRGO_ARENA_BYTES - 16 - 104);
    CHECK(purrgo_arena_alloc(PURRGO_ARENA_MAP, PURRGO_ARENA_BYTES) == NULL);   /* больше остатка */
    CHECK(purrgo_arena_alloc(PURRGO_ARENA_BOOK, 8) == NULL);                    /* чужой владелец */

    /* Повторный acquire тем же владельцем ничего не меняет. */
    CHECK(purrgo_arena_acquire(PURRGO_ARENA_MAP, map_revoke, NULL) == base);
    CHECK(purrgo_arena_session() == session);
    CHECK(purrgo_arena_remaining(PURRGO_ARENA_MAP) == PURRGO_ARENA_BYTES - 16 - 104);

    /* Другой владелец забирает арену: старый получает revoke, выделения сбрасываются. */
    CHECK(purrgo_arena_acquire(PURRGO_ARENA_BOOK, book_revoke, NULL) == base);
    CHECK(g_map_revoked == 1);
    CHECK(purrgo_arena_owner() == PURRGO_ARENA_BOOK);
    CHECK(purrgo_arena_session() != session);
    CHECK(purrgo_arena_remaining(PURRGO_ARENA_BOOK) == PURRGO_ARENA_BYTES);
    CHECK(purrgo_arena_remaining(PURRGO_ARENA_MAP) == 0);

    /* release чужого владельца ничего не делает; release владельца освобождает арену. */
    purrgo_arena_release(PURRGO_ARENA_MAP);
    CHECK(purrgo_arena_owner() == PURRGO_ARENA_BOOK);
    purrgo_arena_release(PURRGO_ARENA_BOOK);
    CHECK(purrgo_arena_owner() == PURRGO_ARENA_NONE);
    CHECK(g_book_revoked == 0);                                   /* при release revoke не зовётся */

    /* Читалка не держит арену, пока книга не открыта, и отдаёт её при закрытии. */
    CHECK(purrgo_arena_owner() == PURRGO_ARENA_NONE);
    purrgo_book_reader_set_ram(NULL, 0);
    CHECK(purrgo_arena_owner() == PURRGO_ARENA_NONE);

    return test_finish("test_ram_arena");
}
