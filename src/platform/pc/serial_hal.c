#include "serial_hal.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

static HANDLE hComm = INVALID_HANDLE_VALUE;

bool serial_hal_open(const char *port_name, uint32_t baud_rate) {
    // Формирование пути для WinAPI. Префикс \\.\ обязателен для работы с портами > COM9.
    char port_path[32];
    snprintf(port_path, sizeof(port_path), "\\\\.\\%s", port_name);

    hComm = CreateFileA(port_path,
                        GENERIC_READ | GENERIC_WRITE,
                        0,    // Эксклюзивный доступ к порту
                        NULL,
                        OPEN_EXISTING,
                        0,
                        NULL);

    if (hComm == INVALID_HANDLE_VALUE) return false;

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    // Получение текущих параметров драйвера и их переопределение
    if (!GetCommState(hComm, &dcbSerialParams)) {
        CloseHandle(hComm);
        return false;
    }

    dcbSerialParams.BaudRate = baud_rate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hComm, &dcbSerialParams)) {
        CloseHandle(hComm);
        return false;
    }

    // Конфигурация таймаутов (эмуляция неблокирующего/полублокирующего чтения)
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;

    SetCommTimeouts(hComm, &timeouts);
    return true;
}

int serial_hal_read_byte(uint8_t *byte) {
    if (hComm == INVALID_HANDLE_VALUE) return -1;
    
    DWORD bytesRead;
    // Чтение байта из буфера драйвера виртуального COM-порта Windows.
    if (ReadFile(hComm, byte, 1, &bytesRead, NULL)) {
        if (bytesRead == 1) return 1; // Успешно считан 1 байт
        return 0;                     // Нет данных (таймаут)
    }
    return -1; // Ошибка чтения уровня ОС (устройство отключено/кабель извлечен)
}

void serial_hal_close(void) {
    if (hComm != INVALID_HANDLE_VALUE) {
        CloseHandle(hComm);
        hComm = INVALID_HANDLE_VALUE;
    }
}
#else

bool serial_hal_open(const char *port_name, uint32_t baud_rate) {
    (void)port_name;
    (void)baud_rate;
    return false;
}

int serial_hal_read_byte(uint8_t *byte) {
    (void)byte;
    return -1;
}

void serial_hal_close(void) {}

#endif
