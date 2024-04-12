project(plasma-applet-launcher)

set(launcher_SRCS
    launcher.cpp
)

kde4_add_plugin(plasma_applet_launcher ${launcher_SRCS})
target_link_libraries(plasma_applet_launcher
    KDE4::plasma
    KDE4::kio
    KDE4::solid
    KDE4::kcmutils
    ${QT_QTNETWORK_LIBRARY}
    kworkspace
)

install(
    TARGETS plasma_applet_launcher
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES plasma-applet-launcher.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
