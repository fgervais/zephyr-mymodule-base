cmake_minimum_required(VERSION 3.20.0)

project(Base VERSION 1.8.1 LANGUAGES C ASM)

if(CONFIG_MY_WATCHDOG_MODULE)
    zephyr_include_directories(
        include
    )

    zephyr_library_sources_ifdef(CONFIG_MY_WATCHDOG_MODULE  src/watchdog.c)




    # zephyr_library_sources_ifdef(CONFIG_MY_WATCHDOG_MODULE  src/watchdog.c)

    # install(FILES ${CMAKE_BINARY_DIR}/picolibc.specs DESTINATION lib)
    # install(FILES ${CMAKE_BINARY_DIR}/picolibc/include/picolibc.h DESTINATION include)
endif()