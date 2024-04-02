project(plasma-system-monitor)

set(systemmonitor_SRCS
    system-monitor.cpp
)

kde4_add_plugin(plasma_applet_system-monitor ${systemmonitor_SRCS})
target_link_libraries(plasma_applet_system-monitor
    KDE4::plasma
    KDE4::solid
    ksgrd
)

install(
    TARGETS plasma_applet_system-monitor
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES plasma-applet-system-monitor.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
