set(krunner_shell_SRCS
    shellrunner.cpp
)

kde4_add_plugin(krunner_shell ${krunner_shell_SRCS})
target_link_libraries(krunner_shell KDE4::kio KDE4::plasma)

install(
    TARGETS krunner_shell
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES plasma-runner-shell.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
