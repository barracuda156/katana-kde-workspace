set(krunner_sessions_SRCS
    sessionrunner.cpp
)

kde4_add_plugin(krunner_sessions ${krunner_sessions_SRCS})
target_link_libraries(krunner_sessions
    KDE4::kio
    KDE4::plasma
    kworkspace
)

install(
    TARGETS krunner_sessions
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES plasma-runner-sessions.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
