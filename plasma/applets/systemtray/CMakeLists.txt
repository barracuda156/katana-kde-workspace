project(plasma-applet-systemtray)

set(systemtray_SRCS
    systemtray.cpp
)

kde4_add_plugin(plasma_applet_systemtray ${systemtray_SRCS})
target_link_libraries(plasma_applet_systemtray
    KDE4::plasma
)

install(
    TARGETS plasma_applet_systemtray
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES plasma-applet-systemtray.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
    RENAME plasma-applet-systemtray.desktop
)
