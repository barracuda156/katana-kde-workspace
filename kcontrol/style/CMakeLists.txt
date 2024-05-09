# needed for including kdecoration.h
include_directories (
    ${CMAKE_SOURCE_DIR}/kwin/libkwineffects
    ${CMAKE_SOURCE_DIR}/kwin/libkdecorations
    ${CMAKE_BINARY_DIR}/kwin/libkwineffects
)

########### next target ###############

set(kcm_style_PART_SRCS
    styleconfdialog.cpp
    kcmstyle.cpp
    stylepreview.ui
    finetuning.ui
)

kde4_add_plugin(kcm_style ${kcm_style_PART_SRCS})

target_link_libraries(kcm_style
    KDE4::kio
    KDE4::plasma
    kdecorations
    ${X11_LIBRARIES}
)

install(
    TARGETS kcm_style
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES style.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
