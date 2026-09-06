#ifndef PURRGO_LOGGER_H
#define PURRGO_LOGGER_H

/*
 * Инициализация платформенного логгера.
 *
 * На текущем этапе UART уже инициализируется CubeMX-функцией
 * MX_USART2_UART_Init(), поэтому функция фактически ничего
 * не делает. Она оставлена как отдельная точка входа платформы.
 */
void purrgo_logger_init(void);

/*
 * Вывод форматированного диагностического сообщения через UART.
 *
 * Формат строки задаётся так же, как для printf():
 *
 *     purrgo_logger_write("Value = %d\r\n", value);
 */
void purrgo_logger_write(const char *format, ...);

#endif /* PURRGO_LOGGER_H */