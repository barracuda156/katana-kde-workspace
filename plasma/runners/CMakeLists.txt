add_subdirectory(bookmarks)
add_subdirectory(locations)
add_subdirectory(places)
add_subdirectory(services)
add_subdirectory(recentdocuments)
add_subdirectory(shell)
add_subdirectory(solid)
add_subdirectory(webshortcuts)
add_subdirectory(windowedwidgets)
add_subdirectory(powerdevil)
add_subdirectory(sessions)
add_subdirectory(windows)
add_subdirectory(kill)

if (QALCULATE_FOUND)
    add_subdirectory(calculator)
endif()