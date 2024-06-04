cmake_minimum_required(VERSION 3.20.0)

project(Base VERSION 1.8.1 LANGUAGES C ASM)

zephyr_include_directories(
    include
)

zephyr_library_sources_ifdef(CONFIG_MY_MODULE_BASE_RESET        src/reset.c)
zephyr_library_sources_ifdef(CONFIG_MY_MODULE_BASE_UID          src/uid.c)
zephyr_library_sources_ifdef(CONFIG_MY_MODULE_BASE_WATCHDOG     src/watchdog.c)
