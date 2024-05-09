########### next target ###############

set(keyboardconfig_SRCS 
    keyboardconfig.cpp
    keyboardlayoutdialog.cpp
    keyboardoptionsdialog.cpp
)

kde4_add_plugin(kcm_keyboard ${keyboardconfig_SRCS})

target_link_libraries(kcm_keyboard
    KDE4::kdeui
    KDE4::kcmutils
    ${X11_LIBRARIES}
    ${X11_Xkbfile_LIB}
)

install(
    TARGETS kcm_keyboard
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES kcm_keyboard.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

install(
    FILES keyboard_new.desktop
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/solid/actions
)

########### next target ###############

add_executable(kkeyboard kkeyboard.cpp)
target_link_libraries(kkeyboard
    KDE4::kdeui
    ${X11_LIBRARIES}
    ${X11_Xkbfile_LIB}
)

install(
    TARGETS kkeyboard
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

install(
    FILES kkeyboard.desktop
    DESTINATION ${KDE4_AUTOSTART_INSTALL_DIR}
)
