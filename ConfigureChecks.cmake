include(CMakePushCheckState)
include(CheckTypeSize)
include(CheckSymbolExists)
include(CheckLibraryExists)

check_function_exists(nice HAVE_NICE)
check_include_files(malloc.h HAVE_MALLOC_H)
kde4_bool_to_01(FONTCONFIG_FOUND HAVE_FONTCONFIG) # kcontrol/fonts
kde4_bool_to_01(FREETYPE_FOUND HAVE_FREETYPE) # kcontrol/fonts
kde4_bool_to_01(X11_Xcursor_FOUND HAVE_XCURSOR) # many uses
kde4_bool_to_01(X11_Xfixes_FOUND HAVE_XFIXES) # kicker, kwin
kde4_bool_to_01(X11_Xkb_FOUND HAVE_XKB) # kglobalaccel
kde4_bool_to_01(X11_XSync_FOUND HAVE_XSYNC) # kwin
kde4_bool_to_01(X11_XRes_FOUND HAVE_XRES) # ksysguard
kde4_bool_to_01(X11_dpms_FOUND HAVE_DPMS) # kscreensaver
