set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_C_FLAGS "-mcpu=cortex-m4 -mthumb" CACHE INTERNAL "C Compiler options")
set(CMAKE_CXX_FLAGS "-mcpu=cortex-m4 -mthumb" CACHE INTERNAL "C++ Compiler options")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m4 -mthumb" CACHE INTERNAL "ASM Compiler options")
set(CMAKE_EXE_LINKER_FLAGS "-mcpu=cortex-m4 -mthumb --specs=nano.specs -T${CMAKE_SOURCE_DIR}/STM32F411xx_FLASH.ld -Wl,--gc-sections" CACHE INTERNAL "Linker options")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
