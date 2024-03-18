#!/bin/sh
#
#  DEFAULT KDE STARTUP SCRIPT ( @KDE_VERSION_STRING@ )
#

if test "x$1" = x--failsafe; then
    KDE_FAILSAFE=1 # General failsafe flag
    KWIN_COMPOSE=N # Disable KWin's compositing
    export KWIN_COMPOSE KDE_FAILSAFE
fi

# When the X server dies we get a HUP signal from xinit. We must ignore it
# because we still need to do some cleanup.
trap 'echo GOT SIGHUP' HUP

# Check if a KDE session already is running and whether it's possible to connect to X
kcheckrunning
kcheckrunning_result=$?
if test $kcheckrunning_result -eq 0 ; then
    echo "KDE seems to be already running on this display."
    xmessage -geometry 500x100 "KDE seems to be already running on this display." > /dev/null 2>/dev/null
    exit 1
elif test $kcheckrunning_result -eq 2 ; then
    echo "\$DISPLAY is not set or cannot connect to the X server."
    exit 1
fi
unset kcheckrunning_result

# Make sure that the KDE prefix is first in XDG_DATA_DIRS and that it's set at all.
# The spec allows XDG_DATA_DIRS to be not set, but X session startup scripts tend
# to set it to a list of paths *not* including the KDE prefix if it's not /usr or
# /usr/local.
if test -z "$XDG_DATA_DIRS"; then
    XDG_DATA_DIRS="@KDE4_SHARE_INSTALL_PREFIX@:/usr/share:/usr/local/share"
else
    XDG_DATA_DIRS="@KDE4_SHARE_INSTALL_PREFIX@:$XDG_DATA_DIRS"
fi
export XDG_DATA_DIRS

# The user's personal KDE directory is usually ~/.kde, but this setting
# may be overridden by setting KDEHOME.
kdehome=$HOME/@KDE_DEFAULT_HOME@
test -n "$KDEHOME" && kdehome=`echo "$KDEHOME"|sed "s,^~/,$HOME/,"`

kcminputrc_mouse_cursortheme=`kreadconfig --file kcminputrc --group Mouse --key cursorTheme --default Oxygen_White`
kcminputrc_mouse_cursorsize=`kreadconfig --file kcminputrc --group Mouse --key cursorSize`
# XCursor mouse theme needs to be applied here to work even for kded or ksmserver
if test -n "$kcminputrc_mouse_cursortheme" -o -n "$kcminputrc_mouse_cursorsize" ; then
    @EXPORT_XCURSOR_PATH@

    kapplymousetheme "$kcminputrc_mouse_cursortheme" "$kcminputrc_mouse_cursorsize"
    if test -n "$kcminputrc_mouse_cursortheme"; then
        XCURSOR_THEME="$kcminputrc_mouse_cursortheme"
        export XCURSOR_THEME
    fi
    if test -n "$kcminputrc_mouse_cursorsize"; then
        XCURSOR_SIZE="$kcminputrc_mouse_cursorsize"
        export XCURSOR_SIZE
    fi
fi
unset kcminputrc_mouse_cursortheme
unset kcminputrc_mouse_cursorsize

# Set a left cursor instead of the standard X11 "X" cursor, since I've heard
# from some users that they're confused and don't know what to do. This is
# especially necessary on slow machines, where starting KDE takes one or two
# minutes until anything appears on the screen.
#
# If the user has overwritten fonts, the cursor font may be different now
# so don't move this up.
xsetroot -cursor_name left_ptr

echo 'startkde: Starting up...'  1>&2

# in case we have been started with full pathname spec without being in PATH
if test -n "$PATH" ; then
    qdbus=$(basename @QT_QDBUS_EXECUTABLE@)
else
    qdbus=@QT_QDBUS_EXECUTABLE@
fi

# Make sure that D-Bus is running
# D-Bus autolaunch is broken
if test -z "$DBUS_SESSION_BUS_ADDRESS" ; then
    eval `dbus-launch --sh-syntax --exit-with-session`
fi
if $qdbus >/dev/null 2>/dev/null; then
    : # ok
else
    echo 'startkde: Could not start D-Bus. Can you call qdbus?'  1>&2
    xmessage -geometry 500x100 "Could not start D-Bus. Can you call qdbus?"
    exit 1
fi


# Mark that full KDE session is running. The KDE_FULL_SESSION property can be
# detected by any X client connected to the same X session, even if not
# launched directly from the KDE session but e.g. using "ssh -X", kdesudo.
# $KDE_FULL_SESSION however guarantees that the application is launched in the
# same environment like the KDE session and that e.g. KDE utilities/libraries
# are available. KDE_FULL_SESSION property is also only available since KDE
# 3.5.5.
# The matching tests are:
#   For $KDE_FULL_SESSION:
#     if test -n "$KDE_FULL_SESSION"; then ... whatever
#   For KDE_FULL_SESSION property:
#     xprop -root | grep "^KDE_FULL_SESSION" >/dev/null 2>/dev/null
#     if test $? -eq 0; then ... whatever
#
# Since KDE4 there is also KDE_SESSION_VERSION, containing the major version number.
# Note that this didn't exist in KDE3, which can be detected by its absense and
# the presence of KDE_FULL_SESSION.
KDE_FULL_SESSION=true
export KDE_FULL_SESSION
xprop -root -f KDE_FULL_SESSION 8t -set KDE_FULL_SESSION true

KDE_SESSION_VERSION=4
export KDE_SESSION_VERSION
xprop -root -f KDE_SESSION_VERSION 32c -set KDE_SESSION_VERSION 4

XDG_CURRENT_DESKTOP=KDE
export XDG_CURRENT_DESKTOP

# For session services that require X11, check for XDG_CURRENT_DESKTOP, etc.
dbus-update-activation-environment DISPLAY XAUTHORITY XDG_CURRENT_DESKTOP \
    KDE_FULL_SESSION KDE_SESSION_VERSION

# Make sure the sycoca database is up-to-date in case something changed without kded4 running to
# update it when that happens
kbuildsycoca4

# Start kcminit_startup
kcminit_startup
if test $? -ne 0; then
    # Startup error
    echo 'startkde: Could not start kcminit_startup. Check your installation.'  1>&2
    xmessage -geometry 500x100 "Could not start kcminit_startup. Check your installation."
    exit 1
fi

# finally, give the session control to the session manager
# see kdebase/ksmserver for the description of the rest of the startup sequence
# if the KDEWM environment variable has been set, then it will be used as KDE's
# window manager instead of kwin.
# if KDEWM is not set, ksmserver will ensure kwin is started.
test -n "$KDEWM" && KDEWM="--windowmanager $KDEWM"
ksmserver $KDEWM
if test $? -ne 0; then
    # Startup error
    echo 'startkde: Could not start ksmserver. Check your installation.'  1>&2
    xmessage -geometry 500x100 "Could not start ksmserver. Check your installation."
fi

echo 'startkde: Shutting down...'  1>&2

unset KDE_FULL_SESSION
xprop -root -remove KDE_FULL_SESSION
unset KDE_SESSION_VERSION
xprop -root -remove KDE_SESSION_VERSION

dbus-update-activation-environment XDG_CURRENT_DESKTOP="" \
    KDE_FULL_SESSION="" KDE_SESSION_VERSION=""

echo 'startkde: Done.'  1>&2
