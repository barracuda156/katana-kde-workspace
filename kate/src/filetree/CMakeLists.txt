project(katefiletreeplugin)

set(katefiletreeplugin_PART_SRCS
    katefiletreeplugin.cpp
    katefiletree.cpp
    katefiletreemodel.cpp
    katefiletreeproxymodel.cpp
    katefiletreeconfigpage.cpp
    katefiletreepluginsettings.cpp
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

kde4_add_plugin(katefiletreeplugin ${katefiletreeplugin_PART_SRCS})

target_link_libraries(katefiletreeplugin
    KDE4::kio
    KDE4::kparts
    KDE4::ktexteditor
    kateinterfaces
)

install(
    TARGETS katefiletreeplugin
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES ui.rc
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/kate/plugins/filetree
)
install(
    FILES katefiletreeplugin.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
