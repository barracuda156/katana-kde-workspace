include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/CContLib/ )

# Laurent: Verify that we install it into (kdeprefix)/etc/ and not into /etc
# otherwise it's necessary to change it.
# into kde 3.x it was installed into /etc/
add_definitions( -DKSYSGUARDDRCFILE="${KDE4_SYSCONF_INSTALL_DIR}/ksysguarddrc" )

kde4_bool_to_01(SENSORS_FOUND HAVE_LMSENSORS)

configure_file(config-ksysguardd.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-ksysguardd.h)

if(CMAKE_SYSTEM_NAME MATCHES "(Linux|GNU)")
    add_definitions(-DOSTYPE_${CMAKE_SYSTEM_NAME})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Linux)
    include(Linux/Linux.cmake)
elseif(CMAKE_SYSTEM_NAME MATCHES "(FreeBSD|DragonFly)")
    add_definitions(-DOSTYPE_${CMAKE_SYSTEM_NAME})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/FreeBSD)
    include(FreeBSD/FreeBSD.cmake)
elseif(CMAKE_SYSTEM_NAME MATCHES "(SunOS|Solaris)")
    add_definitions(-DOSTYPE_Solaris)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Solaris)
    include(Solaris/Solaris.cmake)
elseif(CMAKE_SYSTEM_NAME MATCHES "(NetBSD|OpenBSD)")
    add_definitions(-DOSTYPE_${CMAKE_SYSTEM_NAME})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_SYSTEM_NAME})
    include(${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_NAME}.cmake)
endif()

########### next target ###############

set(ksysguardd_SRCS
    CContLib/ccont.c
    Command.c 
    conf.c 
    ksysguardd.c 
    PWUIDCache.c
    ${LIBKSYSGUARDD_SOURCES}
)

add_executable(ksysguardd ${ksysguardd_SRCS})
target_link_libraries(ksysguardd ${LIBKSYSGUARDD_LIBS})

if(CMAKE_SYSTEM_NAME MATCHES "DragonFly")
    message(STATUS "Adding kinfo library on DragonFly BSD")
    target_link_libraries(ksysguardd kinfo)
endif()

install(
    TARGETS ksysguardd
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)
