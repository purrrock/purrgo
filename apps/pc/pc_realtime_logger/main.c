#include "serial_hal.h"
#include "purrgo/gnss_adapter.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define NMEA_BUFFER_SIZE 128
#define COM_PORT "COM10"
#define BAUD_RATE 9600

int main() {
    if (!serial_hal_open(COM_PORT, BAUD_RATE)) {
        printf("Failed to open port %s. Check Device Manager.\n", COM_PORT);
        return 1;
    }

    printf("Port %s opened at %d bps.\nListening for U-blox 7 NMEA stream...\n\n", COM_PORT, BAUD_RATE);

    purrgo_gnss_solution_t solution = {0};
    char line_buffer[NMEA_BUFFER_SIZE];
    uint16_t line_pos = 0;

    while (true) {
        uint8_t rx_byte;
        int res = serial_hal_read_byte(&rx_byte);
        
        if (res == 1) {
            // Защита от переполнения буфера
            if (line_pos < NMEA_BUFFER_SIZE - 1) {
                line_buffer[line_pos++] = (char)rx_byte;
            } else {
                // Если строка длиннее буфера и без символа завершения — сброс автомата
                line_pos = 0; 
            }

            // Идентификация окончания NMEA-посылки (\r\n)
            if (rx_byte == '\n') {
                line_buffer[line_pos] = '\0';
                
                // Передача собранной строки в платформонезависимое ядро
                purrgo_gnss_process_nmea(line_buffer, &solution);
                
                // Консольный вывод текущего состояния структуры (обновляется после каждого RMC/GGA)
                if (solution.valid) {
                    int32_t lat_int = solution.lat_1e7 / 10000000;
                    int32_t lat_frac = solution.lat_1e7 % 10000000;
                    if (lat_frac < 0) lat_frac = -lat_frac; // Модуль для правильного вывода долей

                    int32_t lon_int = solution.lon_1e7 / 10000000;
                    int32_t lon_frac = solution.lon_1e7 % 10000000;
                    if (lon_frac < 0) lon_frac = -lon_frac;

                    printf("[GNSS FIX] Time: %02d:%02d:%02d UTC | Sats: %02d | "
                           "Lat: %d.%07d, Lon: %d.%07d | Alt: %4dm | Spd: %d.%02d kt\n",
                           solution.hours, solution.minutes, solution.seconds,
                           solution.satellites,
                           lat_int, lat_frac,
                           lon_int, lon_frac,
                           solution.alt_m,
                           solution.speed_knots / 100, solution.speed_knots % 100);
                }
                
                // Очистка индекса для сборки следующей строки
                line_pos = 0;
            }
        } else if (res < 0) {
            printf("Hardware error or U-blox adapter disconnected.\n");
            break;
        }
    }

    serial_hal_close();
    return 0;
}
