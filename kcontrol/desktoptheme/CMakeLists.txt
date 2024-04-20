project(desktoptheme)

set(kcmdesktoptheme_SRCS
    kcmdesktoptheme.cpp
    thememodel.cpp
)

kde4_add_plugin(kcm_desktoptheme ${kcmdesktoptheme_SRCS})
target_link_libraries(kcm_desktoptheme
    ${X11_LIBRARIES}
    KDE4::kdeui
    KDE4::kparts
    KDE4::kcmutils
    KDE4::plasma
    KDE4::kio
    KDE4::karchive
)

install(
    TARGETS kcm_desktoptheme
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES desktoptheme.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

