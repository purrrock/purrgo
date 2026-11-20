/*
 * Звук сбоев железа (purrgo/hw_fault.h). Отдельный файл от hw_fault.c: сам учёт сбоев (лог, эпизоды)
 * зовут драйверы и их тесты без ядра звука и питания, а сигнал нужен только главному циклу ядра.
 */
#include "purrgo/hw_fault.h"
#include "purrgo/sound.h"
#include "purrgo/supply.h"

void purrgo_hw_fault_service(void)
{
    if (!purrgo_hw_fault_take_pending()) {
        return;
    }
    /* Просадка питания: сигнал разряда батареи (supply.c) уже прозвучал, второй тут не нужен. */
    if (purrgo_supply_storage_blocked()) {
        return;
    }
    purrgo_sound_play(PURRGO_SOUND_ERROR);
}
