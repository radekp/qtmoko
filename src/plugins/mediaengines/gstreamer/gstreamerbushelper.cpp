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

#include <QMap>
#include <QTimer>

#include "gstreamerbushelper.h"


namespace gstreamer
{

#ifndef QT_NO_GLIB
class BusHelperPrivate : public QObject
{
    Q_OBJECT

public:
    void addWatch(GstBus* bus, BusHelper* helper)
    {
        this->m_helper = helper;
        setParent(helper);
        m_tag = gst_bus_add_watch_full(bus, 0, busCallback, this, NULL);
    }

    void removeWatch(BusHelper* helper)
    {
        Q_UNUSED(helper);
        g_source_remove(m_tag);
    }

    static BusHelperPrivate* instance()
    {
        return new BusHelperPrivate;
    }

private:
    void processMessage(GstBus* bus, GstMessage* message)
    {
        Q_UNUSED(bus);
        emit m_helper->message(message);
    }

    static gboolean busCallback(GstBus *bus, GstMessage *message, gpointer data)
    {
        reinterpret_cast<BusHelperPrivate*>(data)->processMessage(bus, message);
        return TRUE;
    }

    guint       m_tag;
    BusHelper*  m_helper;
};

#else

class BusHelperPrivate : public QObject
{
    Q_OBJECT
    typedef QMap<BusHelper*, GstBus*>   HelperMap;

public:
    void addWatch(GstBus* bus, BusHelper* helper)
    {
        m_helperMap.insert(helper, bus);

        if (m_helperMap.size() == 1)
            m_intervalTimer->start();
    }

    void removeWatch(BusHelper* helper)
    {
        m_helperMap.remove(helper);

        if (m_helperMap.size() == 0)
            m_intervalTimer->stop();
    }

    static BusHelperPrivate* instance()
    {
        static BusHelperPrivate self;

        return &self;
    }

private slots:
    void interval()
    {
        for (HelperMap::iterator it = m_helperMap.begin(); it != m_helperMap.end(); ++it) {
            GstMessage* message;

            while ((message = gst_bus_poll(it.value(), GST_MESSAGE_ANY, 0)) != 0) {
                emit it.key()->message(message);
                gst_message_unref(message);
            }

            emit it.key()->message(Message());
        }
    }

private:
    BusHelperPrivate()
    {
        m_intervalTimer = new QTimer(this);
        m_intervalTimer->setInterval(250);

        connect(m_intervalTimer, SIGNAL(timeout()), SLOT(interval()));
    }

    HelperMap   m_helperMap;
    QTimer*     m_intervalTimer;
};
#endif


/*!
    \class gstreamer::BusHelper
    \internal
*/

BusHelper::BusHelper(GstBus* bus, QObject* parent):
    QObject(parent),
    d(BusHelperPrivate::instance())
{
    d->addWatch(bus, this);
}

BusHelper::~BusHelper()
{
    d->removeWatch(this);
}

}   // ns gstreamer

#include "gstreamerbushelper.moc"

