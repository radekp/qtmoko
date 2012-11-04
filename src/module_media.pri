#This file contains projects that make up the Qtopia Media module.

PROJECTS*=\
        libraries/qtopiaaudio \
        libraries/qtopiamedia \
        libraries/qtopiavideo \
        tools/mediaserver \
        plugins/content/id3 \
        plugins/content/ogg \
        plugins/content/threegpp
SERVER_PROJECTS*=\
        server/media/servercontrol                #(re)starts media server

!equals(QTOPIA_UI,home) {
    PROJECTS+=applications/mediaplayer
} else {
    PROJECTS+=applications/musicplayer
}

qws:PROJECTS*=\
        plugins/videooutput/directpaintervideooutput

SERVER_PROJECTS*=\
        server/ui/volumedlg \                       #shown when volume keys pressed
        server/media/volumemanagement \             #volume control backend
        server/ui/poweralert \                      #low power alert dialog
        server/media/alertservice                  #audible system alerts

enable_phonon:PROJECTS*=plugins/phonon_backend/qtopiamedia

!equals(QTOPIA_UI,home) { 
    SERVER_PROJECTS*=\
            server/media/keyclick \                     #audible key pressed
            server/media/genericvolumeservice           #generic volume service

    qws:SERVER_PROJECTS*=\
            server/media/screenclick                    #audible touch screen clicks

}

enable_pictureflow:PROJECTS*=3rdparty/libraries/pictureflow

enable_qvfb {
    PROJECTS*=\
        plugins/audiohardware/desktop
}

contains(QTOPIAMEDIA_ENGINES,helix) {
    PROJECTS*=\
        3rdparty/libraries/helix \
        plugins/mediaengines/helix
}

contains(QTOPIAMEDIA_ENGINES,gstreamer) {
    PROJECTS*=\
        3rdparty/libraries/gstreamer \
        plugins/mediaengines/gstreamer
}

contains(QTOPIAMEDIA_ENGINES,cruxus) {
    PROJECTS*=plugins/mediaengines/cruxus
    # MIDI support
    PROJECTS*=\
        3rdparty/libraries/libtimidity\
        3rdparty/plugins/codecs/libtimidity
    # OGG Vorbis support
    PROJECTS*=\
        3rdparty/libraries/tremor\
        3rdparty/plugins/codecs/tremor
    # WAV support
    PROJECTS*=\
        plugins/codecs/wavplay
}

enable_qtopiamedia:contains(QTOPIAMEDIA_ENGINES,cruxus):!enable_singleexec {
    # MP3 support for cruxus
    # not in source packages
    PROJECTS*=\
        3rdparty/libraries/libmad\
        3rdparty/plugins/codecs/libmad \
        3rdparty/libraries/amr\
        3rdparty/plugins/codecs/libamr
}

