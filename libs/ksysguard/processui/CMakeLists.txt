
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ../processcore/
)

set(processui_LIB_SRCS
   ksysguardprocesslist.cpp
   ProcessFilter.cpp
   ProcessModel.cpp
   ReniceDlg.cpp
   ReniceDlgUi.ui
   ProcessWidgetUI.ui
)

add_library(processui SHARED ${processui_LIB_SRCS})

if(X11_XRes_FOUND)
    target_link_libraries(processui PRIVATE ${X11_XRes_LIB} ${X11_LIBRARIES})
    include_directories(${X11_XRes_INCLUDE_PATH})
endif()

target_link_libraries(processui PUBLIC KDE4::kdeui processcore)

set_target_properties(processui
    PROPERTIES VERSION ${GENERIC_LIB_VERSION}
    SOVERSION ${GENERIC_LIB_SOVERSION}
)

install(
    TARGETS processui
    EXPORT kdeworkspaceTargets
    DESTINATION ${KDE4_LIB_INSTALL_DIR}
)

#----------------------

kde4_add_widget(ksysguardwidgets_SRCS ksysguard.widgets)

kde4_add_plugin(ksysguardwidgets ${ksysguardwidgets_SRCS} ${ksysguardwidgets_PART_SRCS})

target_link_libraries(ksysguardwidgets
    KDE4::kdeui
    ${QT_QTUITOOLS_LIBRARY}
    processui
)

install(
    TARGETS ksysguardwidgets
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}/plugins/designer
)

install(
    FILES
    ProcessModel.h
    ProcessFilter.h
    ksysguardprocesslist.h
    DESTINATION ${KDE4_INCLUDE_INSTALL_DIR}/ksysguard
)

