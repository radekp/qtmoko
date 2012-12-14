#!/bin/sh
qcop service send RotationManager "setCurrentRotation(int)" 90
export SDL_VIDEODRIVER=QtMoko
export SDL_AUDIODRIVER=dummy
#export SDL_QT_DEBUG=1
export SDL_QT_FS=1
openttd
qcop service send RotationManager "setCurrentRotation(int)" 0
