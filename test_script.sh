#!/bin/bash
# Check if the cache logic looks right
cat apps/stm32/PurrGO_STM32/Core/Src/fs_hal_stm32.c | grep -n 'purrgo_fs_read_cached'
