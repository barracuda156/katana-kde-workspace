include_directories(
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/kworkspace
)


set(plasma-windowed_SRCS
    singleview.cpp
    main.cpp
    plasmaapp.cpp
    ${appletbrowserdialog_SRCS}
)

add_executable(plasma-windowed ${plasma-windowed_SRCS})

target_link_libraries(plasma-windowed
    KDE4::plasma
    KDE4::kio
    ${X11_LIBRARIES}
    kworkspace
)

install(
    TARGETS plasma-windowed
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

