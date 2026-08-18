if (WIN32 OR UNIX)
    file(GLOB_RECURSE PC_PLATFORM_SOURCES CONFIGURE_DEPENDS "src/platform/pc/*.c")
    
    add_library(purrgo_platform_pc
        ${PC_PLATFORM_SOURCES}
    )
    
    target_include_directories(purrgo_platform_pc
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/pc
    )

    # Использование GLOB без рекурсии для исключения захвата исходников из вложенных папок
    file(GLOB PC_APP_SOURCES CONFIGURE_DEPENDS "apps/pc/*.c")
    add_executable(purrgo_pc
        ${PC_APP_SOURCES}
    )
    target_link_libraries(purrgo_pc PRIVATE purrgo_core purrgo_platform_pc)

    # Корректный путь к исходникам приложения логгера
    file(GLOB_RECURSE LOGGER_APP_SOURCES CONFIGURE_DEPENDS "apps/pc/pc_realtime_logger/*.c")
    add_executable(pc_realtime_logger
        ${LOGGER_APP_SOURCES}
    )
    target_link_libraries(pc_realtime_logger PRIVATE purrgo_core purrgo_platform_pc)

    enable_testing()

    add_executable(test_geo tests/core/test_geo.c)
    target_link_libraries(test_geo PRIVATE purrgo_core)
    add_test(NAME test_geo COMMAND test_geo)

    add_executable(test_gnss tests/core/test_gnss.c)
    target_link_libraries(test_gnss PRIVATE purrgo_core)
    add_test(NAME test_gnss COMMAND test_gnss)
endif()