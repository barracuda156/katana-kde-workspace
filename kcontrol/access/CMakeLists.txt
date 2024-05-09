########### next target ###############

kde4_add_plugin(kcm_access kcmaccess.cpp)

target_link_libraries(kcm_access
    KDE4::kio
    KDE4::kdeui
    ${X11_LIBRARIES}
)

install(
    TARGETS kcm_access
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)


########### next target ###############

set(kaccess_SRCS
    kaccess.cpp
    main.cpp
)

add_executable(kaccess ${kaccess_SRCS})

target_link_libraries(kaccess
    KDE4::kdeui
    ${X11_LIBRARIES}
)

install(
    TARGETS kaccess
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES kcmaccess.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

install(
    FILES kaccess.desktop
    DESTINATION ${KDE4_AUTOSTART_INSTALL_DIR}
)

install(
    FILES kaccess.notifyrc
    DESTINATION ${KDE4_CONFIG_INSTALL_DIR}/notifications
)
