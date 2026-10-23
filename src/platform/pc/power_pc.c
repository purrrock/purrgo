#include "purrgo/power.h"
#include <stdlib.h>

void purrgo_platform_storage_unmount(void)
{
    /* Файлы на хосте закрываются самим ядром, размонтировать нечего. */
}

void purrgo_platform_enter_standby(void)
{
    exit(0);
}
