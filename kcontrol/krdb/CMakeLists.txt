add_subdirectory( ad ) 

add_executable(krdb main.cpp)
target_link_libraries(krdb
    KDE4::kdeui
    ${X11_X11_LIB}
)

install(
    TARGETS krdb
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

install(
    FILES krdb.desktop
    DESTINATION ${KDE4_AUTOSTART_INSTALL_DIR}
)
