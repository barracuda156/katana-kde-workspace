include_directories(
    ${CMAKE_SOURCE_DIR}/libs/konq
    ${CMAKE_BINARY_DIR}/libs/konq
)

########### konqmimedatatest ###############

kde4_add_test(libkonq-konqmimedatatest konqmimedatatest.cpp)

target_link_libraries(libkonq-konqmimedatatest konq
    KDE4::kdecore
    ${QT_QTGUI_LIBRARY}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTTEST_LIBRARY}
)

########### konqpopupmenutest ###############

kde4_add_test(libkonq-konqpopupmenutest konqpopupmenutest.cpp)

target_link_libraries(libkonq-konqpopupmenutest
    KDE4::kio
    ${QT_QTCORE_LIBRARY}
    ${QT_QTGUI_LIBRARY}
    ${QT_QTTEST_LIBRARY}
    konq
)

########### favicontest ###############

set(favicontest_SRCS favicontest.cpp)

qt4_add_dbus_interface(favicontest_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../favicons/org.kde.FavIcon.xml
    favicon_interface
)

kde4_add_test(libkonq-favicontest ${favicontest_SRCS})

target_link_libraries(libkonq-favicontest
    KDE4::kdecore
    KDE4::kio
    KDE4::knetworkmanager
    ${QT_QTCORE_LIBRARY}
    ${QT_QTGUI_LIBRARY}
    ${QT_QTDBUS_LIBRARY}
    ${QT_QTTEST_LIBRARY}
    konq
)
