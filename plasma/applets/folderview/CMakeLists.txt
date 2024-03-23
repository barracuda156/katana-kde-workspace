project(plasma-folderview)

include_directories(
    ${CMAKE_SOURCE_DIR}/libs/konq
    ${CMAKE_BINARY_DIR}/libs/konq
)

set(folderview_SRCS
    folderview.cpp
    abstractitemview.cpp
    listview.cpp
    dialog.cpp
    label.cpp
    iconview.cpp
    popupview.cpp
    iconwidget.cpp
    dirlister.cpp
    proxymodel.cpp
    folderviewadapter.cpp
    previewpluginsmodel.cpp
    tooltipwidget.cpp
    style.cpp
    itemeditor.cpp
    animator.cpp
    actionoverlay.cpp
    asyncfiletester.cpp
    folderviewFilterConfig.ui
    folderviewDisplayConfig.ui
    folderviewLocationConfig.ui
    folderviewPreviewConfig.ui
)

kde4_add_plugin(plasma_applet_folderview ${folderview_SRCS})

target_link_libraries(plasma_applet_folderview
    KDE4::plasma
    KDE4::kio
    KDE4::knetworkmanager
    ${X11_LIBRARIES}
    konq
)

install(TARGETS plasma_applet_folderview DESTINATION ${KDE4_PLUGIN_INSTALL_DIR})
install(FILES plasma-applet-folderview.desktop DESTINATION ${KDE4_SERVICES_INSTALL_DIR})

