project(plasma-applet-notifications)

set(notifications_SRCS
    notifications.cpp
    jobswidget.cpp
    jobtrackeradaptor.cpp
    applicationswidget.cpp
    notificationsadaptor.cpp
)

kde4_add_plugin(plasma_applet_notifications ${notifications_SRCS})
target_link_libraries(plasma_applet_notifications
    KDE4::plasma
    KDE4::kio
)

install(
    TARGETS plasma_applet_notifications
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

install(
    FILES plasma-applet-notifications.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
