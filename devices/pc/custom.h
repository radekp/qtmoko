/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
**
** Contact: Qt Extended Information (info@qtextended.org)
**
** This file may be used under the terms of the GNU General Public License
** version 2.0 as published by the Free Software Foundation and appearing
** in the file LICENSE.GPL included in the packaging of this file.
**
** Please review the following information to ensure GNU General Public
** Licensing requirements will be met:
**     http://www.fsf.org/licensing/licenses/info/GPLv2.html.
**
**
****************************************************************************/

#ifndef QT_QWS_FICGTA01
#define QT_QWS_FICGTA01
#endif

#if defined(__GNUC__) && (__GNUC__ > 2)
#define QPE_USE_MALLOC_FOR_NEW
#endif


//#define QT_NO_LOG_STREAM


//#define QPE_NEED_CALIBRATION

#define QTOPIA_PHONE_DEVICE "/dev/ttyHS_Application"
//#define QTOPIA_PHONE_VENDOR "ficgta01"

#ifdef Q_WS_QWS
#define QTOPIA_ENABLE_EXPORTED_BACKGROUNDS
#define QTOPIA_ENABLE_GLOBAL_BACKGROUNDS
#endif
#ifndef NO_VISUALIZATION
#define NO_VISUALIZATION
#endif
#define QGLOBAL_PIXMAP_CACHE_LIMIT 2097152



// Define the devices whose packages are compatible with this device,
// by convention the first device listed is this device.
#define QTOPIA_COMPATIBLE_DEVICES "ficgta01,neo1973"

// Define the name of the Video4Linux device to use for the camera.
#ifndef V4L_VIDEO_DEVICE
#define V4L_VIDEO_DEVICE            "" //no video
#endif

#define NMEA_GPS_DEVICE "/dev/ttyO1"

#define GOOGLE_CALENDAR_CONTEXT
