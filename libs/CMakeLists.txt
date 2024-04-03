include_directories(
    # for konq_export.h
    ${CMAKE_BINARY_DIR}/libs
)

add_subdirectory(konq)
add_subdirectory(kworkspace)
add_subdirectory(oxygen)
add_subdirectory(plasmagenericshell)
add_subdirectory(ksysguard)

