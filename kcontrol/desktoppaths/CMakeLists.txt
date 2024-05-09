set(kcm_desktoppaths_PART_SRCS
    kcmdesktoppaths.cpp
)

kde4_add_plugin(kcm_desktoppaths ${kcm_desktoppaths_PART_SRCS})

target_link_libraries(kcm_desktoppaths
    KDE4::kcmutils
    KDE4::kio
)

install(
    TARGETS kcm_desktoppaths
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)
install(
    FILES desktoppath.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

install(
    FILES
    data/directory.desktop
    data/Trash.desktop
    data/Home.desktop
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/kcm_desktoppaths
)

########### next target ###############

add_executable(kdesktoppaths kdesktoppaths.cpp)
target_link_libraries(kdesktoppaths
    KDE4::kdecore
)

install(
    TARGETS kdesktoppaths
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

install(
    FILES kdesktoppaths.desktop
    DESTINATION ${KDE4_AUTOSTART_INSTALL_DIR}
)
