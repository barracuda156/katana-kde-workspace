set(kcm_colors_PART_SRCS
    colorscm.cpp
    previewwidget.cpp
    setpreviewwidget.cpp
)

kde4_add_plugin(kcm_colors ${kcm_colors_PART_SRCS})
target_link_libraries(kcm_colors
    KDE4::kio
    KDE4::kdeui
    ${X11_LIBRARIES}
)

install(
    TARGETS kcm_colors
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)
install(
    FILES colors.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

install(
    FILES
    schemes/Honeycomb.colors
    schemes/Norway.colors
    schemes/ObsidianCoast.colors
    schemes/Oxygen.colors
    schemes/OxygenDark.colors
    schemes/Steel.colors
    schemes/WontonSoup.colors
    schemes/Zion.colors
    schemes/ZionReversed.colors
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/color-schemes
)
