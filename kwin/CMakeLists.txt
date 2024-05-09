########### configure tests ###############

option(KWIN_BUILD_KCMS "Enable building of KWin configuration modules." ON)
option(KWIN_BUILD_SCREENEDGES "Enable building of KWin with screen edge support" ON)
option(KWIN_BUILD_COMPOSITE "Enable building of KWin with XRender Compositing support" ON)

include_directories(
    ${XCB_INCLUDE_DIR}
    # for kworkspace_export.h
    ${CMAKE_BINARY_DIR}/libs/kworkspace
)

# for kwin internal things
configure_file(config-kwin.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h)

########### global ###############

include_directories(BEFORE
    ${CMAKE_CURRENT_BINARY_DIR}/libkwineffects
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libkwineffects
    ${CMAKE_CURRENT_SOURCE_DIR}/libkdecorations
    ${CMAKE_CURRENT_SOURCE_DIR}/effects
)

add_subdirectory( libkdecorations )
add_subdirectory( libkwineffects )
add_subdirectory( killer )
if(KWIN_BUILD_KCMS)
    add_subdirectory( kcmkwin )
endif()

add_subdirectory( clients )


# Install the KWin/Effect service type
install(FILES effects/kwineffect.desktop DESTINATION ${KDE4_SERVICETYPES_INSTALL_DIR})

# Create initial variables
set(kwin4_effect_builtins_sources)
set(kwin4_effect_builtins_config_sources effects/configs_builtins.cpp)
set(kwin4_effect_include_directories)

include( effects/presentwindows/CMakeLists.txt )
include( effects/slidingpopups/CMakeLists.txt )
include( effects/taskbarthumbnail/CMakeLists.txt )
include( effects/diminactive/CMakeLists.txt )
include( effects/dimscreen/CMakeLists.txt )
include( effects/highlightwindow/CMakeLists.txt )
include( effects/minimizeanimation/CMakeLists.txt )
include( effects/resize/CMakeLists.txt )
include( effects/showfps/CMakeLists.txt )
include( effects/showpaint/CMakeLists.txt )
include( effects/slide/CMakeLists.txt )
include( effects/slideback/CMakeLists.txt )
include( effects/thumbnailaside/CMakeLists.txt )
include( effects/windowgeometry/CMakeLists.txt )
include( effects/zoom/CMakeLists.txt )
include( effects/logout/CMakeLists.txt )
include( effects/magnifier/CMakeLists.txt )
include( effects/mousemark/CMakeLists.txt )
include( effects/snaphelper/CMakeLists.txt )
include( effects/trackmouse/CMakeLists.txt )
include( effects/startupfeedback/CMakeLists.txt )

set_source_files_properties(
    ${kwin4_effect_builtins_config_sources}
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

kde4_add_plugin(kcm_kwin4_effect_builtins ${kwin4_effect_builtins_config_sources})
target_link_libraries(kcm_kwin4_effect_builtins kwineffects KDE4::kio KDE4::kdeui)
install(TARGETS kcm_kwin4_effect_builtins DESTINATION ${KDE4_PLUGIN_INSTALL_DIR})

########### next target ###############

set(kwin_SRCS
   workspace.cpp 
   dbusinterface.cpp
   client.cpp 
   client_machine.cpp
   cursor.cpp
   tabgroup.cpp
   focuschain.cpp
   netinfo.cpp
   placement.cpp 
   atoms.cpp 
   utils.cpp 
   layers.cpp 
   main.cpp 
   options.cpp 
   decorations.cpp
   events.cpp 
   killwindow.cpp 
   geometrytip.cpp 
   screens.cpp
   shadow.cpp
   sm.cpp 
   group.cpp 
   bridge.cpp 
   manage.cpp 
   overlaywindow.cpp
   activation.cpp 
   useractions.cpp 
   geometry.cpp 
   rules.cpp
   composite.cpp
   toplevel.cpp
   unmanaged.cpp
   scene.cpp
   scene_xrender.cpp
   deleted.cpp
   effects.cpp
   compositingprefs.cpp
   paintredirector.cpp
   virtualdesktops.cpp
   xcbutils.cpp
)

if(KWIN_BUILD_SCREENEDGES)
    set(kwin_SRCS
        ${kwin_SRCS}
        screenedge.cpp
    )
endif()

kde4_add_kcfg_files(kwin_SRCS settings.kcfgc)

qt4_add_dbus_adaptor(kwin_SRCS org.kde.KWin.xml dbusinterface.h KWin::DBusInterface)
qt4_add_dbus_adaptor(kwin_SRCS org.kde.kwin.Compositing.xml composite.h KWin::Compositor)
qt4_add_dbus_adaptor(kwin_SRCS org.kde.kwin.Effects.xml effects.h KWin::EffectsHandlerImpl)

########### target link libraries ###############

if (NOT X11_xf86vmode_FOUND)
    add_definitions(-DKWIN_NO_XF86VM)
endif()

add_executable(kwin ${kwin_SRCS} ${kwin4_effect_builtins_sources})

target_link_libraries(kwin
    KDE4::kdeui
    ${X11_X11_LIB}
    ${X11_Xext_LIB}
    ${X11_Xcursor_LIB}
    ${X11_Xrandr_LIB}
    ${X11_Xdamage_LIB}
    ${X11_Xrender_LIB}
    ${X11_Xfixes_LIB}
    ${X11_XCB_LIBRARIES}
    ${XCB_XCB_LIBRARIES}
    ${XCB_XFIXES_LIBRARIES}
    ${XCB_DAMAGE_LIBRARIES}
    ${XCB_COMPOSITE_LIBRARIES}
    ${XCB_SHAPE_LIBRARIES}
    ${XCB_SYNC_LIBRARIES}
    ${XCB_RENDER_LIBRARIES}
    ${XCB_RANDR_LIBRARIES}
    ${XCB_KEYSYMS_LIBRARIES}
    kdecorations
    kwineffects
)

if(X11_xf86vmode_FOUND)
    target_link_libraries(kwin ${X11_Xxf86vm_LIB})
endif()

install(
    TARGETS kwin
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES kwin.notifyrc
    DESTINATION ${KDE4_CONFIG_INSTALL_DIR}/notifications
)

install(
    FILES org.kde.KWin.xml
    DESTINATION ${KDE4_DBUS_INTERFACES_INSTALL_DIR}
)

kde4_install_icons(${KDE4_ICON_INSTALL_DIR})

if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()
