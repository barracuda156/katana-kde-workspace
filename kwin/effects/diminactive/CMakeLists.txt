#######################################
# Effect

# Source files
set(kwin4_effect_builtins_sources
    ${kwin4_effect_builtins_sources}
    effects/diminactive/diminactive.cpp
)

kde4_add_kcfg_files(
    kwin4_effect_builtins_sources
    effects/diminactive/diminactiveconfig.kcfgc
)

# .desktop files
install(
    FILES effects/diminactive/diminactive.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}/kwin
)

#######################################
# Config

# Source files
set(kwin4_effect_builtins_config_sources
    ${kwin4_effect_builtins_config_sources}
    effects/diminactive/diminactive_config.cpp
)

kde4_add_kcfg_files(
    kwin4_effect_builtins_config_sources
    effects/diminactive/diminactiveconfig.kcfgc
)

# .desktop files
install(
    FILES effects/diminactive/diminactive_config.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}/kwin
)
