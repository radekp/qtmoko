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

#ifndef GPSDWHEREABOUTS_P_H
#define GPSDWHEREABOUTS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt Extended API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qwhereabouts.h"
#include "qwhereaboutsupdate.h"

#include <gps.h>
#include <QHostAddress>

class QBasicTimer;
class QTimerEvent;

class QTOPIAWHEREABOUTS_EXPORT QGpsdWhereabouts : public QWhereabouts
{
    Q_OBJECT

public:
    explicit QGpsdWhereabouts(QObject *parent = 0,
                             const QHostAddress &addr = QHostAddress::LocalHost,
                             quint16 port = 2947);
    virtual ~QGpsdWhereabouts();

public slots:
    virtual void startUpdates();
    virtual void stopUpdates();
    virtual void requestUpdate();

protected:
    void timerEvent(QTimerEvent *event);

private:
    QHostAddress m_address;
    quint16 m_port;
    bool active;
    bool emit_frequent; /* shall we emit frequent updates */
    bool emit_oneshot;  /* shall we emit next update only */
    int  elapsed_time;  /* calculate time since last emit */
    struct gps_data_t gpsdata;
    struct gps_data_t *gps_data;

    QBasicTimer *m_queryTimer;

    void parseFix(struct gps_data_t * fix);
    bool activateGps();
    void deactivateGps();

    Q_DISABLE_COPY(QGpsdWhereabouts)
};

#endif
