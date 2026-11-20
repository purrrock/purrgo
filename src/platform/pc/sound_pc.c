/*
 * Звуковые сигналы на ПК (эмулятор): излучателя нет, сигнал пишется в лог.
 * Приоритеты и политику (включён ли звук) проверяет ядро, purrgo/sound.h.
 */
#include "purrgo/sound.h"
#include "purrgo/logger.h"

void purrgo_platform_sound_play(purrgo_sound_t sound, purrgo_sound_level_t level)
{
    PURRGO_LOG("SOUND: %s (%s)\n", purrgo_sound_name(sound), purrgo_sound_level_name(level));
}

void purrgo_platform_sound_stop(void)
{
    PURRGO_LOG("SOUND: stop\n");
}
