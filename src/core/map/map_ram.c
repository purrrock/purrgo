#include "map_internal.h"

#include "purrgo/fs_hal.h"
#include "purrgo/ram_arena.h"
#include "purrgo/render_prof.h"

static bool s_cache_attached;

/* Арену забирает другой режим (книга): кэш секторов отключается, файлы читаются через малый. */
static void map_arena_revoked(void *ctx)
{
    (void)ctx;
    if (s_cache_attached) {
        purrgo_fs_cache_detach();
        s_cache_attached = false;
    }
}

void map_ram_enter(void)
{
    if (purrgo_arena_owner() == PURRGO_ARENA_MAP) {
        return;
    }

    const size_t bytes = purrgo_fs_cache_bytes(PURRGO_MAP_CACHE_SECTORS);

#if !PURRGO_RENDER_PROF
    if (bytes == 0u) {
        return;   /* платформа без крупного кэша (PC): арена карте не нужна */
    }
#endif

    (void)purrgo_arena_acquire(PURRGO_ARENA_MAP, map_arena_revoked, NULL);

    if (bytes != 0u) {
        void *mem = purrgo_arena_alloc(PURRGO_ARENA_MAP, bytes);
        if (mem != NULL && purrgo_fs_cache_attach(mem, bytes, PURRGO_MAP_CACHE_SECTORS)) {
            s_cache_attached = true;
        }
    }
}
