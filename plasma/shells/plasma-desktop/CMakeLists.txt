include_directories(
    ${CMAKE_SOURCE_DIR}/libs 
    ${CMAKE_SOURCE_DIR}/libs/plasmagenericshell
)

set(plasma_SRCS
    controllerwindow.cpp
    desktopcorona.cpp
    desktopview.cpp
    desktoptracker.cpp
    main.cpp
    panelcontroller.cpp
    panelview.cpp
    panelapplethandle.cpp
    panelappletoverlay.cpp
    plasmaapp.cpp
    positioningruler.cpp
    shutdowndlg.cpp
)

set(plasmaapp_dbusXML dbus/org.kde.plasma.App.xml)
qt4_add_dbus_adaptor(plasma_SRCS ${plasmaapp_dbusXML} plasmaapp.h PlasmaApp)

set(kapplication_xml ${KDE4_DBUS_INTERFACES_INSTALL_DIR}/org.kde.KApplication.xml)
qt4_add_dbus_interface(plasma_SRCS ${kapplication_xml} kapplication_interface)

add_executable(plasma-desktop ${plasma_SRCS})

target_link_libraries(plasma-desktop
    KDE4::plasma
    KDE4::kio
    KDE4::ktexteditor
    KDE4::kidletime
    ${X11_LIBRARIES}
    kworkspace
    plasmagenericshell
)

install(
    TARGETS plasma-desktop
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

install(
    FILES data/plasma-desktoprc
    DESTINATION ${KDE4_CONFIG_INSTALL_DIR}
)
