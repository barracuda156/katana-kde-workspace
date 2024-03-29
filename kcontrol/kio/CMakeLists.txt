########### next target ###############

set(kcm_kio_PART_SRCS
    main.cpp
    netpref.cpp
    bookmarks.cpp
    ksaveioconfig.cpp
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/netpref.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bookmarks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ksaveioconfig.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

kde4_add_plugin(kcm_kio ${kcm_kio_PART_SRCS})

target_link_libraries(kcm_kio
    KDE4::kcmutils
    KDE4::kio
)

########### install files ###############

install(
    TARGETS kcm_kio
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES
    bookmarks.desktop
    netpref.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
