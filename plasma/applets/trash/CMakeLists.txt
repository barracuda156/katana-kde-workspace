project(plasma-applet-trash)

include_directories(
    ${CMAKE_SOURCE_DIR}/libs/konq
    ${CMAKE_BINARY_DIR}/libs/konq
)

set(trash_SRCS
    trash.cpp
)

kde4_add_plugin(plasma_applet_trash ${trash_SRCS})
target_link_libraries(plasma_applet_trash
    KDE4::plasma
    KDE4::solid
    KDE4::kio
    KDE4::kcmutils
    konq
)

install(
    TARGETS plasma_applet_trash
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)
install(
    FILES plasma-applet-trash.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
