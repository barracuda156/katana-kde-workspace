include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/libs/konq
    ${CMAKE_BINARY_DIR}/libs/konq
)

if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()

########### next target ###############

set(dolphinprivate_LIB_SRCS
    kitemviews/kfileitemlistview.cpp
    kitemviews/kfileitemlistwidget.cpp
    kitemviews/kfileitemmodel.cpp
    kitemviews/kfileitemmodelrolesupdater.cpp
    kitemviews/kitemlistcontainer.cpp
    kitemviews/kitemlistcontroller.cpp
    kitemviews/kitemlistgroupheader.cpp
    kitemviews/kitemlistheader.cpp
    kitemviews/kitemlistselectionmanager.cpp
    kitemviews/kitemliststyleoption.cpp
    kitemviews/kitemlistview.cpp
    kitemviews/kitemlistwidget.cpp
    kitemviews/kitemmodelbase.cpp
    kitemviews/kitemset.cpp
    kitemviews/kstandarditem.cpp
    kitemviews/kstandarditemlistgroupheader.cpp
    kitemviews/kstandarditemlistwidget.cpp
    kitemviews/kstandarditemlistview.cpp
    kitemviews/kstandarditemmodel.cpp
    kitemviews/private/kdirectorycontentscounter.cpp
    kitemviews/private/kdirectorycontentscounterworker.cpp
    kitemviews/private/kfileitemclipboard.cpp
    kitemviews/private/kfileitemmodeldirlister.cpp
    kitemviews/private/kfileitemmodelfilter.cpp
    kitemviews/private/kitemlistheaderwidget.cpp
    kitemviews/private/kitemlistkeyboardsearchmanager.cpp
    kitemviews/private/kitemlistroleeditor.cpp
    kitemviews/private/kitemlistrubberband.cpp
    kitemviews/private/kitemlistselectiontoggle.cpp
    kitemviews/private/kitemlistsizehintresolver.cpp
    kitemviews/private/kitemlistsmoothscroller.cpp
    kitemviews/private/kitemlistviewanimation.cpp
    kitemviews/private/kitemlistviewlayouter.cpp
    settings/additionalinfodialog.cpp
    settings/applyviewpropsjob.cpp
    settings/viewmodes/viewmodesettings.cpp
    settings/viewpropertiesdialog.cpp
    settings/viewpropsprogressinfo.cpp
    views/dolphinfileitemlistwidget.cpp
    views/dolphinitemlistview.cpp
    views/dolphinnewfilemenuobserver.cpp
    views/dolphinremoteencoding.cpp
    views/dolphinview.cpp
    views/dolphinviewactionhandler.cpp
    views/draganddrophelper.cpp
    views/renamedialog.cpp
    views/tooltips/filemetadatatooltip.cpp
    views/tooltips/tooltipmanager.cpp
    views/versioncontrol/updateitemstatesthread.cpp
    views/versioncontrol/versioncontrolobserver.cpp
    views/viewmodecontroller.cpp
    views/viewproperties.cpp
    views/zoomlevelinfo.cpp
    dolphinremoveaction.cpp
    dolphinnewfilemenu.cpp
)

kde4_add_kcfg_files(dolphinprivate_LIB_SRCS
    settings/dolphin_compactmodesettings.kcfgc
    settings/dolphin_directoryviewpropertysettings.kcfgc
    settings/dolphin_detailsmodesettings.kcfgc
    settings/dolphin_iconsmodesettings.kcfgc
    settings/dolphin_generalsettings.kcfgc
    settings/dolphin_versioncontrolsettings.kcfgc
)

add_library(dolphinprivate SHARED ${dolphinprivate_LIB_SRCS})

target_link_libraries(dolphinprivate
    KDE4::kio
    konq
)

set_target_properties(dolphinprivate PROPERTIES
    VERSION ${GENERIC_LIB_VERSION}
    SOVERSION ${GENERIC_LIB_SOVERSION}
)

generate_export_header(dolphinprivate)

install(
    TARGETS dolphinprivate
    DESTINATION ${KDE4_LIB_INSTALL_DIR}
)

install(
    FILES views/versioncontrol/fileviewversioncontrolplugin.desktop
    DESTINATION ${KDE4_SERVICETYPES_INSTALL_DIR}
)

##########################################

set(dolphin_SRCS
    dolphinapplication.cpp
    dolphindockwidget.cpp
    dolphinmainwindow.cpp
    dolphinviewcontainer.cpp
    dolphincontextmenu.cpp
    dolphinrecenttabsmenu.cpp
    dolphintabpage.cpp
    filterbar/filterbar.cpp
    main.cpp
    panels/information/filemetadataconfigurationdialog.cpp
    panels/information/informationpanel.cpp
    panels/information/informationpanelcontent.cpp
    panels/places/placespanel.cpp
    panels/places/placesitem.cpp
    panels/places/placesitemeditdialog.cpp
    panels/places/placesitemlistgroupheader.cpp
    panels/places/placesitemlistwidget.cpp
    panels/places/placesitemmodel.cpp
    panels/places/placesitemsignalhandler.cpp
    panels/places/placesview.cpp
    panels/panel.cpp
    search/dolphinfacetswidget.cpp
    search/dolphinsearchbox.cpp
    settings/general/behaviorsettingspage.cpp
    settings/general/configurepreviewplugindialog.cpp
    settings/general/confirmationssettingspage.cpp
    settings/general/generalsettingspage.cpp
    settings/general/previewssettingspage.cpp
    settings/general/statusbarsettingspage.cpp
    settings/dolphinsettingsdialog.cpp
    settings/navigation/navigationsettingspage.cpp
    settings/services/servicessettingspage.cpp
    settings/settingspagebase.cpp
    settings/serviceitemdelegate.cpp
    settings/servicemodel.cpp
    settings/startup/startupsettingspage.cpp
    settings/trash/trashsettingspage.cpp
    settings/viewmodes/dolphinfontrequester.cpp
    settings/viewmodes/viewsettingspage.cpp
    settings/viewmodes/viewmodesettings.cpp
    settings/viewmodes/viewsettingstab.cpp
    statusbar/dolphinstatusbar.cpp
    statusbar/mountpointobserver.cpp
    statusbar/mountpointobservercache.cpp
    statusbar/spaceinfoobserver.cpp
    statusbar/statusbarspaceinfo.cpp
    views/zoomlevelinfo.cpp
)

kde4_add_kcfg_files(dolphin_SRCS
    panels/information/dolphin_informationpanelsettings.kcfgc
    panels/places/dolphin_placespanelsettings.kcfgc
    settings/dolphin_compactmodesettings.kcfgc
    settings/dolphin_detailsmodesettings.kcfgc
    settings/dolphin_generalsettings.kcfgc
    settings/dolphin_iconsmodesettings.kcfgc
    search/dolphin_searchsettings.kcfgc
    settings/dolphin_versioncontrolsettings.kcfgc
)

set(dolphin_SRCS ${dolphin_SRCS} panels/terminal/terminalpanel.cpp)

add_executable(dolphin ${dolphin_SRCS})

target_link_libraries(dolphin
    KDE4::kio
    KDE4::kparts
    KDE4::kcmutils
    KDE4::solid
    KDE4::kmediaplayer
    konq
    dolphinprivate
)

install(
    TARGETS dolphin
    DESTINATION ${KDE4_BIN_INSTALL_DIR}
)

##########################################

set(kcm_dolphinviewmodes_PART_SRCS
    settings/kcm/kcmdolphinviewmodes.cpp
    settings/viewmodes/dolphinfontrequester.cpp
    settings/viewmodes/viewmodesettings.cpp
    settings/viewmodes/viewsettingstab.cpp
    views/zoomlevelinfo.cpp
)

set(kcm_dolphinnavigation_PART_SRCS
    settings/kcm/kcmdolphinnavigation.cpp
    settings/navigation/navigationsettingspage.cpp
    settings/settingspagebase.cpp
)

set(kcm_dolphinservices_PART_SRCS
    settings/kcm/kcmdolphinservices.cpp
    settings/services/servicessettingspage.cpp
    settings/settingspagebase.cpp
    settings/serviceitemdelegate.cpp
    settings/servicemodel.cpp
)

set(kcm_dolphingeneral_PART_SRCS
    settings/kcm/kcmdolphingeneral.cpp
    settings/general/behaviorsettingspage.cpp
    settings/general/previewssettingspage.cpp
    settings/general/configurepreviewplugindialog.cpp
    settings/general/confirmationssettingspage.cpp
    settings/settingspagebase.cpp
    settings/serviceitemdelegate.cpp
    settings/servicemodel.cpp
)

kde4_add_kcfg_files(kcm_dolphinviewmodes_PART_SRCS
    settings/dolphin_compactmodesettings.kcfgc
    settings/dolphin_directoryviewpropertysettings.kcfgc
    settings/dolphin_detailsmodesettings.kcfgc
    settings/dolphin_iconsmodesettings.kcfgc
    settings/dolphin_generalsettings.kcfgc
    settings/dolphin_versioncontrolsettings.kcfgc
)

kde4_add_kcfg_files(kcm_dolphinnavigation_PART_SRCS
    settings/dolphin_generalsettings.kcfgc
)

kde4_add_kcfg_files(kcm_dolphinservices_PART_SRCS
    settings/dolphin_generalsettings.kcfgc
    settings/dolphin_versioncontrolsettings.kcfgc
)

kde4_add_kcfg_files(kcm_dolphingeneral_PART_SRCS
    settings/dolphin_generalsettings.kcfgc
)

kde4_add_plugin(kcm_dolphinviewmodes ${kcm_dolphinviewmodes_PART_SRCS})
kde4_add_plugin(kcm_dolphinnavigation ${kcm_dolphinnavigation_PART_SRCS})
kde4_add_plugin(kcm_dolphinservices ${kcm_dolphinservices_PART_SRCS})
kde4_add_plugin(kcm_dolphingeneral ${kcm_dolphingeneral_PART_SRCS})

target_link_libraries(kcm_dolphinviewmodes KDE4::kdeui KDE4::kio dolphinprivate)
target_link_libraries(kcm_dolphinnavigation  KDE4::kdeui KDE4::kio dolphinprivate)
target_link_libraries(kcm_dolphinservices KDE4::kdeui KDE4::kio dolphinprivate)
target_link_libraries(kcm_dolphingeneral KDE4::kdeui KDE4::kio dolphinprivate)

install(
    TARGETS
    kcm_dolphinviewmodes
    kcm_dolphinnavigation
    kcm_dolphinservices
    kcm_dolphingeneral
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)

#########################################

add_executable(kio_filenamesearch search/filenamesearchprotocol.cpp)

target_link_libraries(kio_filenamesearch KDE4::kio)

install(
    TARGETS kio_filenamesearch
    DESTINATION ${KDE4_LIBEXEC_INSTALL_DIR}
)

########### install files ###############

install(
    PROGRAMS dolphin.desktop
    DESTINATION ${KDE4_XDG_APPS_INSTALL_DIR}
)

install(
    FILES dolphinui.rc
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/dolphin
)

install(
    FILES
    search/filenamesearch.protocol
    settings/kcm/kcmdolphinviewmodes.desktop
    settings/kcm/kcmdolphinnavigation.desktop
    settings/kcm/kcmdolphinservices.desktop
    settings/kcm/kcmdolphingeneral.desktop
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
