project(KMediaPlayer)

# application
add_executable(kmediaplayer main.cpp kmediawindow.cpp)

target_link_libraries(kmediaplayer
    KDE4::kdecore
    KDE4::kdeui
    KDE4::kio
    KDE4::kcmutils
    KDE4::kmediaplayer
    KDE4::solid
)

# part
kde4_add_plugin(kmediaplayerpart kmediaplayerpart.cpp)

target_link_libraries(kmediaplayerpart
    KDE4::kdecore
    KDE4::kdeui
    KDE4::kparts
    KDE4::kmediaplayer
)

# install everything
install(
    TARGETS kmediaplayer
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

install(
    TARGETS kmediaplayerpart
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    PROGRAMS kmediaplayer.desktop
    DESTINATION ${KDE4_XDG_APPS_INSTALL_DIR}
)

install(
    PROGRAMS kmediaplayerpart.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

install(
    FILES kmediaplayerui.rc
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/kmediaplayer
)
