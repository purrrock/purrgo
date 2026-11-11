#ifndef PURRGO_TRIP_COMPUTER_H
#define PURRGO_TRIP_COMPUTER_H

#include "purrgo/gnss_types.h"
#include "purrgo/app_fsm.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Снимок данных путевого компьютера, которые не входят в решение GNSS: дистанция
 * трека, точность, индикаторы помех/глушения/спуфинга (UBX, PURRGO_GNSS_UBX = 1).
 * Снимок обновляется purrgo_trip_computer_update() - то есть не чаще раза в секунду,
 * как и остальные данные экрана, - и хранит уже "экранные" значения (округлённые),
 * поэтому любое изменение снимка стоит перерисовки. Без UBX все *_valid == false.
 */
typedef struct {
    bool dist_valid;
    uint32_t dist_m;        /* дистанция с начала записи трека (одометр модуля) */

    bool acc_valid;
    uint32_t hacc_dm;       /* горизонтальная точность hAcc, дециметры */
    uint32_t vacc_dm;       /* вертикальная точность vAcc, дециметры */

    bool sig_valid;         /* есть свежий UBX-SEC-SIG */
    uint8_t jam_state;      /* PURRGO_UBX_JAM_*: 0 неизвестно, 1 нет, 2 глушение */
    uint8_t spoof_state;    /* PURRGO_UBX_SPOOF_*: 0 неизвестно, 1 нет, 2 признаки, 3 подтверждён */

    bool rf_valid;          /* есть свежий UBX-MON-RF */
    uint8_t cw_percent;     /* CW-помеха, % (шаг 5) */
    uint8_t agc_percent;    /* усиление AGC, % (шаг 5): высокое - сильные помехи */
    uint8_t ant_status;     /* PURRGO_UBX_ANT_*: 2 OK, 3 замыкание, 4 обрыв */
} purrgo_trip_info_t;

void purrgo_trip_computer_init(void);
void purrgo_trip_computer_update(const purrgo_gnss_solution_t* current_fix);
bool purrgo_trip_computer_handle_button(purrgo_action_t button);

/* Текущий снимок (никогда не NULL). */
const purrgo_trip_info_t* purrgo_trip_computer_info(void);

/*
 * true, если с прошлого вызова снимок изменился (экран Trip computer нужно
 * перерисовать); флаг сбрасывается.
 */
bool purrgo_trip_computer_take_changed(void);

#endif // PURRGO_TRIP_COMPUTER_H
