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
)

set(plasmaapp_dbusXML dbus/org.kde.plasma.App.xml)
qt4_add_dbus_adaptor(plasma_SRCS ${plasmaapp_dbusXML} plasmaapp.h PlasmaApp)

add_executable(plasma-desktop ${plasma_SRCS})

target_link_libraries(plasma-desktop
    KDE4::plasma
    KDE4::kio
    KDE4::kfile
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
    FILES data/plasma-desktop.desktop
    DESTINATION ${KDE4_AUTOSTART_INSTALL_DIR}
)

install(
    FILES data/plasma-desktoprc
    DESTINATION ${KDE4_CONFIG_INSTALL_DIR}
)
