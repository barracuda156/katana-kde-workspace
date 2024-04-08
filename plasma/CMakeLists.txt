include_directories(
    # for kworkspace_export.h
    ${CMAKE_BINARY_DIR}/libs/kworkspace
)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=1204)

add_subdirectory(applets)
add_subdirectory(containmentactions)
add_subdirectory(containments)
add_subdirectory(desktoptheme)
add_subdirectory(runners)
add_subdirectory(shells)
add_subdirectory(wallpapers)
add_subdirectory(toolboxes)
add_subdirectory(sessions)
