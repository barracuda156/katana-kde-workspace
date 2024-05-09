
add_subdirectory(module)

include_directories(${X11_Xrandr_INCLUDE_PATH})

########### next target ###############

set(randrinternal_PART_SRCS 
    randr.cpp 
    randrdisplay.cpp
    collapsiblewidget.cpp
    layoutmanager.cpp 
    outputconfig.cpp
    outputgraphicsitem.cpp
    randrconfig.cpp
    randrcrtc.cpp 
    randrmode.cpp
    randroutput.cpp
    randrscreen.cpp
)

set(kcm_randr_PART_SRCS 
    ${randrinternal_PART_SRCS}
    krandrmodule.cpp 
    randrconfigbase.ui
    outputconfigbase.ui
)

kde4_add_plugin(kcm_randr ${kcm_randr_PART_SRCS})

target_link_libraries(kcm_randr
    KDE4::kdeui
    ${X11_Xrandr_LIB}
    ${X11_LIBRARIES}
)

install(
    TARGETS kcm_randr
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES randr.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

########### next target ###############

add_executable(krandr krandr.cpp)
target_link_libraries(krandr
    KDE4::kdecore
    ${X11_X11_LIB}
)

install(
    TARGETS krandr
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

install(
    FILES krandr.desktop
    DESTINATION ${KDE4_AUTOSTART_INSTALL_DIR}
)
