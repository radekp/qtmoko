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

#ifndef QCONFIG_QPE_H
#define QCONFIG_QPE_H

// Disable dynamic cast
#ifndef QT_NO_DYNAMIC_CAST
#  define QT_NO_DYNAMIC_CAST
#endif

// Data structures
#ifndef QT_NO_STL
#  define QT_NO_STL
#endif

// Dialogs
#ifndef QT_NO_COLORDIALOG
#  define QT_NO_COLORDIALOG
#endif
#ifndef QT_NO_ERRORMESSAGE
#  define QT_NO_ERRORMESSAGE
#endif
#ifndef QT_NO_INPUTDIALOG
#  define QT_NO_INPUTDIALOG
#endif
//#ifndef QT_NO_FILEDIALOG          file dialog is useful e.g. in fbreader
//#  define QT_NO_FILEDIALOG
//#endif
#ifndef QT_NO_PROGRESSDIALOG
#  define QT_NO_PROGRESSDIALOG
#endif
#ifndef QT_NO_TABDIALOG
#  define QT_NO_TABDIALOG
#endif

// Images
#ifndef QT_NO_IMAGEFORMAT_PPM
#  define QT_NO_IMAGEFORMAT_PPM
#endif
#ifndef QT_NO_IMAGE_HEURISTIC_MASK
#  define QT_NO_IMAGE_HEURISTIC_MASK
#endif
#ifndef QT_NO_IMAGE_TEXT
#  define QT_NO_IMAGE_TEXT
#endif

// Internationalization
#ifndef QT_NO_BIG_CODECS
#  define QT_NO_BIG_CODECS
#endif

// ItemViews
#ifndef QT_NO_DIRMODEL
#  define QT_NO_DIRMODEL
#endif

// Kernel
#ifndef QT_NO_CURSOR
#  define QT_NO_CURSOR
#endif
#ifndef QT_NO_DRAGANDDROP
#  define QT_NO_DRAGANDDROP
#endif
#ifndef QT_NO_EFFECTS
#  define QT_NO_EFFECTS
#endif
#ifndef QT_NO_SESSIONMANAGER
#  define QT_NO_SESSIONMANAGER
#endif

// Networking
#ifndef QT_NO_SOCKS5
#  define QT_NO_SOCKS5
#endif

// Painting
#ifndef QT_NO_COLORNAMES
#  define QT_NO_COLORNAMES
#endif

// Fonts
#ifndef QT_NO_QWS_QPF
#  define QT_NO_QWS_QPF
#endif
#ifndef QT_NO_QWS_QPF2
#  define QT_NO_QWS_QPF2
#endif

// Qtopia Core
#ifndef QT_NO_QWS_ALPHA_CURSOR
#  define QT_NO_QWS_ALPHA_CURSOR
#endif
#ifndef QT_NO_QWS_CURSOR
#  define QT_NO_QWS_CURSOR
#endif
#ifndef QT_NO_QWS_DECORATION_WINDOWS
#  define QT_NO_QWS_DECORATION_WINDOWS
#endif
#ifndef QT_NO_QWS_MOUSE
#  define QT_NO_QWS_MOUSE
#endif
#ifndef QT_NO_QWS_MOUSE_AUTO
#  define QT_NO_QWS_MOUSE_AUTO
#endif
#ifndef QT_NO_QWS_MOUSE_MANUAL
#  define QT_NO_QWS_MOUSE_MANUAL
#endif

// Styles
#ifndef QT_NO_STYLE_MOTIF
#  define QT_NO_STYLE_MOTIF
#endif
#ifndef QT_NO_STYLE_CDE
#  define QT_NO_STYLE_CDE
#endif
#ifndef QT_NO_STYLE_PLASTIQUE
#  define QT_NO_STYLE_PLASTIQUE
#endif
#ifndef QT_NO_STYLE_WINDOWSXP
#  define QT_NO_STYLE_WINDOWSXP
#endif

// Widgets
#ifndef QT_NO_DOCKWIDGET
#  define QT_NO_DOCKWIDGET
#endif
#ifndef QT_NO_WORKSPACE
#  define QT_NO_WORKSPACE
#endif
#ifndef QT_NO_CONTEXTMENU
#  define QT_NO_CONTEXTMENU
#endif
#ifndef QT_NO_SPLITTER
#  define QT_NO_SPLITTER
#endif
#ifndef QT_NO_SIZEGRIP
#  define QT_NO_SIZEGRIP
#endif
#ifndef QT_NO_DIAL
#  define QT_NO_DIAL
#endif
#ifndef QT_NO_SYNTAXHIGHLIGHTER
#  define QT_NO_SYNTAXHIGHLIGHTER
#endif
#ifndef QT_NO_STATUSBAR
#  define QT_NO_STATUSBAR
#endif
#ifndef QT_NO_STATUSTIP
#  define QT_NO_STATUSTIP
#endif
#ifndef QT_NO_TOOLBOX
#  define QT_NO_TOOLBOX
#endif
#ifndef QT_NO_TOOLTIP
#  define QT_NO_TOOLTIP
#endif

#endif
