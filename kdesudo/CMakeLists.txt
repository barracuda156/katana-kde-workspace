include(CheckSymbolExists)
include(CheckIncludeFiles)

# Linux
check_symbol_exists(PR_SET_DUMPABLE "sys/prctl.h" HAVE_PR_SET_DUMPABLE)
add_feature_info("pr_set_dumpable" HAVE_PR_SET_DUMPABLE "Used to disallow process tracing")

# FreeBSD
check_symbol_exists(procctl "sys/procctl.h" HAVE_PROCCTL)
add_feature_info("procctl" HAVE_PROCCTL "Used to disallow process tracing")

configure_file(
    config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=900)

set(kdesudo_SRC
    kdesudo.cpp
)

add_executable(kdesudo ${kdesudo_SRC})
target_link_libraries(kdesudo
    KDE4::kdeui
)

set(kaskpass_SRC
    kaskpass.cpp
)

add_executable(kaskpass ${kaskpass_SRC})
target_link_libraries(kaskpass
    KDE4::kdeui
)

install(
    TARGETS kdesudo kaskpass
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)
