set(libkxftconfig_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/kxftconfig.cpp
)

if(X11_Xrandr_FOUND)
   add_subdirectory( randr )
endif()

if(X11_Xkb_FOUND AND X11_Xkbfile_FOUND)
    add_subdirectory( keyboard )
    add_subdirectory( access )
endif()

add_subdirectory( input )
add_subdirectory( dateandtime )
add_subdirectory( autostart )

add_subdirectory( launch )
add_subdirectory( colors )
add_subdirectory( krdb )
add_subdirectory( style )
add_subdirectory( desktoptheme )
add_subdirectory( standard_actions )
add_subdirectory( workspaceoptions )

add_subdirectory( icons )
add_subdirectory( locale )
add_subdirectory( kded )
add_subdirectory( componentchooser )
add_subdirectory( mediaplayer )
add_subdirectory( menus )
add_subdirectory( spellchecking )

add_subdirectory( kdebug )
add_subdirectory( kmetainfo )
add_subdirectory( kpasswdstore )

add_subdirectory( desktoppaths )

if(FONTCONFIG_FOUND AND FREETYPE_FOUND)
    add_subdirectory( fonts )
endif()

add_subdirectory( ebrowsing )
add_subdirectory( kio )

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    # this one doesn't seem to be very portable, Alex
    add_subdirectory(joystick)
endif()