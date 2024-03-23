include(CheckCXXSymbolExists)

include_directories( ${KDE4_INCLUDES} )
add_definitions( -DKDE_DEFAULT_DEBUG_AREA=1208 )

# info_fbsd.cpp
check_include_files(devinfo.h HAVE_DEVINFO_H)

# opengl.cpp
if(LIBDRM_FOUND)
    cmake_reset_check_state()
    set(CMAKE_REQUIRED_INCLUDES ${LIBDRM_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES ${LIBDRM_LIBRARIES})
    check_symbol_exists(drmIsKMS "xf86drmMode.h" HAVE_DRMISKMS)
    cmake_reset_check_state()
else()
    set(HAVE_DRMISKMS FALSE)
endif()

configure_file (
    config-infocenter.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config-infocenter.h
)
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_subdirectory( Categories )
add_subdirectory( Modules )

include_directories(
    ToolTips
)

set( kinfocenter_SRCS
    infocenter.cpp
    main.cpp
    sidepanel.cpp
    kcmcontainer.cpp
    kcmtreeitem.cpp
    kcmcategoryitem.cpp
    infokcmmodel.cpp
    infokcmproxymodel.cpp
    ToolTips/ktooltip.cpp
    ToolTips/ktooltipwindow.cpp
    ToolTips/tooltipmanager.cpp
)

add_executable(kinfocenter ${kinfocenter_SRCS} )

target_link_libraries(kinfocenter
    KDE4::kio
    KDE4::kcmutils
)

##install
install(
    TARGETS kinfocenter
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)
install(
    PROGRAMS kinfocenter.desktop
    DESTINATION ${KDE4_XDG_APPS_INSTALL_DIR}
)
INSTALL(
    FILES kinfocenterui.rc
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/kinfocenter
)
