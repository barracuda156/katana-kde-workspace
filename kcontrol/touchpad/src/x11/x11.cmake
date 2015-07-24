# // krazy:excludeall=copyright,license
find_package(X11 REQUIRED)
find_package(X11_XCB REQUIRED)
find_package(XCB REQUIRED)
find_package(PkgConfig REQUIRED)

if(NOT X11_Xinput_FOUND)
    message(FATAL_ERROR "Xinput not found")
endif()

pkg_search_module(SYNAPTICS REQUIRED xorg-synaptics)
pkg_search_module(XORG REQUIRED xorg-server)

include_directories(${XCB_INCLUDE_DIR}
                    ${X11_XCB_INCLUDE_DIR}
                    ${X11_Xinput_INCLUDE_PATH}
                    ${X11_X11_INCLUDE_PATH}
                    ${SYNAPTICS_INCLUDE_DIRS}
                    ${XORG_INCLUDE_DIRS}
)

add_definitions(${X11_XCB_DEFINITIONS} ${XCB_DEFINITIONS})

SET(backend_SRCS
    ${backend_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/x11/synclientproperties.c
    ${CMAKE_CURRENT_SOURCE_DIR}/x11/xcbatom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/x11/xlibbackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/x11/xlibnotifications.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/x11/xrecordkeyboardmonitor.cpp
)

SET(backend_LIBS
    ${backend_LIBS}
    ${XCB_LIBRARIES}
    ${X11_X11_LIB}
    ${X11_XCB_LIBRARIES}
    ${X11_Xinput_LIB}
)

add_executable(kcm-touchpad-list-devices ${CMAKE_CURRENT_SOURCE_DIR}/x11/listdevices.cpp)
target_link_libraries(kcm-touchpad-list-devices
                      ${X11_X11_LIB}
                      ${X11_Xinput_LIB}
)
install(TARGETS kcm-touchpad-list-devices
        DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS}
)
