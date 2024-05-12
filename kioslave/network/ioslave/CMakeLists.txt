project(kio_network)

set(kio_network_SRCS
    networkslave.cpp
)

add_executable(kio_network ${kio_network_SRCS})

target_link_libraries(kio_network
    KDE4::kio
    KDE4::kdnssd
)

install(
    TARGETS kio_network
    DESTINATION ${KDE4_LIBEXEC_INSTALL_DIR}
)

install(
    FILES network.protocol
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
install(
    FILES network.desktop
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/remoteview
)
