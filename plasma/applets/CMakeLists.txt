add_subdirectory(icon)
add_subdirectory(lockout)
add_subdirectory(panelspacer)
add_subdirectory(batterymonitor)
add_subdirectory(calendar)
add_subdirectory(calculator)
add_subdirectory(devicenotifier)
add_subdirectory(digital-clock)
add_subdirectory(quicklaunch)
add_subdirectory(system-monitor)
add_subdirectory(notifications)
add_subdirectory(systemtray)
add_subdirectory(keyboard)
add_subdirectory(kickoff)
add_subdirectory(trash)
add_subdirectory(folderview)
add_subdirectory(weather)
add_subdirectory(pager)
add_subdirectory(luna)

if (ALSA_FOUND)
    add_subdirectory(mixer)
endif()

# tasks depend on libs/kworkspace
include_directories(
    # for kworkspace_export.h
    ${CMAKE_BINARY_DIR}/libs
    ${CMAKE_BINARY_DIR}/libs/kworkspace
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/kworkspace
)

add_subdirectory(tasks)
