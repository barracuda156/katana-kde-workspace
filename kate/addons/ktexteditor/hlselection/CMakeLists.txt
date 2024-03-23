
########### next target ###############

kde4_add_plugin(ktexteditor_hlselection hlselectionplugin.cpp)

target_link_libraries(ktexteditor_hlselection
    KDE4::kio
    KDE4::ktexteditor
    KDE4::kdeui
)

install(
    TARGETS ktexteditor_hlselection
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES ktexteditor_hlselection.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

# Dominik: right now, we do not have any actions, so do not insall ui.rc file
# install( FILES ktexteditor_hlselectionui.rc  DESTINATION  ${KDE4_DATA_INSTALL_DIR}/ktexteditor_hlselection )
