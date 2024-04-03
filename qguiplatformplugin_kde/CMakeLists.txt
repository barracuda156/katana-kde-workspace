project(qguiplatformplugin_kde)

set(kde_SOURCES qguiplatformplugin_kde.cpp)

kde4_add_plugin(kde ${kde_SOURCES})

target_link_libraries(kde KDE4::kdeui ${QT_QTGUI_LIBRARY})
if (X11_Xcursor_FOUND)
    target_link_libraries(kde ${X11_Xcursor_LIB})
endif()

install(
    TARGETS kde LIBRARY
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}/plugins/gui_platform
)
