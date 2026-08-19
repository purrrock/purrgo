#include "serial_hal.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/track_logger.h" // Подключение API логгера
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>

#define NMEA_BUFFER_SIZE 128
#define BAUD_RATE 9600

// Атомарный флаг для безопасной синхронизации между потоком ОС и основным циклом
static volatile sig_atomic_t keep_running = 1;

// Обработчик прерывания от ОС (например, при нажатии Ctrl+C)
static void sigint_handler(int dummy) {
    (void)dummy;
    keep_running = 0; // Даем сигнал основному циклу на корректное завершение
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <COM_PORT>\n", argv[0]);
        printf("Example: %s COM3\n", argv[0]);
        return 1;
    }

    const char *com_port = argv[1];

    if (!serial_hal_open(com_port, BAUD_RATE)) {
        printf("Failed to open port %s. Check Device Manager.\n", com_port);
        return 1;
    }

    // Регистрация перехвата сигнала SIGINT (Ctrl+C)
    signal(SIGINT, sigint_handler);

    printf("Port %s opened at %d bps.\n", com_port, BAUD_RATE);
    printf("Listening for U-blox stream. Press Ctrl+C to stop and save GPX.\n\n");

    purrgo_gnss_solution_t solution = {0};
    char line_buffer[NMEA_BUFFER_SIZE];
    uint16_t line_pos = 0;
    
    // Флаг состояния для однократного создания файла
    bool is_logging_active = false; 

    // Замена бесконечного цикла на цикл с проверкой флага
    while (keep_running) {
        uint8_t rx_byte;
        int res = serial_hal_read_byte(&rx_byte);
        
        if (res == 1) {
            if (line_pos < NMEA_BUFFER_SIZE - 1) {
                line_buffer[line_pos++] = (char)rx_byte;
            } else {
                line_pos = 0; 
            }

            if (rx_byte == '\n') {
                line_buffer[line_pos] = '\0';
                
                purrgo_gnss_process_nmea(line_buffer, &solution);
                
                // Фильтрация дубликатов: запись и вывод осуществляются только 1 раз за эпоху,
                // после обработки сообщения GGA (содержащего высоту). К этому моменту 
                // координаты и время уже гарантированно обновлены сообщением RMC.
                if (solution.valid && strstr(line_buffer, "GGA") != NULL) {
                    // 1. Инициализация логгера при первом получении 3D Fix
                    if (!is_logging_active) {
                        if (purrgo_logger_start(&solution)) {
                            is_logging_active = true;
                            printf(">>> Valid fix acquired. Started recording GPX file.\n");
                        }
                    }
                    
                    // 2. Добавление точки в буфер (сброс на диск происходит автоматически внутри функции)
                    if (is_logging_active) {
                        purrgo_logger_add_point(&solution);
                    }

                    // Консольный вывод для контроля
                    int32_t lat_int = solution.lat_1e7 / 10000000;
                    int32_t lat_frac = solution.lat_1e7 % 10000000;
                    if (lat_frac < 0) lat_frac = -lat_frac;

                    int32_t lon_int = solution.lon_1e7 / 10000000;
                    int32_t lon_frac = solution.lon_1e7 % 10000000;
                    if (lon_frac < 0) lon_frac = -lon_frac;

                    printf("[GNSS FIX] %02d:%02d:%02d UTC | Sats: %02d | "
                           "Lat: %d.%07d, Lon: %d.%07d | Alt: %4dm\n",
                           solution.hours, solution.minutes, solution.seconds,
                           solution.satellites_tracked,
                           lat_int, lat_frac,
                           lon_int, lon_frac,
                           solution.alt_m);
                }
                
                line_pos = 0;
            }
        } else if (res < 0) {
            printf("Hardware error or U-blox adapter disconnected.\n");
            break;
        }
    }

    // --- Штатное завершение работы ---
    printf("\nShutting down...\n");
    
    // 3. Принудительный сброс буфера (flush) и запись закрывающих XML-тегов
    purrgo_logger_stop();
    
    // 4. Освобождение дескриптора ОС
    serial_hal_close();
    
    printf("GPX file saved. Exiting.\n");
    return 0;
}