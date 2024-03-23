set(katefilebrowserplugin_PART_SRCS
    katefilebrowserplugin.cpp
    katefilebrowserconfig.cpp
    katefilebrowser.cpp
    katebookmarkhandler.cpp
)

kde4_add_plugin(katefilebrowserplugin ${katefilebrowserplugin_PART_SRCS})

target_link_libraries(katefilebrowserplugin
    KDE4::kio
    KDE4::kparts
    kateinterfaces
)

install(
    TARGETS katefilebrowserplugin
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES katefilebrowserplugin.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
