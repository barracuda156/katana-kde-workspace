########### next target ###############

set(kcm_clock_PART_SRCS
    kcm_clock.cpp
)
kde4_add_plugin(kcm_clock ${kcm_clock_PART_SRCS})

target_link_libraries(kcm_clock
    KDE4::kio
    KDE4::plasma
)

install(
    TARGETS kcm_clock
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### next target ###############

add_executable(kcmclockhelper kcm_clock_helper.cpp)
target_link_libraries(kcmclockhelper KDE4::kdecore)

install(
    TARGETS kcmclockhelper
    DESTINATION ${KDE4_LIBEXEC_INSTALL_DIR}
)
kde4_install_auth_helper_files(kcmclockhelper org.kde.kcontrol.kcmclock)

########### install files ###############

install(
    FILES clock.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
