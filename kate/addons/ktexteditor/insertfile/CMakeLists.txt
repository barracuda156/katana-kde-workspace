
########### next target ###############

kde4_add_plugin(ktexteditor_insertfile insertfileplugin.cpp)

target_link_libraries(ktexteditor_insertfile
    KDE4::kio
    KDE4::ktexteditor
    KDE4::kdeui
)

install(
    TARGETS ktexteditor_insertfile
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES ktexteditor_insertfile.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
install(
    FILES ktexteditor_insertfileui.rc
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/ktexteditor_insertfile
)
