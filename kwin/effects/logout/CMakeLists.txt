#######################################
# Effect

# Source files
set(kwin4_effect_builtins_sources
    ${kwin4_effect_builtins_sources}
    effects/logout/logout.cpp
)

# .desktop files
install(
    FILES effects/logout/logout.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}/kwin
)
