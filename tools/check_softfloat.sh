#!/bin/sh
# Проверка итогового .elf на программные (soft-float) помощники ARM EABI.
# Использование: tools/check_softfloat.sh path/to/PurrGO_STM32U5.elf
# Ядро целочисленное; __aeabi_f* / __aeabi_d* означают, что где-то осталась
# арифметика с float/double (например, inline-функции minmea.h) - см. вывод
# с -Wl,-Map или `arm-none-eabi-nm -S --size-sort`, откуда они подтянуты.
set -u
ELF=${1:?usage: $0 file.elf}
NM=${NM:-arm-none-eabi-nm}

# __aeabi_f*/__aeabi_d* - арифметика, сравнения, преобразования float/double;
# __aeabi_[iul]2[fd], __aeabi_l2[fd] - целое -> float/double.
OUT=$("$NM" -S --size-sort "$ELF" | grep -E '__aeabi_[fd]|__aeabi_[iul]2[fd]|__aeabi_ul2[fd]' || true)

if [ -z "$OUT" ]; then
    echo "OK: soft-float helpers не найдены"
    exit 0
fi
echo "Найдены soft-float helpers:"
echo "$OUT"
exit 1
