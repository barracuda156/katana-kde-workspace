project(plasma-wallpaper-potd)

set(potd_SRCS
    potd.cpp
)

kde4_add_plugin(plasma_wallpaper_potd ${potd_SRCS})
target_link_libraries(plasma_wallpaper_potd
    KDE4::plasma
    KDE4::kdeui
    KDE4::kio
)

install(
    TARGETS plasma_wallpaper_potd
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES
    plasma-wallpaper-potd.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
