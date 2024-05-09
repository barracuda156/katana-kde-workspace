
add_subdirectory( pics )

kde4_bool_to_01(LIBUSB_FOUND HAVE_LIBUSB)

configure_file (
    config-kcontrol-input.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config-kcontrol-input.h
)

include_directories(${KDE4_INCLUDES})

if (LIBUSB_FOUND)
  include_directories(${LIBUSB_INCLUDES})
endif (LIBUSB_FOUND)


if(X11_Xcursor_FOUND)
  set( libnoinst_SRCS
    xcursor/themepage.cpp
    xcursor/thememodel.cpp
    xcursor/cursortheme.cpp
    xcursor/xcursortheme.cpp
    xcursor/previewwidget.cpp
    xcursor/itemdelegate.cpp
    xcursor/sortproxymodel.cpp
    xcursor/themepage.ui
  )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/xcursor/ )
else(X11_Xcursor_FOUND)
  set( libnoinst_SRCS core/themepage.cpp )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/core/ )
endif(X11_Xcursor_FOUND)



########### next target ###############

set(kapplymousetheme_SRCS kapplymousetheme.cpp )


add_executable(kapplymousetheme ${kapplymousetheme_SRCS})

target_link_libraries(kapplymousetheme
    KDE4::kdeui
    ${X11_X11_LIB}
)
if (X11_Xcursor_FOUND)
   target_link_libraries(kapplymousetheme ${X11_Xcursor_LIB})
endif (X11_Xcursor_FOUND)

install(
    TARGETS kapplymousetheme
    DESTINATION ${KDE_BIN_INSTALL_DIR}
)


########### next target ###############

set(kcm_input_PART_SRCS mouse.cpp main.cpp logitechmouse.cpp kmousedlg.ui logitechmouse_base.ui )

kde4_add_plugin(kcm_input ${kcm_input_PART_SRCS})


target_link_libraries(kcm_input KDE4::kio ${X11_LIBRARIES})
if (LIBUSB_FOUND)
   target_link_libraries(kcm_input ${LIBUSB_LIBRARIES})
endif (LIBUSB_FOUND)
if (X11_Xcursor_FOUND)
   target_link_libraries(kcm_input ${X11_Xcursor_LIB})
endif (X11_Xcursor_FOUND)
if (X11_Xfixes_FOUND)
   target_link_libraries(kcm_input ${X11_Xfixes_LIB})
endif (X11_Xfixes_FOUND)

install(TARGETS kcm_input  DESTINATION ${KDE4_PLUGIN_INSTALL_DIR} )


########### next target ###############

set(kcm_cursortheme_PART_SRCS kcmcursortheme.cpp ${libnoinst_SRCS})

kde4_add_plugin(kcm_cursortheme ${kcm_cursortheme_PART_SRCS})


target_link_libraries(kcm_cursortheme
    KDE4::kio
    KDE4::karchive
    ${X11_LIBRARIES}
)

if (X11_Xcursor_FOUND)
   target_link_libraries(kcm_cursortheme ${X11_Xcursor_LIB})
endif (X11_Xcursor_FOUND)
if (X11_Xfixes_FOUND)
   target_link_libraries(kcm_cursortheme ${X11_Xfixes_LIB})
endif (X11_Xfixes_FOUND)

install(
    TARGETS kcm_cursortheme
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES mouse.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

install(
    FILES cursortheme.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

install(
    FILES mouse_new.desktop
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/solid/actions
)
