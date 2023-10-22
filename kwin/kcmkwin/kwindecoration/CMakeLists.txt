set(kcm_kwindecoration_PART_SRCS
    kwindecoration.cpp
)

kde4_add_plugin(kcm_kwindecoration ${kcm_kwindecoration_PART_SRCS})
target_link_libraries(kcm_kwindecoration
    KDE4::kcmutils
    KDE4::kdeui
    ${QT_QTDBUS_LIBRARY}
    kdecorations
)

install(
    TARGETS kcm_kwindecoration
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES kwindecoration.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
) 
