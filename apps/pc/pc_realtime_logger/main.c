#include "serial_hal.h"
#include "purrgo/gnss_adapter.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define NMEA_BUFFER_SIZE 128
#define BAUD_RATE 9600

// Использование аргументов командной строки: argc содержит количество аргументов,
// argv - массив указателей на строки аргументов (argv[0] - имя исполняемого файла).
int main(int argc, char *argv[]) {
    // Проверка наличия переданного аргумента с номером COM-порта.
    if (argc < 2) {
        printf("Usage: %s <COM_PORT>\n", argv[0]);
        printf("Example: %s COM3\n", argv[0]);
        return 1;
    }

    // Получение имени порта из первого пользовательского аргумента
    const char *com_port = argv[1];

    if (!serial_hal_open(com_port, BAUD_RATE)) {
        printf("Failed to open port %s. Check Device Manager.\n", com_port);
        return 1;
    }

    printf("Port %s opened at %d bps.\nListening for U-blox 7 NMEA stream...\n\n", com_port, BAUD_RATE);

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
                // Сброс автомата при превышении длины буфера без получения символа завершения \n
                line_pos = 0; 
            }

            // Идентификация окончания NMEA-посылки (\r\n)
            if (rx_byte == '\n') {
                line_buffer[line_pos] = '\0';
                
                purrgo_gnss_process_nmea(line_buffer, &solution);
                
                if (solution.valid) {
                    int32_t lat_int = solution.lat_1e7 / 10000000;
                    int32_t lat_frac = solution.lat_1e7 % 10000000;
                    if (lat_frac < 0) lat_frac = -lat_frac;

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