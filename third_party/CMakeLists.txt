# third_party/CMakeLists.txt

# Создание статической библиотеки minmea
add_library(minmea STATIC 
    minmea/minmea.c
)

# Указание директории с заголовочными файлами. 
# PUBLIC позволяет транслировать путь к include всем таргетам, которые линкуют minmea.
target_include_directories(minmea PUBLIC minmea)

# Отключение предупреждений компилятора для стороннего кода
target_compile_options(minmea PRIVATE -w)

# Проверка целевой ОС (Windows). 
# POSIX-функция timegm() отсутствует в стандартной библиотеке среды MinGW-w64.
# Директива препроцессора заменяет вызовы timegm на Windows-совместимый аналог _mkgmtime.
if(WIN32)
    target_compile_definitions(minmea PRIVATE timegm=_mkgmtime)
endif()