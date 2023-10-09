########### next target ###############

set(krunner_bookmarksrunner_SRCS
    browserfactory.cpp
    bookmarkmatch.cpp
    favicon.cpp
    bookmarksrunner.cpp
    browsers/kdebrowser.cpp
    browsers/chromefindprofile.cpp
    browsers/chrome.cpp
)

kde4_add_plugin(krunner_bookmarksrunner ${krunner_bookmarksrunner_SRCS})
target_link_libraries(krunner_bookmarksrunner
    KDE4::kio
    KDE4::plasma
    ${QT_QTDBUS_LIBRARY}
)

install(TARGETS krunner_bookmarksrunner DESTINATION ${KDE4_PLUGIN_INSTALL_DIR} )

########### install files ###############

install(FILES plasma-runner-bookmarks.desktop DESTINATION ${KDE4_SERVICES_INSTALL_DIR})

if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()
