#include "purrgo/ram_arena.h"

#include <stdalign.h>
#include <stdbool.h>

static alignas(8) uint8_t s_arena[PURRGO_ARENA_BYTES];

static purrgo_arena_user_t s_owner = PURRGO_ARENA_NONE;
static purrgo_arena_revoke_fn s_revoke;
static void *s_revoke_ctx;
static size_t s_used;
static uint32_t s_session;

void *purrgo_arena_acquire(purrgo_arena_user_t user, purrgo_arena_revoke_fn revoke, void *ctx)
{
    if (user == PURRGO_ARENA_NONE) {
        return s_arena;
    }

    if (s_owner == user) {
        /* Тот же владелец: возможно новый revoke (например, после перезапуска режима). */
        s_revoke = revoke;
        s_revoke_ctx = ctx;
        return s_arena;
    }

    /*
     * Сначала арена объявляется свободной, потом зовётся revoke: прежний владелец может
     * в нём вызвать purrgo_arena_release и не должен мешать переходу.
     */
    purrgo_arena_revoke_fn old_revoke = s_revoke;
    void *old_ctx = s_revoke_ctx;
    const bool had_owner = (s_owner != PURRGO_ARENA_NONE);

    s_owner = PURRGO_ARENA_NONE;
    s_revoke = NULL;
    s_revoke_ctx = NULL;
    s_used = 0u;

    if (had_owner && old_revoke != NULL) {
        old_revoke(old_ctx);
    }

    s_owner = user;
    s_revoke = revoke;
    s_revoke_ctx = ctx;
    s_used = 0u;
    s_session++;
    return s_arena;
}

void purrgo_arena_release(purrgo_arena_user_t user)
{
    if (user == PURRGO_ARENA_NONE || s_owner != user) {
        return;
    }
    s_owner = PURRGO_ARENA_NONE;
    s_revoke = NULL;
    s_revoke_ctx = NULL;
    s_used = 0u;
    s_session++;
}

purrgo_arena_user_t purrgo_arena_owner(void)
{
    return s_owner;
}

uint32_t purrgo_arena_session(void)
{
    return s_session;
}

void *purrgo_arena_alloc(purrgo_arena_user_t user, size_t bytes)
{
    if (user == PURRGO_ARENA_NONE || s_owner != user) {
        return NULL;
    }

    const size_t need = (bytes + 7u) & ~(size_t)7u;
    if (need < bytes || need > sizeof(s_arena) - s_used) {
        return NULL;
    }

    void *p = &s_arena[s_used];
    s_used += need;
    return p;
}

size_t purrgo_arena_remaining(purrgo_arena_user_t user)
{
    if (user == PURRGO_ARENA_NONE || s_owner != user) {
        return 0u;
    }
    return sizeof(s_arena) - s_used;
}
