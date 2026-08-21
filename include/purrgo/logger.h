#ifndef PURRGO_LOGGER_H
#define PURRGO_LOGGER_H

#include "purrgo/hardware_config.h"

/*
 * В режиме разработки на ПК (эмулятор) логи направляются в стандартный поток ошибок.
 * Обертка do { ... } while(0) обеспечивает безопасное использование макроса
 * в условных конструкциях без фигурных скобок.
 */
#if PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_DEVELOPMENT
    #include <stdio.h>
    #define PURRGO_LOG(...) do { fprintf(stderr, __VA_ARGS__); fflush(stderr); } while(0)
#else
    /*
     * На целевом микроконтроллере логирование отключается для экономии Flash/RAM.
     * В будущем здесь может быть вызов UART_Transmit или ITM_SendChar.
     */
    #define PURRGO_LOG(...) do { } while(0)
#endif

#endif // PURRGO_LOGGER_H
