if(FONTCONFIG_FOUND)
    include_directories(${FONTCONFIG_INCLUDE_DIR})
endif(FONTCONFIG_FOUND)
include_directories(${FREETYPE_INCLUDE_DIRS})

########### next target ###############

set(kcm_fonts_PART_SRCS
    fonts.cpp
    ${libkxftconfig_SRCS}
)

kde4_add_plugin(kcm_fonts ${kcm_fonts_PART_SRCS})

target_link_libraries(kcm_fonts
    KDE4::kdeui
    ${QT_QTXML_LIBRARY}
    ${FREETYPE_LIBRARIES}
    ${X11_LIBRARIES}
)

if(FONTCONFIG_FOUND)
    target_link_libraries(kcm_fonts ${FONTCONFIG_LIBRARIES})
endif(FONTCONFIG_FOUND)

########### install files ###############

install(
    TARGETS kcm_fonts
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES fonts.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)

install(
    FILES
    data/aa_rgb.png
    data/aa_bgr.png
    data/aa_vrgb.png
    data/aa_vbgr.png
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/kcmfonts/pics
)
