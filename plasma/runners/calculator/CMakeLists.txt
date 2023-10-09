
include_directories(${QALCULATE_INCLUDE_DIR})

########### next target ###############

set(krunner_calculatorrunner_SRCS
    calculatorrunner.cpp
    qalculate_engine.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}")

kde4_add_plugin(krunner_calculatorrunner ${krunner_calculatorrunner_SRCS})
target_link_libraries(krunner_calculatorrunner
    KDE4::kdeui
    KDE4::plasma
    KDE4::solid
    KDE4::kio
    ${QALCULATE_LIBRARIES}
)

install(
    TARGETS krunner_calculatorrunner
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

########### install files ###############

install(
    FILES plasma-runner-calculator.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
