include_directories(
    ${CMAKE_SOURCE_DIR}/libs/konq
    ${CMAKE_BINARY_DIR}/libs/konq
)

set(kfind_SRCS
    main.cpp
    kfinddlg.cpp
    kftabdlg.cpp
    kquery.cpp
    kdatecombo.cpp
    kfindtreeview.cpp
)

add_executable(kfind ${kfind_SRCS})

target_link_libraries(kfind
    KDE4::kio
    KDE4::karchive
    konq
)

install(
    TARGETS kfind
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

########### install files ###############

install(
    PROGRAMS kfind.desktop
    DESTINATION ${KDE4_XDG_APPS_INSTALL_DIR}
)

install(
    FILES findhere.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}/ServiceMenus
)

kde4_install_icons(${KDE4_ICON_INSTALL_DIR})
