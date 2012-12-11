#This file contains projects that make up the Essentials module.

PROJECTS*=\
    3rdparty/applications/qmplayer \
    3rdparty/applications/qx \
    3rdparty/applications/screenshot \
    3rdparty/applications/qterminal \
    settings/backup \
    settings/light-and-power \
    settings/security \
    applications/calculator \
    applications/clock \
    applications/sysinfo \
    applications/textedit \
    settings/language \
    settings/systemtime \
    settings/worldtime \
    plugins/content/exif \

!equals(QTOPIA_UI,home) {
    PROJECTS*=\
        applications/photoedit 
    SERVER_PROJECTS*=\
        server/ui/components/firstuse \          #firstuse dialogs

    !x11:SERVER_PROJECTS*=\
        server/ui/components/calibrate          #calibrate application
} else {
    PROJECTS+=applications/photogallery
}

!x11:SERVER_PROJECTS*=\
    server/infrastructure/keyboardlock   #qws based keyboard locking

SERVER_PROJECTS*=\
    server/phone/powermanager \                 #power manager for suspend/lightoff/dim
    server/infrastructure/suspendtasks \        #default suspend handlers
    server/infrastructure/apm                   #apm impl for QPowerSource

# Camera
enable_pictureflow:PROJECTS*=3rdparty/libraries/pictureflow
PROJECTS*=\
    libraries/qtopiavideo \
    applications/camera \
    plugins/content/exif                       #exif file support
SERVER_PROJECTS*=\
    server/infrastructure/camera                #camera detection

enable_qvfb:PROJECTS*=plugins/cameras/v4lwebcams # webcam for qvfb builds
