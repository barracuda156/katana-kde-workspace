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

# install everything
install(
    TARGETS kmediaplayer
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

install(
    PROGRAMS kmediaplayer.desktop
    DESTINATION ${KDE4_XDG_APPS_INSTALL_DIR}
)

install(
    FILES kmediaplayerui.rc
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/kmediaplayer
)
