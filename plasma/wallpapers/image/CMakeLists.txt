project(plasma-wallpaper-image)

set(image_SRCS
    image.cpp
    backgrounddelegate.cpp
    backgroundlistmodel.cpp
    removebuttonmanager.cpp
    removebutton.cpp
    itemsview.cpp
)

kde4_add_plugin(plasma_wallpaper_image ${image_SRCS})
target_link_libraries(plasma_wallpaper_image KDE4::plasma KDE4::kio)

install(TARGETS plasma_wallpaper_image DESTINATION ${KDE4_PLUGIN_INSTALL_DIR})
install(FILES plasma-wallpaper-image.desktop DESTINATION ${KDE4_SERVICES_INSTALL_DIR})
