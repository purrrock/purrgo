#include "purrgo/sound.h"
#include "purrgo/config.h"

static const char* const k_names[PURRGO_SOUND_COUNT] = {
    [PURRGO_SOUND_NONE]    = "NONE",
    [PURRGO_SOUND_CLICK]   = "CLICK",
    [PURRGO_SOUND_INFO]    = "INFO",
    [PURRGO_SOUND_ARRIVED] = "ARRIVED",
    [PURRGO_SOUND_ERROR]   = "ERROR",
};

const char* purrgo_sound_name(purrgo_sound_t sound)
{
    return ((unsigned)sound < (unsigned)PURRGO_SOUND_COUNT) ? k_names[sound] : "?";
}

void purrgo_sound_play(purrgo_sound_t sound)
{
    if (!app_config.sound_on) {
        return;
    }
    if (sound <= PURRGO_SOUND_NONE || sound >= PURRGO_SOUND_COUNT) {
        return;
    }
    purrgo_platform_sound_play(sound);
}
