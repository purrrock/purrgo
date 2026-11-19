/*
 * Звуковые сигналы на ПК (эмулятор): излучателя нет, сигнал пишется в лог.
 * Приоритеты и политику (включён ли звук) проверяет ядро, purrgo/sound.h.
 */
#include "purrgo/sound.h"
#include "purrgo/logger.h"

void purrgo_platform_sound_play(purrgo_sound_t sound)
{
    PURRGO_LOG("SOUND: %s\n", purrgo_sound_name(sound));
}
