#!/bin/bash
$EXTRACTRC *.kcfg >> rc.cpp
$XGETTEXT *.h *.cpp killer/*.cpp -o $podir/kwin.pot
