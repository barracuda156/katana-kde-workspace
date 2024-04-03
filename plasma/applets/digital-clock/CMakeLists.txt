project(plasma-dig-clock)

set(digitalclock_SRCS
    digitalclock.cpp
)

kde4_add_plugin(plasma_applet_dig_clock ${digitalclock_SRCS})
target_link_libraries(plasma_applet_dig_clock
    KDE4::plasma
    KDE4::kcmutils
)

install(
    TARGETS plasma_applet_dig_clock
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES plasma-applet-digitalclock.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

