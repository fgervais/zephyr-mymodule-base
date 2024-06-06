cmake_minimum_required(VERSION 3.20.0)

project(Base VERSION 1.8.1 LANGUAGES C ASM)

zephyr_include_directories(include)

zephyr_library_sources_ifdef(CONFIG_MY_MODULE_BASE_RESET        src/reset.c)
zephyr_library_sources_ifdef(CONFIG_MY_MODULE_BASE_UID          src/uid.c)
zephyr_library_sources_ifdef(CONFIG_MY_MODULE_BASE_WATCHDOG     src/watchdog.c)

if (CONFIG_MY_MODULE_BASE_WATCHDOG)
    list(APPEND EXTRA_CONF_FILE "config/watchdog.conf")
endif()

if (CONFIG_MY_MODULE_BASE_HOME_ASSISTANT)
    zephyr_library_sources(
        src/ha.c
        src/mqtt.c
        src/openthread.c
    )
endif()
