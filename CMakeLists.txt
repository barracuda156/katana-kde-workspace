project(kde-workspace)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(GenerateExportHeader)
include(FeatureSummary)

find_package(KDELibs4 4.23.0 REQUIRED)

# kdelibs requires 0.91
find_package(SharedMimeInfo 0.91)
set_package_properties(SharedMimeInfo PROPERTIES
    DESCRIPTION "Shared Mime Info"
    URL "http://freedesktop.org/wiki/Software/shared-mime-info"
    PURPOSE "Needed for update MIME database"
    TYPE REQUIRED
)

find_package(Sudo)
set_package_properties(Sudo PROPERTIES
    DESCRIPTION "Sudo allows a system administrator to delegate authority to give certain users"
    URL "https://www.sudo.ws/"
    TYPE RUNTIME
    PURPOSE "Needed for kdesudo to operate"
)

find_package(XCB)
set_package_properties(XCB PROPERTIES
    DESCRIPTION "X protocol C-language Binding"
    URL "http://xcb.freedesktop.org"
    TYPE REQUIRED
)

find_package(X11_XCB)
set_package_properties(X11_XCB
    PROPERTIES DESCRIPTION "XCB X11 protocol client library"
    TYPE REQUIRED
)

find_package(X11)
set_package_properties(X11 PROPERTIES
    DESCRIPTION "X11 libraries"
    URL "http://www.x.org"
    TYPE REQUIRED
)

add_feature_info("XKBfile" X11_Xkbfile_FOUND "The keyboard file manipulation library may be used by keyboard KCM")
add_feature_info("libXSync" X11_XSync_FOUND "The X11 synchronization may be used by KWin")
add_feature_info("libXRes" X11_XRes_FOUND "The X Resource library may be used by ksysguard")
add_feature_info("libXext" X11_dpms_FOUND "The X extensions library may be used by kscreensaver")

if(NOT X11_Xau_FOUND)
    message(FATAL_ERROR "The X11 authorization extension library was not found. Required for authorization in kworkspace library")
endif()
if(NOT X11_Xdamage_FOUND)
    message(FATAL_ERROR "The X11 damaged region extension library was not found. Required for compositing support in KWin")
endif()
if(NOT X11_Xrender_FOUND)
    message(FATAL_ERROR " The X Rendering extension client library was not found. Required for XRender Compositing backend in KWin")
endif()
if(NOT X11_Xfixes_FOUND)
    message(FATAL_ERROR "The X11 miscellaneous 'fixes' extension library was not found. Required for XRender Compositing backend in KWin")
endif()
if(NOT X11_Xrandr_FOUND)
    message(FATAL_ERROR "The X11 RandR extension library was not found. Required for Multi Screen Support")
endif()
if(NOT X11_Xcursor_FOUND)
    message(FATAL_ERROR "The X11 cursor management library was not found. Required for desktop effects support in KWin")
endif()

kde4_optional_find_package(OpenGL)
set_package_properties(OpenGL PROPERTIES
    DESCRIPTION "The OpenGL libraries"
    URL "http://www.opengl.org"
    PURPOSE "OpenGL info module"
    TYPE OPTIONAL
)

kde4_optional_find_package(LibDRM)
set_package_properties(LibDRM PROPERTIES
    DESCRIPTION "Userspace interface to kernel DRM services"
    URL "https://dri.freedesktop.org/wiki/"
    TYPE OPTIONAL
    PURPOSE "OpenGL info module"
)

kde4_optional_find_package(Freetype)
set_package_properties(Freetype PROPERTIES
    DESCRIPTION "Freely available software library to render fonts"
    URL "https://www.freetype.org"
    PURPOSE "Needed to build font configuration and installation tools"
    TYPE OPTIONAL
)

kde4_optional_find_package(Fontconfig)
set_package_properties(Fontconfig PROPERTIES
    DESCRIPTION "Font access configuration library"
    URL "http://www.freedesktop.org/wiki/Software/fontconfig"
    PURPOSE "Needed to build font configuration and installation tools"
    TYPE OPTIONAL
)

kde4_optional_find_package(LibUSB)
set_package_properties(LibUSB PROPERTIES
    DESCRIPTION "User level access to USB devices"
    URL "https://libusb.info/"
    TYPE OPTIONAL
    PURPOSE "Provides Logitech mouse support in KControl and USB devices information"
)

kde4_optional_find_package(Kmod)
set_package_properties(Kmod PROPERTIES
    DESCRIPTION "Set of tools to handle common tasks with Linux kernel modules"
    URL "https://git.kernel.org/pub/scm/utils/kernel/kmod/kmod.git/"
    TYPE OPTIONAL
    PURPOSE "Kernel modules information"
)
kde4_optional_find_package(PCIUTILS)
set_package_properties(PCIUTILS PROPERTIES
    DESCRIPTION "PciUtils is a library for direct access to PCI slots"
    URL "http://atrey.karlin.mff.cuni.cz/~mj/pciutils.shtml"
    TYPE OPTIONAL
    PURPOSE "View PCI details in kinfocenter"
)

kde4_optional_find_package(RAW1394)
set_package_properties(RAW1394 PROPERTIES
    DESCRIPTION "library for direct access to IEEE 1394 bus"
    URL "http://www.linux1394.org/"
    TYPE OPTIONAL
    PURPOSE "View FireWire devices in kinfocenter"
)

kde4_optional_find_package(Mtp 1.1.2)
set_package_properties(Mtp PROPERTIES
    DESCRIPTION "the library implementation of the Media Transfer Protocol"
    URL "http://libmtp.sourceforge.net/"
    TYPE OPTIONAL
    PURPOSE "Needed to build the MTP kioslave"
)

kde4_optional_find_package(Gphoto2 2.5)
set_package_properties(Gphoto2 PROPERTIES
    DESCRIPTION "Free, redistributable, ready to use set of digital camera software applications"
    URL "http://www.gphoto.org/"
    TYPE OPTIONAL
    PURPOSE "Needed to build camera kioslave"
)

kde4_optional_find_package(EPub)
set_package_properties(EPub PROPERTIES
    DESCRIPTION "A library for reading EPub documents"
    URL "https://sourceforge.net/projects/ebook-tools/"
    TYPE RECOMMENDED
    PURPOSE "Provides support for eBook in the thumbnail KIO slave"
)

kde4_optional_find_package(DjVuLibre)
set_package_properties(DjVuLibre PROPERTIES
    DESCRIPTION "Open source DjVu library"
    URL "https://djvu.sourceforge.net/"
    TYPE RECOMMENDED
    PURPOSE "Provides support for DjVu in the thumbnail KIO slave"
)

kde4_optional_find_package(Sensors)
set_package_properties(Sensors PROPERTIES
    DESCRIPTION "Linux hardware monitoring"
    URL "https://hwmon.wiki.kernel.org/lm_sensors"
    PURPOSE "Sensors feedback in ksysguard"
    TYPE OPTIONAL
)

kde4_optional_find_package(Qalculate)
set_package_properties(Qalculate PROPERTIES
    DESCRIPTION "Qalculate Library"
    URL "http://qalculate.sourceforge.net"
    TYPE OPTIONAL
    PURPOSE "Needed to enable advanced features of the calculator runner"
)

kde4_optional_find_package(LightDM)
set_package_properties(LightDM PROPERTIES
    DESCRIPTION "Cross-desktop display manager"
    URL "https://github.com/canonical/lightdm"
    TYPE OPTIONAL
    PURPOSE "Needed for the LightDM greeter"
)

kde4_optional_find_package(LibGit2)
set_package_properties(LibGit2 PROPERTIES
    DESCRIPTION "Portable, pure C implementation of the Git core methods"
    URL "https://libgit2.org/"
    TYPE OPTIONAL
    PURPOSE "Needed for the Dolphin Git plugin"
)

kde4_optional_find_package(ALSA)
set_package_properties(ALSA PROPERTIES
    DESCRIPTION "Advanced Linux Sound Architecture"
    URL "https://alsa-project.org/wiki/Main_Page"
    PURPOSE "Needed for Plasma mixer applet"
)

find_program(WRESTOOL_EXECUTABLE wrestool)
add_feature_info(wrestool
    WRESTOOL_EXECUTABLE
    "ICO thumbnails support in KIO slave"
)

find_program(CTAGS_EXECUTABLE ctags ctags-universal ctags-exuberant exctags uctags)
add_feature_info(ctags
    CTAGS_EXECUTABLE
    "Code indexing support in Kate"
)

find_program(CPPCHECK_EXECUTABLE cppcheck)
add_feature_info(cppcheck
    CPPCHECK_EXECUTABLE
    "Code analysis support in Kate"
)

find_program(RZSZ_EXECUTABLE NAMES rz lrz)
add_feature_info(rzsz
    RZSZ_EXECUTABLE
    "ZModem support in Konsole"
)

find_program(XKILL_EXECUTABLE xkill)
add_feature_info(xkill
    XKILL_EXECUTABLE
    "Window killing support in KWin"
)

find_program(EJECT_EXECUTABLE NAMES eject cdcontrol cdio)
add_feature_info(eject
    EJECT_EXECUTABLE
    "Eject CD and DVD devices"
)

find_program(DIFF_EXECUTABLE diff)
add_feature_info(diff
    DIFF_EXECUTABLE
    "Show difference between files in Kate"
)

find_program(XRANDR_EXECUTABLE xrandr)
add_feature_info(xrandr
    XRANDR_EXECUTABLE
    "Set size, orientation, etc. of screens"
)

include(ConfigureChecks.cmake)
configure_file(config-unix.h.cmake ${CMAKE_BINARY_DIR}/config-unix.h )
configure_file(config-X11.h.cmake ${CMAKE_BINARY_DIR}/config-X11.h )
if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
  set(EXPORT_XCURSOR_PATH "XCURSOR_PATH=${CMAKE_INSTALL_PREFIX}/share/icons:$XCURSOR_PATH\":~/.icons:/usr/share/icons:/usr/share/pixmaps:/usr/X11R6/lib/X11/icons\"; export XCURSOR_PATH")
endif(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
configure_file(startkde.cmake ${CMAKE_BINARY_DIR}/startkde  @ONLY)
configure_file(config-workspace.h.cmake ${CMAKE_BINARY_DIR}/config-workspace.h )

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} ${CMAKE_SOURCE_DIR}/libs)

# libs
add_subdirectory(libs)

add_subdirectory(systemsettings)

# core apps
add_subdirectory(kwin)
add_subdirectory(kscreensaver)

add_subdirectory(qguiplatformplugin_kde)
add_subdirectory(ksysguard)

add_subdirectory(kcontrol)
add_subdirectory(kmenuedit)
add_subdirectory(kinfocenter)

if (LightDM_FOUND)
    add_subdirectory(kgreeter)
endif(LightDM_FOUND)

# data
add_subdirectory(cursors)

add_subdirectory(plasma)
add_subdirectory(kstyles)

# Imported from kde-runtime and other sub-projects
add_subdirectory(menu)
add_subdirectory(knotify)
add_subdirectory(soliduiserver)
add_subdirectory(solid-hardware)
add_subdirectory(kcmshell)
add_subdirectory(kioslave)
add_subdirectory(kurifilter-plugins)
add_subdirectory(kquitapp)
if (Q_WS_X11)
    add_subdirectory(kstart)
endif (Q_WS_X11)
# Imported from kde-baseapps
add_subdirectory(dolphin)
add_subdirectory(kate)
add_subdirectory(kdepasswd)
add_subdirectory(kdesudo)
add_subdirectory(kdialog)
add_subdirectory(keditbookmarks)
add_subdirectory(kfind)
add_subdirectory(konsole)
# Goodies
add_subdirectory(kmediaplayer)
add_subdirectory(kmail)
add_subdirectory(kfreespace)
# Background processes
add_subdirectory(kdontchangethehostname)
# Command-line tools (e.g. for shell scripts)
add_subdirectory(kdeeject)
add_subdirectory(kfile)
add_subdirectory(kiconfinder)
add_subdirectory(kioclient)
add_subdirectory(ktraderclient)
add_subdirectory(kreadconfig)
add_subdirectory(kmimetypefinder)
# UI Helper applications
add_subdirectory(kcrash)
add_subdirectory(knetattach)
add_subdirectory(kdirshare)
add_subdirectory(keditfiletype)
# Default settings, content and config
add_subdirectory(l10n)
add_subdirectory(kde-menu)

########### install files ###############
# install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/kde4 DESTINATION ${KDE4_BIN_INSTALL_DIR})
install(PROGRAMS ${CMAKE_BINARY_DIR}/startkde DESTINATION ${KDE4_BIN_INSTALL_DIR})

set(KDE4WORKSPACE_TARGET_PREFIX KDE4Workspace::)

configure_file(
    KDE4WorkspaceConfig.cmake.in
    "${CMAKE_BINARY_DIR}/KDE4WorkspaceConfig.cmake"
    @ONLY
)
configure_file(
    KDE4WorkspaceConfigVersion.cmake.in
    "${CMAKE_BINARY_DIR}/KDE4WorkspaceConfigVersion.cmake"
    @ONLY
)

install(
    FILES
    ${CMAKE_BINARY_DIR}/KDE4WorkspaceConfig.cmake
    ${CMAKE_BINARY_DIR}/KDE4WorkspaceConfigVersion.cmake
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/cmake/KDE4Workspace
)

install(
    EXPORT kdeworkspaceTargets 
    NAMESPACE ${KDE4WORKSPACE_TARGET_PREFIX}
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/cmake/KDE4Workspace
    FILE KDE4WorkspaceTargets.cmake
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
