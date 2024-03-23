
set(krunner_placesrunner_SRCS
    placesrunner.cpp
)

kde4_add_plugin(krunner_placesrunner ${krunner_placesrunner_SRCS})
target_link_libraries(
    krunner_placesrunner
    KDE4::solid
    KDE4::kio
    KDE4::plasma
)

install(TARGETS krunner_placesrunner DESTINATION ${KDE4_PLUGIN_INSTALL_DIR} )

install(FILES plasma-runner-places.desktop DESTINATION ${KDE4_SERVICES_INSTALL_DIR})

