#include "purrgo/sound.h"
#include "purrgo/config.h"

static const char* const k_names[PURRGO_SOUND_COUNT] = {
    [PURRGO_SOUND_NONE]    = "NONE",
    [PURRGO_SOUND_CLICK]   = "CLICK",
    [PURRGO_SOUND_INFO]    = "INFO",
    [PURRGO_SOUND_ARRIVED] = "ARRIVED",
    [PURRGO_SOUND_ERROR]   = "ERROR",
};

static const char* const k_level_names[PURRGO_SOUND_LEVEL_COUNT] = {
    [PURRGO_SOUND_LEVEL_OFF]  = "OFF",
    [PURRGO_SOUND_LEVEL_LOW]  = "LOW",
    [PURRGO_SOUND_LEVEL_HIGH] = "HIGH",
};

const char* purrgo_sound_name(purrgo_sound_t sound)
{
    return ((unsigned)sound < (unsigned)PURRGO_SOUND_COUNT) ? k_names[sound] : "?";
}

const char* purrgo_sound_level_name(purrgo_sound_level_t level)
{
    return ((unsigned)level < (unsigned)PURRGO_SOUND_LEVEL_COUNT) ? k_level_names[level] : "?";
}

void purrgo_sound_play(purrgo_sound_t sound)
{
    const purrgo_sound_level_t level = app_config.sound_level;

    /* OFF (и любое недопустимое значение): полная тишина, без исключений. */
    if (level <= PURRGO_SOUND_LEVEL_OFF || level >= PURRGO_SOUND_LEVEL_COUNT) {
        return;
    }
    if (sound <= PURRGO_SOUND_NONE || sound >= PURRGO_SOUND_COUNT) {
        return;
    }
    purrgo_platform_sound_play(sound, level);
}

void purrgo_sound_stop(void)
{
    purrgo_platform_sound_stop();
}
