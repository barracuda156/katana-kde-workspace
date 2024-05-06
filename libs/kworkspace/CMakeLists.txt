set(kworkspace_LIB_SRCS
    kdisplaymanager.cpp
    screenpreviewwidget.cpp
    kworkspace.cpp
    ktaskmanager.cpp
)


set(plasma_xml ${CMAKE_SOURCE_DIR}/plasma/shells/plasma-desktop/dbus/org.kde.plasma.App.xml)
qt4_add_dbus_interface(kworkspace_LIB_SRCS ${plasma_xml} plasma_interface)

set(kwin_xml ${CMAKE_SOURCE_DIR}/kwin/org.kde.KWin.xml)

set_source_files_properties(${kwin_xml} PROPERTIES INCLUDE "interface_util.h")

qt4_add_dbus_interface(kworkspace_LIB_SRCS ${kwin_xml} kwin_interface)

add_library(kworkspace SHARED ${kworkspace_LIB_SRCS})

target_link_libraries(kworkspace PRIVATE
    ${X11_Xau_LIB}
    ${X11_SM_LIB}
    ${X11_ICE_LIB}
)

target_link_libraries(kworkspace PUBLIC
    KDE4::kdeui
    KDE4::plasma
)

set_target_properties(kworkspace PROPERTIES
    VERSION ${GENERIC_LIB_VERSION}
    SOVERSION ${GENERIC_LIB_SOVERSION}
)

generate_export_header(kworkspace)

install(
    TARGETS kworkspace
    EXPORT kdeworkspaceTargets
    DESTINATION ${KDE4_LIB_INSTALL_DIR}
)

install(
    FILES
    kdisplaymanager.h
    kworkspace.h
    ktaskmanager.h
    ${CMAKE_CURRENT_BINARY_DIR}/kworkspace_export.h
    DESTINATION ${KDE4_INCLUDE_INSTALL_DIR}/kworkspace
)
