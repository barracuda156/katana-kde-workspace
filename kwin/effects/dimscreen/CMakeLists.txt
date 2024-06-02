#######################################
# Effect

# Source files
set(kwin4_effect_builtins_sources
    ${kwin4_effect_builtins_sources}
    effects/dimscreen/dimscreen.cpp
)

kde4_add_kcfg_files(
    kwin4_effect_builtins_sources
    effects/dimscreen/dimscreenconfig.kcfgc
)

# .desktop files
install(
    FILES effects/dimscreen/dimscreen.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}/kwin
)


#######################################
# Config

# Source files
set(kwin4_effect_builtins_config_sources
    ${kwin4_effect_builtins_config_sources}
    effects/dimscreen/dimscreen_config.cpp
)

kde4_add_kcfg_files(
    kwin4_effect_builtins_config_sources
    effects/dimscreen/dimscreenconfig.kcfgc
)

# .desktop files
install(
    FILES effects/dimscreen/dimscreen_config.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}/kwin
)