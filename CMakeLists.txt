cmake_minimum_required(VERSION 3.20.0)

# ---------------------------------------------------------------------------
# Zephyr package discovery
#
# ZEPHYR_BASE must be set in your environment (e.g. by sourcing zephyr-env.sh
# or running `west build`).
# ---------------------------------------------------------------------------
# Explicitly apply our board overlay (Zephyr 4.x qualified board targets
# do not auto-discover overlays named after just the board root name).
set(DTC_OVERLAY_FILE "${CMAKE_SOURCE_DIR}/boards/esp32s3_devkitc.overlay")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(gear_guardian
    VERSION 0.1.0
    DESCRIPTION "Gear Guardian — Zephyr-based embedded anti-theft device"
    LANGUAGES C
)

# ---------------------------------------------------------------------------
# Application source files
# ---------------------------------------------------------------------------
target_sources(app PRIVATE
    # --- Entry point and boot coordinator ---
    app/src/main.c
    app/src/startup.c

    # --- Mode management ---
    app/src/mode_manager.c
    app/src/normal_mode.c
    app/src/diag_mode.c
    app/src/safe_mode.c

    # --- Core services ---
    app/src/state_machine.c
    app/src/event_dispatcher.c
    app/src/fault_manager.c
    app/src/boot_metadata.c

    # --- Application services ---
    app/src/sensor_thread.c
    app/src/motion_classifier.c
    app/src/logger_service.c
    app/src/ui_service.c
    app/src/alarm_service.c
)

# ---------------------------------------------------------------------------
# Driver source files
# ---------------------------------------------------------------------------
target_sources(app PRIVATE
    drivers/icm20948/icm20948.c
    drivers/reed_switch/reed_switch.c
    drivers/buzzer/buzzer.c
    drivers/oled/oled.c
    drivers/sdlog/sdlog.c
)

# ---------------------------------------------------------------------------
# Include paths
#
# All project headers are included without path prefixes in source files
# (e.g. #include "app_events.h" not #include "app/include/app_events.h").
# ---------------------------------------------------------------------------
target_include_directories(app PRIVATE
    app/include
    drivers/icm20948
    drivers/reed_switch
    drivers/buzzer
    drivers/oled
    drivers/sdlog
)
