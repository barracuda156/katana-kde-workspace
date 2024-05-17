########### next target ###############

add_subdirectory( processcore )
add_subdirectory( processui )
add_subdirectory( signalplotter )
add_subdirectory( ksgrd )
if(ENABLE_TESTING)
    add_subdirectory( tests )
endif()

