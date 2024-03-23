project(plasma-applet-trash)

set(trash_SRCS
    trash.cpp)

kde4_add_plugin(plasma_applet_trash ${trash_SRCS})
target_link_libraries(plasma_applet_trash KDE4::plasma KDE4::solid KDE4::kio KDE4::kcmutils)

install(TARGETS plasma_applet_trash DESTINATION ${KDE4_PLUGIN_INSTALL_DIR})
install(FILES plasma-applet-trash.desktop DESTINATION ${KDE4_SERVICES_INSTALL_DIR})
