set(desktop_SRCS
    desktop.cpp
    itemspace.cpp
    desktoplayout.cpp
)

kde4_add_plugin(plasma_containment_desktop ${desktop_SRCS})
target_link_libraries(plasma_containment_desktop KDE4::plasma KDE4::kio)

install(TARGETS plasma_containment_desktop DESTINATION ${KDE4_PLUGIN_INSTALL_DIR})
install(FILES plasma-containment-desktop.desktop DESTINATION ${KDE4_SERVICES_INSTALL_DIR})
install(FILES plasma_desktop_containment.kwinrules DESTINATION ${KDE4_DATA_INSTALL_DIR}/kwin/default_rules)
