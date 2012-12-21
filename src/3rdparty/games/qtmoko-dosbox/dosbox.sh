#!/bin/sh
qcop service send RotationManager "setCurrentRotation(int)" 90
export SDL_VIDEODRIVER=QtMoko
if [ -f ~/.sdlnosound ]
then
    export SDL_AUDIODRIVER=dummy
else
    export SDL_AUDIODRIVER=pulse
fi
#export SDL_QT_DEBUG=1
export SDL_QT_FS=0
export SDL_QT_SCRIPT=/opt/qtmoko/share/sdl/dosbox.js
dosbox
qcop service send RotationManager "setCurrentRotation(int)" 0
