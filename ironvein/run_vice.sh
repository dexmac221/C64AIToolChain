#!/bin/bash
# Launch VICE on the demo, sound off, never fullscreen, monitor on 6510.
cd "$(dirname "$0")"
env -u LD_LIBRARY_PATH -u GTK_PATH -u GTK_EXE_PREFIX -u GIO_MODULE_DIR \
    -u GTK_IM_MODULE_FILE -u GSETTINGS_SCHEMA_DIR -u LOCPATH \
    -u GDK_PIXBUF_MODULE_FILE -u GDK_PIXBUF_MODULEDIR -u XDG_DATA_HOME \
    nohup setsid x64 +warp +autostart-warp -speed 100 +sound +VICIIfull \
    -remotemonitor -remotemonitoraddress ip4://127.0.0.1:6510 \
    -autostart ironvein.prg > /dev/null 2>&1 < /dev/null &
disown
