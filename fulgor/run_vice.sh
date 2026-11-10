#!/bin/bash
# Launch VICE (x64sc, the cycle-exact one) on FULGOR: windowed, monitor on
# 6510. Sound is off unless you pass "sound".
cd "$(dirname "$0")"
SND=+sound
[ "$1" = "sound" ] && SND=-sound
env -u LD_LIBRARY_PATH -u GTK_PATH -u GTK_EXE_PREFIX -u GIO_MODULE_DIR \
    -u GTK_IM_MODULE_FILE -u GSETTINGS_SCHEMA_DIR -u LOCPATH \
    -u GDK_PIXBUF_MODULE_FILE -u GDK_PIXBUF_MODULEDIR -u XDG_DATA_HOME \
    nohup setsid x64sc +warp -autostart-warp -speed 100 $SND +VICIIfull \
    -remotemonitor -remotemonitoraddress ip4://127.0.0.1:6510 \
    -autostart fulgor.prg > /dev/null 2>&1 < /dev/null &
disown
