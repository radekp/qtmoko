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

#include <qbluetoothlocaldevice.h>
#include <qbluetoothdbus.h>
#include <qbluetoothremotedevice.h>
#include "qbluetoothnamespace_p.h"
#include <qbluetoothlocaldevicemanager.h>
#include <qbluetoothaddress.h>
#include <qbluetoothdefaultpasskeyagent.h>

#include <QList>
#include <QStringList>
#include <QDateTime>
#include <qglobal.h>
#include <qtopialog.h>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QSet>
#include <QDebug>
#include <QProcess>

#include <stdio.h>
#include <string.h>

#include <QMetaObject>

/*!
    \class QBluetoothReply
    \inpublicgroup QtBluetoothModule

    \brief The QBluetoothReply class is a wrapper object that contains a reply, and an error indicator.

    This object represents a return value of a method call.  It contains
    the return value or the error code and is used by the Bluetooth classes
    to allow returning the result value or an error condition as the
    function's return argument.

    \ingroup qtopiabluetooth
*/

/*!
    \fn QBluetoothReply::QBluetoothReply(const T &value)

    Constructs a QBluetoothReply object from a valid value of Type \c Type.
    The reply will be valid, and the \a value will represent the returned
    value.

    \sa isError()
 */

/*!
    \fn QBluetoothReply::QBluetoothReply()

    Constructs a QBluetoothReply object with an error condition set.

    \sa isError()
 */

/*!
    \fn bool QBluetoothReply::isError() const

    Returns whether a reply is an error condition.  The class returning the
    error condition should be queried for the actual error that has occurred.

    \sa value()
 */

/*!
    \fn QBluetoothReply::value() const
    Returns the reply return value.  If the reply is an error, the return
    value of this function is undefined and may be undistinguishable
    from a valid return value.

    \sa isError()
 */

/*!
    \fn QBluetoothReply::operator const T &() const

    Convenience method, same as value()
*/

enum __q__signals_enum {
    REMOTE_DEVICE_DISCOVERED = 1,
    NAME_CHANGED,
    REMOTE_ALIAS_CHANGED,
    REMOTE_ALIAS_CLEARED,
    REMOTE_DEVICE_CONNECTED,
    REMOTE_DEVICE_DISCONNECTED,
    REMOTE_DEVICE_DISCONNECT_REQUEST,
    DEVICE_CREATED,
    DEVICE_REMOVED,
    REMOTE_NAME_UPDATED,
    REMOTE_NAME_FAILED,
    REMOTE_CLASS_UPDATED,
    REMOTE_DEVICE_DISAPPEARED,
    DISCOVERABLE_TIMEOUT_CHANGED,
    PROPERTY_CHANGED
};

class QBluetoothLocalDevice_Private : public QObject
{
    Q_OBJECT

public:
    QBluetoothLocalDevice_Private(QBluetoothLocalDevice *parent,
                                    const QString &devName);
    QBluetoothLocalDevice_Private(QBluetoothLocalDevice *parent,
                                    const QBluetoothAddress &addr);

    ~QBluetoothLocalDevice_Private();

    template <class T>
            QDBusReply<T> callAdapter(const QString & method,
                                      const QVariant & arg1 = QVariant(),
                                      const QVariant & arg2 = QVariant(),
                                      const QVariant & arg3 = QVariant());

    template <class T>
            bool callAdapter(const QString & method,
                             QList<QVariant> args,
                             QDBusReply<T> & reply = QDBusReply<T>(),
                             bool async = false,
                             QObject * receiver = NULL,
                             const char * returnMethod = NULL,
                             const char * errorMethod = NULL);
    
    QBluetoothLocalDevice::Error handleError(const QDBusError &error);
    void emitError(const QDBusError &error);

    QBluetoothLocalDevice *m_parent;
    QBluetoothDefaultPasskeyAgent *m_agent;
    QBluetoothLocalDevice::Error m_error;
    QString m_errorString;
    QList<QBluetoothRemoteDevice> m_discovered;
    QSet<int> m_sigSet;
    uint m_discovTo;

    void lazyInit();

    inline QString& devname()
    { if (!m_doneInit) lazyInit(); return m_devname; }

    inline QString& adapterPath()
    { if (!m_doneInit) lazyInit(); return m_adapterPath; }

    inline QBluetoothAddress& addr()
    { if (!m_doneInit) lazyInit(); return m_addr; }

    inline QBluetoothDbusIface*& iface()
    { if (!m_doneInit) lazyInit(); return m_iface; }

    inline bool& valid()
    { if (!m_doneInit) lazyInit(); return m_valid; }

    void requestSignal(int signal);
    QVariant getProperty(QString name);
    bool setProperty(QString name, QVariant value);
    bool setPropertyAsync(QString name, QVariant value, const char * returnMethod);

public slots:
    void propertyChanged(const QString &name, const QDBusVariant &value);
    void asyncDiscoverableChange(const QDBusMessage &msg);
    void asyncReply(const QDBusMessage &msg);
    void asyncErrorReply(const QDBusError &error, const QDBusMessage &msg);
    void cancelScanReply(const QDBusMessage  &msg);

    void remoteDeviceConnected(const QString &dev);
    void remoteDeviceDisconnected(const QString &dev);

    void discoveryStarted();
    void remoteDeviceFound(const QString &addr, const QMap< QString,QVariant > & values);
    void remoteDeviceDisappeared(const QString &addr);
    void discoveryCompleted();

    void remoteAliasChanged(const QString &addr, const QString &alias);
    void remoteAliasRemoved(const QString &addr);

    void pairingCreated(const QBluetoothAddress &addr);
    void createBondingError(const QBluetoothAddress &addr, const QDBusError &error);
    
    void deviceCreated(const QDBusObjectPath &device);
    void deviceRemoved(const QDBusObjectPath &device);

    void remoteNameUpdated(const QString &addr, const QString &name);
    void remoteNameFailed(const QString &addr);
    void remoteClassUpdated(const QString &addr, uint cls);
    void disconnectRemoteDeviceRequested(const QString &addr);

private:
    QString m_initString;
    bool m_doneInit;

    QString m_devname;
    QString m_adapterPath;
    QBluetoothAddress m_addr;
    QBluetoothDbusIface *m_iface;
    bool m_valid;
};

class PairingCancelledProxy : public QObject
{
    Q_OBJECT

public:
    PairingCancelledProxy(const QBluetoothAddress &addr,
                          QBluetoothLocalDevice_Private *parent);

public slots:
    void createBondingReply(const QDBusMessage &msg);
    void createBondingError(const QDBusError &error, const QDBusMessage &msg);

public:
    QBluetoothLocalDevice_Private *m_parent;
    QBluetoothAddress m_addr;
};

PairingCancelledProxy::PairingCancelledProxy(const QBluetoothAddress &addr,
                                             QBluetoothLocalDevice_Private *parent)
    : QObject(parent), m_parent(parent), m_addr(addr)
{

}

void PairingCancelledProxy::createBondingReply(const QDBusMessage &)
{
    m_parent->pairingCreated(m_addr);
    deleteLater();
}

void PairingCancelledProxy::createBondingError(const QDBusError &error, const QDBusMessage &)
{
    m_parent->createBondingError(m_addr, error);
    deleteLater();
}

static QString btAddr(const QDBusObjectPath & path)
{
    return path.path().right(17).replace('_', ':');     // e.g. "/org/bluez/923/hci0/dev_30_38_55_02_34_21"
}

template <class T>
        bool QBluetoothLocalDevice_Private::callAdapter(const QString & method,
                                                        QList<QVariant> args,
                                                        QDBusReply<T> & reply,
                                                        bool async,
                                                        QObject * receiver,
                                                        const char * returnMethod,
                                                        const char * errorMethod)
{
    if (!m_doneInit)
        lazyInit();
    
    if(!m_valid)
        return false;

    if(async) {
        if(receiver == NULL)
            receiver = this;
        if(returnMethod == NULL)
            returnMethod = SLOT(asyncReply(QDBusMessage));
        if(errorMethod == NULL)
            errorMethod = SLOT(asyncErrorReply(QDBusError,QDBusMessage));
    }
    
    if(m_iface->btcall(method, reply, args, async, receiver, returnMethod, errorMethod))
        return true;

    handleError(reply.error());
    return false;
}

template <class T>
        QDBusReply<T> QBluetoothLocalDevice_Private::callAdapter(const QString & method,
                                                                 const QVariant & arg1,
                                                                 const QVariant & arg2,
                                                                 const QVariant & arg3)
{
    QList<QVariant> args;
    QDBusReply<T> reply;
    
    if(arg1.isValid()) {
        args << arg1;
        if(arg2.isValid()) {
            args << arg2;
            if(arg3.isValid()) {
                args << arg3;
            }
        }
    }
    
    if(callAdapter(method, args, reply))
        return reply;
    
    return reply;
}


QVariant QBluetoothLocalDevice_Private::getProperty(QString name)
{
    QDBusError error;
    
    QVariant value = m_iface->getProperty(name, &error);
    if (value.isValid())
        return value;

    handleError(error);
    return value;
}

bool QBluetoothLocalDevice_Private::setProperty(QString name, QVariant value)
{
    QDBusError error;
    
    if (m_iface->setProperty(name, value, &error))
        return true;
    
    handleError(error);
    return false;
}

bool QBluetoothLocalDevice_Private::setPropertyAsync(QString name, QVariant value, const char * returnMethod)
{
    if(returnMethod == NULL)
        returnMethod = SLOT(asyncReply(QDBusMessage));
    
    return m_iface->setPropertyAsync(name, value, this, returnMethod, SLOT(asyncErrorReply(QDBusError,QDBusMessage)));
}

void QBluetoothLocalDevice_Private::requestSignal(int signal)
{
    if (!iface() || !iface()->isValid()) {
        qWarning("Trying to connect a signal of an invalid QBluetoothLocalDevice instance!");
        return;
    }

    if (m_sigSet.contains(signal))
        return;

    m_sigSet.insert(signal);

    QDBusConnection dbc =
#ifdef QTOPIA_TEST
        QDBusConnection::sessionBus();
#else
        QDBusConnection::systemBus();
#endif
    QString service = iface()->service();
    QString path = iface()->path();
    QString interface = iface()->interface();

    switch(signal) {
        case REMOTE_DEVICE_DISCOVERED:
            dbc.connect(service, path, interface, "DiscoveryStarted",
                        this, SLOT(discoveryStarted()));
            dbc.connect(service, path, interface, "DiscoveryCompleted",
                        this, SLOT(discoveryCompleted()));
            dbc.connect(service, path, interface, "DeviceFound",
                this, SLOT(remoteDeviceFound(const QString &, const QMap< QString,QVariant > &)));
            break;
        case NAME_CHANGED:
            dbc.connect(service, path, interface, "NameChanged",
                m_parent, SIGNAL(nameChanged(QString)));
            break;
        case REMOTE_ALIAS_CHANGED:
            dbc.connect(service, path, interface, "RemoteAliasChanged",
                this,
                SLOT(remoteAliasChanged(QString,QString)));
            break;
        case REMOTE_ALIAS_CLEARED:
            dbc.connect(service, path, interface, "RemoteAliasCleared",
                this, SLOT(remoteAliasRemoved(QString)));
            break;
        case REMOTE_DEVICE_CONNECTED:
            dbc.connect(service, path, interface, "RemoteDeviceConnected",
                this, SLOT(remoteDeviceConnected(QString)));
            break;
        case REMOTE_DEVICE_DISCONNECTED:
            dbc.connect(service, path, interface, "RemoteDeviceDisconnected",
                this, SLOT(remoteDeviceDisconnected(QString)));
            break;
        case REMOTE_DEVICE_DISCONNECT_REQUEST:
            dbc.connect(service, path, interface, "RemoteDeviceDisconnectRequested",
                this, SLOT(disconnectRemoteDeviceRequested(QString)));
            break;
        case DEVICE_CREATED:
            dbc.connect(service, path, interface, "DeviceCreated",
                this, SLOT(deviceCreated(const QDBusObjectPath &)));
            break;
        case DEVICE_REMOVED:
            dbc.connect(service, path, interface, "DeviceRemoved",
                this, SLOT(deviceRemoved(const QDBusObjectPath &)));
            break;
        case REMOTE_NAME_UPDATED:
            dbc.connect(service, path, interface, "RemoteNameUpdated",
                this, SLOT(remoteNameUpdated(QString,QString)));
            break;
        case REMOTE_NAME_FAILED:
            dbc.connect(service, path, interface, "RemoteNameFailed",
                this, SLOT(remoteNameFailed(QString)));
            break;
        case REMOTE_CLASS_UPDATED:
            dbc.connect(service, path, interface, "RemoteClassUpdated",
                this, SLOT(remoteClassUpdated(QString,uint)));
            break;
        case REMOTE_DEVICE_DISAPPEARED:
            dbc.connect(service, path, interface, "RemoteDeviceDisappeared",
                this, SLOT(remoteDeviceDisappeared(QString)));
            break;
        case DISCOVERABLE_TIMEOUT_CHANGED:
            dbc.connect(service, path, interface, "DiscoverableTimeoutChanged",
                m_parent, SIGNAL(discoverableTimeoutChanged(uint)));
            break;
        case PROPERTY_CHANGED:
            dbc.connect(service, path, interface, "PropertyChanged",
                this, SLOT(propertyChanged(QString,QDBusVariant)));
            break;
        default:
            break;
    }
}

void QBluetoothLocalDevice_Private::lazyInit()
{
    if (m_doneInit) return;
    m_doneInit = true;

    if(m_initString.isEmpty())
        return;
    
    QDBusConnection dbc =
#ifdef QTOPIA_TEST
        QDBusConnection::sessionBus();
#else
        QDBusConnection::systemBus();
#endif
    QBluetoothDbusIface iface("org.bluez", "/",
                         "org.bluez.Manager", dbc);
    if (!iface.isValid()) {
        return;
    }

    // m_initString can be either MAC address, hciXX or /full/path/to/hciXX
    // Bluez understands the first two, for the last we strip the full/path.
    m_initString = QBluetoothLocalDevice::adapterPathToDevName(m_initString);

    QDBusReply<QDBusObjectPath> reply;
    QList<QVariant> args;

    args << m_initString;
    
    if (!iface.btcall("FindAdapter", reply, args)) {
        return;
    }
    m_initString.clear();

    if (!reply.isValid()) {
        handleError(reply.error());
        return;
    }

    m_adapterPath = reply.value().path();
    m_devname = QBluetoothLocalDevice::adapterPathToDevName(m_adapterPath);

    m_iface = new QBluetoothDbusIface("org.bluez", m_adapterPath, "org.bluez.Adapter",
                                      dbc);

    if (!m_iface->isValid()) {
        qWarning() << "Could not find org.bluez Adapter interface for" << m_adapterPath;
        delete m_iface;
        m_iface = 0;
        return;
    }

    QString addrStr = getProperty("Address").toString();
    m_addr = QBluetoothAddress(addrStr);
    m_valid = true;
}

QBluetoothLocalDevice_Private::QBluetoothLocalDevice_Private(
        QBluetoothLocalDevice *parent,
        const QBluetoothAddress &addr) : QObject(parent),
        m_parent(parent),
        m_agent(NULL),
        m_error(QBluetoothLocalDevice::NoError),
        m_initString(addr.toString()), m_doneInit(false),
        m_iface(0), m_valid(false)
{
}

QBluetoothLocalDevice_Private::QBluetoothLocalDevice_Private(
        QBluetoothLocalDevice *parent,
        const QString &devName) : QObject(parent),
        m_parent(parent),
        m_agent(NULL),
        m_error(QBluetoothLocalDevice::NoError),
        m_initString(devName), m_doneInit(false),
        m_iface(0), m_valid(false)
{
}

QBluetoothLocalDevice_Private::~QBluetoothLocalDevice_Private()
{
    if (m_agent)
        delete m_agent;

    delete iface(); 
}

struct bluez_error_mapping
{
    const char *name;
    QBluetoothLocalDevice::Error error;
};

static bluez_error_mapping bluez_errors[] = {
    { "org.bluez.Error.Failed", QBluetoothLocalDevice::UnknownError },
    { "org.bluez.Error.InvalidArguments", QBluetoothLocalDevice::InvalidArguments },
    { "org.bluez.Error.NotAuthorized", QBluetoothLocalDevice::NotAuthorized },
    { "org.bluez.Error.OutOfMemory", QBluetoothLocalDevice::OutOfMemory },
    { "org.bluez.Error.NoSuchAdapter", QBluetoothLocalDevice::NoSuchAdaptor },
    { "org.bluez.Error.UnknownAddress", QBluetoothLocalDevice::UnknownAddress },
    { "org.bluez.Error.NotAvailable", QBluetoothLocalDevice::UnknownError },
    { "org.bluez.Error.NotConnected", QBluetoothLocalDevice::NotConnected },
    { "org.bluez.Error.ConnectionAttemptFailed", QBluetoothLocalDevice::ConnectionAttemptFailed },
    { "org.bluez.Error.AlreadyExists", QBluetoothLocalDevice::AlreadyExists },
    { "org.bluez.Error.DoesNotExist", QBluetoothLocalDevice::DoesNotExist },
    { "org.bluez.Error.InProgress", QBluetoothLocalDevice::InProgress },
    { "org.bluez.Error.AuthenticationFailed", QBluetoothLocalDevice::AuthenticationFailed },
    { "org.bluez.Error.AuthenticationTimeout", QBluetoothLocalDevice::AuthenticationTimeout },
    { "org.bluez.Error.AuthenticationRejected", QBluetoothLocalDevice::AuthenticationRejected },
    { "org.bluez.Error.AuthenticationCanceled", QBluetoothLocalDevice::AuthenticationCancelled },
    { "org.bluez.Error.UnsupportedMajorClass", QBluetoothLocalDevice::UnknownError },
    { NULL, QBluetoothLocalDevice::NoError }
};

QBluetoothLocalDevice::Error QBluetoothLocalDevice_Private::handleError(const QDBusError &error)
{
    m_error = QBluetoothLocalDevice::UnknownError;

    int i = 0;
    while (bluez_errors[i].name) {
        if (error.name() == bluez_errors[i].name) {
            m_error = bluez_errors[i].error;
            break;
        }
        i++;
    }

    m_errorString = error.message();

    return m_error;
}

void QBluetoothLocalDevice_Private::emitError(const QDBusError &error)
{
    QBluetoothLocalDevice::Error err = handleError(error);

    emit m_parent->error(err, error.message());
}

void QBluetoothLocalDevice_Private::propertyChanged(const QString &name, const QDBusVariant &value)
{
    QVariant val = value.variant();
    
    qLog(Bluetooth) << "propertyChanged " << name << "=" << val.toString();
    
    if (name == "Powered") {
        if (val.toBool())
            emit m_parent->stateChanged(QBluetoothLocalDevice::Connectable);
        else
            emit m_parent->stateChanged(QBluetoothLocalDevice::Off);
    }
    else if (name == "Discoverable") {
        if (val.toBool())
            emit m_parent->stateChanged(QBluetoothLocalDevice::Discoverable);
        else
            emit m_parent->stateChanged(QBluetoothLocalDevice::Connectable);
    }
}

void QBluetoothLocalDevice_Private::asyncReply(const QDBusMessage &)
{
    // On a success, the signal should have been emitted already
}

void QBluetoothLocalDevice_Private::asyncDiscoverableChange(const QDBusMessage &)
{
    setProperty("DiscoverableTimeout", m_discovTo);
}

void QBluetoothLocalDevice_Private::asyncErrorReply(const QDBusError &error, const QDBusMessage &)
{
    qWarning() << "asyncErrorReply " << error;
    emitError(error);
}

void QBluetoothLocalDevice_Private::cancelScanReply(const QDBusMessage &msg)
{
    if (msg.type() != QDBusMessage::ErrorMessage) {
        emit m_parent->discoveryCancelled();
        return;
    }

    emitError(QDBusError(msg));
}

void QBluetoothLocalDevice_Private::remoteDeviceConnected(const QString &dev)
{
    QBluetoothAddress addr(dev);
    if (addr.isValid()) {
        emit m_parent->remoteDeviceConnected(addr);
    }
}

void QBluetoothLocalDevice_Private::remoteDeviceDisconnected(const QString &dev)
{
    QBluetoothAddress addr(dev);
    if (addr.isValid()) {
        emit m_parent->remoteDeviceDisconnected(addr);
    }
}

void QBluetoothLocalDevice_Private::discoveryStarted()
{
    m_discovered.clear();
    emit m_parent->discoveryStarted();
}

void QBluetoothLocalDevice_Private::remoteAliasChanged(const QString &addr,
        const QString &alias)
{
    emit m_parent->remoteAliasChanged(QBluetoothAddress(addr), alias);
}

void QBluetoothLocalDevice_Private::remoteAliasRemoved(const QString &addr)
{
    emit m_parent->remoteAliasRemoved(QBluetoothAddress(addr));
}

void QBluetoothLocalDevice_Private::remoteDeviceDisappeared(const QString &addr)
{
    emit m_parent->remoteDeviceDisappeared(QBluetoothAddress(addr));
}

void QBluetoothLocalDevice_Private::disconnectRemoteDeviceRequested(const QString &addr)
{
    emit m_parent->remoteDeviceDisconnectRequested(QBluetoothAddress(addr));
}

void QBluetoothLocalDevice_Private::remoteClassUpdated(const QString &addr, uint cls)
{
    quint8 major = (cls >> 8) & 0x1F;
    quint8 minor = (cls >> 2) & 0x3F;
    quint8 service = (cls >> 16) & 0xFF;

    emit m_parent->remoteClassUpdated(QBluetoothAddress(addr), major_to_device_major(major),
                                      minor, QBluetooth::ServiceClasses(service));
}

void QBluetoothLocalDevice_Private::remoteDeviceFound(const QString &addr, const QMap< QString,QVariant > & values)
{
    // Check the truly bizarre case
    //if (invalidIface()) {
      //  return;
    //}

    uint cls = values.value("Class").toUInt();
    int rssi = values.value("RSSI").toInt();
    
    quint8 major = (cls >> 8) & 0x1F;
    quint8 minor = (cls >> 2) & 0x3F;
    quint8 service = (cls >> 16) & 0xFF;

    // Check if we already have this device in the list, sometimes we get spurious
    // signals
    QBluetoothAddress a(addr);

    for (int i = 0; i < m_discovered.size(); i++) {
        if (m_discovered[i].address() == a) {
            qLog(Bluetooth) << "RemoteDeviceFound: " << addr << cls << rssi << "[filtered]";
            return;
        }
    }

    qLog(Bluetooth) << "RemoteDeviceFound: " << addr << cls << rssi;

    QString version;
    QString revision;
    QString manufacturer;
    QString company;
    QString name = values.value("Name").toString();

    /*
     * TODO: fill properties for bluez4
     * 
    QDBusReply<QString> reply;

    reply = iface()->call("GetRemoteVersion", addr);
    if (reply.isValid()) {
        version = reply.value();
    }

    reply = iface()->call("GetRemoteRevision", addr);
    if (reply.isValid()) {
        revision = reply.value();
    }

    reply = iface()->call("GetRemoteManufacturer", addr);
    if (reply.isValid()) {
        manufacturer = reply.value();
    }

    reply = iface()->call("GetRemoteCompany", addr);
    if (reply.isValid()) {
        company = reply.value();
    }

    reply = iface()->call("GetRemoteName", addr);
    if (reply.isValid()) {
        name = reply.value();
    } */

    QBluetoothRemoteDevice dev(a, name, version, revision,
                               manufacturer, company, rssi,
                               major_to_device_major(major),
                               minor, QBluetooth::ServiceClasses(service));
    m_discovered.push_back(dev);

    emit m_parent->remoteDeviceFound(dev);
}

void QBluetoothLocalDevice_Private::discoveryCompleted()
{
    emit m_parent->discoveryCompleted();
    emit m_parent->remoteDevicesFound(m_discovered);

    m_discovered.clear();
}

void QBluetoothLocalDevice_Private::createBondingError(const QBluetoothAddress &addr,
        const QDBusError &error)
{
    handleError(error);
    emit m_parent->pairingFailed(addr);
}

void QBluetoothLocalDevice_Private::pairingCreated(const QBluetoothAddress &addr)
{
    emit m_parent->pairingCreated(addr);
}

void QBluetoothLocalDevice_Private::deviceCreated(const QDBusObjectPath &device)
{
    Q_UNUSED(device);
}

void QBluetoothLocalDevice_Private::deviceRemoved(const QDBusObjectPath &device)
{
    emit m_parent->pairingRemoved(QBluetoothAddress(btAddr(device)));
}

void QBluetoothLocalDevice_Private::remoteNameUpdated(const QString &addr, const QString &name)
{
    emit m_parent->remoteNameUpdated(QBluetoothAddress(addr), name);
}

void QBluetoothLocalDevice_Private::remoteNameFailed(const QString &addr)
{
    emit m_parent->remoteNameFailed(QBluetoothAddress(addr));
}

/*!
    \class QBluetoothLocalDevice
    \inpublicgroup QtBluetoothModule

    \brief The QBluetoothLocalDevice class represents a local bluetooth device.

    The class can be used to query for device attributes, such as device address,
    device class, device manufacturer, etc.  Changing of certain attributes is
    also allowed.  This class can also be used to query remote devices which are
    within range and to initiate bluetooth pairing procedures with remote devices.

    This class uses a QBluetoothReply template class in order to return
    method call results.  The QBluetoothReply class holds an error
    condition flag and the actual return value.  If the error condition
    flag is set to true, then the return value is set to the default
    constructed value.  Thus it is recommended to always check the
    return value by using the QBluetoothReply::isError method like so:

    \code
        QBluetoothAddress addr(remoteDevice);
        QBluetoothLocalDevice local;

        QBluetoothReply<QDateTime> result = local.lastSeen(remoteDevice);
        if (result.isError()) {
            // handle error
            return;
        }

        QDateTime realTime = result.value();
    \endcode

    Alternatively, if you know the return value will always succeed, you
    can do:

    \code
        QDateTime result = local.lastSeen(remoteDevice);
    \endcode

    \ingroup qtopiabluetooth
    \sa QBluetoothAddress, QBluetoothRemoteDevice, QBluetoothReply
    \sa QBluetoothLocalDeviceManager
*/

/*!
    \enum QBluetoothLocalDevice::State
    \brief State of the local adaptor

    \value Off The device is turned off.
    \value Connectable The device can be connected to, but cannot be discovered.
    \value Discoverable The device can be connected to and can be discovered by other remote devicess.

    The device has two scanning types, page scanning and inquiry scanning.
    \list
    \o Page scan - Controls whether other devices can connect to the local device.
    \o Inquiry scan - Controls whether the device can be discovered by remote devices.
    \endlist
    While each scan type can be activated or disabled independently,
    only three combinations really make sense:

    \list
    \o Page Scan Off, Inquiry Scan Off - Device is in \bold Off state
    \o Page Scan On, Inquiry Scan On - Device is in \bold Discoverable state
    \o Page Scan On, Inquiry Scan Off - Device is \bold Connectable state
    \endlist

    The \bold Connectable state is entered by using the bringUp() method.
    The \bold Discoverable state is entered by using the setDiscoverable()
    method.
    The \bold Off state is entered by using the bringDown() method.
*/

/*!
    \enum QBluetoothLocalDevice::Error
    \brief Possible errors that might occur.

    \value NoError No error.
    \value InvalidArguments Invalid arguments have been provided for the operation.
    \value NotAuthorized The client has no permission to perform the action.
    \value OutOfMemory Out of memory condition occurred.
    \value NoSuchAdaptor Trying to use a device which does not exist.
    \value UnknownAddress No such host has been found.
    \value ConnectionAttemptFailed Connection attempt has failed.
    \value NotConnected No connection exists.
    \value AlreadyExists A record or procedure already exists.
    \value DoesNotExist A record or procedure does not exist.
    \value InProgress A long running operation is in progress.
    \value AuthenticationFailed Authentication has failed.
    \value AuthenticationTimeout Authentication has timed out.
    \value AuthenticationRejected Authentication has been rejected.
    \value AuthenticationCancelled Authentication has been canceled.
    \value UnknownError Unknown error has occurred.
*/

/*!
    Constructs a QBluetoothLocalDevice with the default adaptor,
    obtained from QBluetoothLocalDeviceManager::defaultAdaptor().
    This is equivalent to:
    \code
        QBluetoothLocalDeviceManager mgr;
        QString device = mgr.defaultDevice();
        QBluetoothLocalDevice localDevice(device);
    \endcode
    The \a parent parameter specifies the QObject parent.
*/
QBluetoothLocalDevice::QBluetoothLocalDevice(QObject *parent)
    : QObject(parent)
{
    QBluetoothLocalDeviceManager mgr;
    QString device = mgr.defaultDevice();
    m_data = new QBluetoothLocalDevice_Private(this, device);
}

/*!
    Constructs a QBluetoothLocalDevice with \a address paremeter
    representing the bluetooth address of the local device.
    The \a parent parameter specifies the QObject parent.

    \sa deviceName()
*/
QBluetoothLocalDevice::QBluetoothLocalDevice(const QBluetoothAddress &address, QObject* parent ) :
        QObject( parent )
{
    m_data = new QBluetoothLocalDevice_Private(this, address);
}

/*!
    Constructs a QBluetoothLocalDevice with \a devName, which represents a
    system internal device name for the device, typically hci<0-7>.
    The \a parent parameter specifies the QObject parent.

    \sa deviceName()
*/
QBluetoothLocalDevice::QBluetoothLocalDevice(const QString &devName, QObject* parent ) :
        QObject( parent )
{
    m_data = new QBluetoothLocalDevice_Private(this, devName);
}

/*!
    Deconstructs a QBluetoothLocalDevice object.
*/
QBluetoothLocalDevice::~QBluetoothLocalDevice()
{
    delete m_data;
}

/*!
    \internal
*/
void QBluetoothLocalDevice::connectNotify(const char *signal)
{
    QByteArray sig(signal);

    if ((sig == QMetaObject::normalizedSignature(SIGNAL(remoteDeviceFound(QBluetoothRemoteDevice)))) ||
        (sig == QMetaObject::normalizedSignature(SIGNAL(remoteDevicesFound(QList<QBluetoothRemoteDevice>)))) ||
        (sig == QMetaObject::normalizedSignature(SIGNAL(discoveryStarted()))) ||
        (sig == QMetaObject::normalizedSignature(SIGNAL(discoveryCompleted())))
       ) {
        m_data->requestSignal(REMOTE_DEVICE_DISCOVERED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(nameChanged(QString)))) {
        m_data->requestSignal(NAME_CHANGED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(remoteAliasChanged(QBluetoothAddress,QString)))) {
        m_data->requestSignal(REMOTE_ALIAS_CHANGED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(remoteAliasRemoved(QBluetoothAddress)))) {
        m_data->requestSignal(REMOTE_ALIAS_CLEARED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(remoteDeviceConnected(QBluetoothAddress)))) {
        m_data->requestSignal(REMOTE_DEVICE_CONNECTED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(remoteDeviceDisconnected(QBluetoothAddress)))) {
        m_data->requestSignal(REMOTE_DEVICE_DISCONNECTED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(remoteDeviceDisconnectRequested(QBluetoothAddress)))) {
        m_data->requestSignal(REMOTE_DEVICE_DISCONNECT_REQUEST);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(pairingCreated(QBluetoothAddress)))) {
        m_data->requestSignal(DEVICE_CREATED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(pairingRemoved(QBluetoothAddress)))) {
        m_data->requestSignal(DEVICE_REMOVED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(remoteNameUpdated(QBluetoothAddress,QString)))) {
        m_data->requestSignal(REMOTE_NAME_UPDATED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(remoteNameFailed(QBluetoothAddress)))) {
        m_data->requestSignal(REMOTE_NAME_FAILED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(remoteClassUpdated(QBluetoothAddress,QBluetooth::DeviceMajor,quint8,QBluetooth::ServiceClasses)))) {
        m_data->requestSignal(REMOTE_CLASS_UPDATED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(remoteDeviceDisappeared(QBluetoothAddress)))) {
        m_data->requestSignal(REMOTE_DEVICE_DISAPPEARED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(discoverableTimeoutChanged(uint)))) {
        m_data->requestSignal(DISCOVERABLE_TIMEOUT_CHANGED);
    }

    else if (sig == QMetaObject::normalizedSignature(SIGNAL(stateChanged(QBluetoothLocalDevice::State)))) {
        m_data->requestSignal(PROPERTY_CHANGED);
    }
}

/*!
    Returns whether the instance is valid.
*/
bool QBluetoothLocalDevice::isValid() const
{
    return m_data->valid();
}

/*!
    Returns the last error that has occurred.

    \sa errorString()
*/
QBluetoothLocalDevice::Error QBluetoothLocalDevice::error() const
{
    return m_data->m_error;
}

/*!
    Returns the human readable form of the last error that has occurred.

    \sa error()
*/
QString QBluetoothLocalDevice::errorString() const
{
    return m_data->m_errorString;
}

/*!
    Returns the Bluetooth address of this device.

    \sa deviceName()
*/
QBluetoothAddress QBluetoothLocalDevice::address() const
{
    return m_data->addr();
}

/*!
    Returns the system device name of this device.  This is of the form \bold hciX.

    \sa address()
*/
QString QBluetoothLocalDevice::deviceName() const
{
    return m_data->devname();
}

/*!
    Returns the bluez adapter name as used for bluez4 dbus calls. For example \bold /org/bluez/906/hci0.

    \sa address()
*/
QString QBluetoothLocalDevice::adapterPath() const
{
    return m_data->adapterPath();
}

/*!
    Returns the manufacturer of the device.

    \sa version(), revision(), company()
 */
QBluetoothReply<QString> QBluetoothLocalDevice::manufacturer() const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QString>();
    }

    QDBusReply<QString> reply = m_data->iface()->call("GetManufacturer");
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QString>();
    }

    return reply.value();
}

/*!
    Returns the Bluetooth protocol version this device supports.
    This can for instance be 1.0, 1.1, 1.2, 2.0...

    \sa manufacturer(), revision(), company()
 */
QBluetoothReply<QString> QBluetoothLocalDevice::version() const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QString>();
    }

    QDBusReply<QString> reply = m_data->iface()->call("GetVersion");
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QString>();
    }

    return reply.value();
}

/*!
    Returns the device revision.  This is generally manufacturer-specific.

    \sa manufacturer(), version(), company()
 */
QBluetoothReply<QString> QBluetoothLocalDevice::revision() const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QString>();
    }

    QDBusReply<QString> reply = m_data->iface()->call("GetRevision");
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QString>();
    }

    return reply.value();
}

/*!
    Returns the company name of the device (e.g. the brand of the device).

    Note that the underlying HCI daemon requires the IEEE standard oui.txt
    file in order to read the company information correctly. This file
    can be downloaded from the IEEE site. The HCI daemon expects the file
    to be placed at \c /usr/share/misc/oui.txt.

    \sa manufacturer(), version(), revision()
 */
QBluetoothReply<QString> QBluetoothLocalDevice::company() const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QString>();
    }

    QDBusReply<QString> reply = m_data->iface()->call("GetCompany");
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QString>();
    }

    return reply.value();
}

/*!
    Returns the human readable name of the device.

    \sa setName(), nameChanged()
*/
QBluetoothReply<QString> QBluetoothLocalDevice::name() const
{
    return m_data->getProperty("Name").toString();
}

/*!
    Sets the human readable name of this device to \a name.
    Returns true if the call succeeded, and false otherwise.
    In addition, nameChanged signal will be sent once the name
    has been changed.

    \sa name(), nameChanged()
*/
bool QBluetoothLocalDevice::setName(const QString &name)
{
    return m_data->setProperty("Name", name);
}

/*!
    Sets the device into \bold Discoverable state.  The \a timeout value
    is used to specify how long the device will remain discoverable.
    If the timeout value of 0 is specified, the device will remain
    discoverable indefinitely.

    Returns true if the request could be queued, and false otherwise.
    The stateChanged() signal will be sent once the device has changed
    state.  An error() signal will be sent if the state change failed.

    \sa discoverableTimeout(), discoverable(), stateChanged()
*/
bool QBluetoothLocalDevice::setDiscoverable(uint timeout)
{
 #ifdef QT_QWS_GTA04
    QProcess::execute("rfkill", QStringList() << "unblock" << "bluetooth");
#endif
    m_data->m_discovTo = timeout;
    return m_data->setPropertyAsync("Discoverable", true, SLOT(asyncDiscoverableChange(QDBusMessage)));
}

/*!
    Returns the discoverable timeout set for the device.  A value of 0
    signifies that the timeout is indefinite.

    \sa discoverable(), setDiscoverable()
*/
QBluetoothReply<uint> QBluetoothLocalDevice::discoverableTimeout() const
{
    return m_data->getProperty("DiscoverableTimeout").toUInt();
}

/*!
    Returns true if other devices can discover the local device,
    i.e. inquiry scan is enabled.

    Note that if the device is in \bold Discoverable state, it is also in
    \bold Connectable state.

    \sa connectable()
 */
QBluetoothReply<bool> QBluetoothLocalDevice::discoverable() const
{
    return m_data->getProperty("Discoverable").toBool();
}

/*!
    Sets the device into \bold Connectable state.  Other devices
    can connect to the local device, but not discover its existence.

    Returns true if the request could be queued, and false otherwise.
    The stateChanged() signal will be sent once the device has changed
    state.  An error() signal will be sent if the state change failed.

    \sa connectable(), stateChanged()
 */
bool QBluetoothLocalDevice::setConnectable()
{
    return
#ifdef QT_QWS_GTA04
    (QProcess::execute("rfkill", QStringList() << "unblock" << "bluetooth") == 0) &&
#endif
    m_data->setPropertyAsync("Powered", true, SLOT(asyncReply(QDBusMessage)));
}

/*!
    Returns true if other devices can connect to the local device, i.e. page scan is enabled.

    \sa discoverable()
 */
QBluetoothReply<bool> QBluetoothLocalDevice::connectable() const
{
    return m_data->getProperty("Powered").toBool();
}

/*!
    Turns off the device.

    Returns true if the request could be queued, and false otherwise.
    The stateChanged() signal will be sent once the device has changed
    state.  An error() signal will be sent if the state change failed.

    Use setConnectable() or setDiscoverable() to turn the device back on.

    \sa connectable(), State
 */
bool QBluetoothLocalDevice::turnOff()
{
    return
#ifdef QT_QWS_GTA04
    (QProcess::execute("rfkill", QStringList() << "block" << "bluetooth") == 0) &&
#endif
    m_data->setPropertyAsync("Powered", false, SLOT(asyncReply(QDBusMessage)));
}

/*!
    Returns true if the local device is currently enabled.  This is the
    same as calling connectable().

    \sa connectable(), setConnectable(), turnOff()
 */
QBluetoothReply<bool> QBluetoothLocalDevice::isUp()
{
    return connectable();
}

/*!
    Returns a list of all remote devices which are currently connected
    to the local device.

    \sa isConnected()
*/
QBluetoothReply<QList<QBluetoothAddress> > QBluetoothLocalDevice::connections() const
{
    QList<QBluetoothAddress> ret;

    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QList<QBluetoothAddress> >();
    }

    QDBusReply<QStringList> reply = m_data->iface()->call("ListConnections");
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QList<QBluetoothAddress> >();
    }

    foreach(QString addr, reply.value()) {
        ret.push_back(QBluetoothAddress(addr));
    }

    return ret;
}

/*!
    Returns true if a remote device is connected to the local device.
    The address of the remote device is given by \a addr.

    \sa connections()
*/
QBluetoothReply<bool> QBluetoothLocalDevice::isConnected(const QBluetoothAddress &addr) const
{
    return false;
    /*
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<bool>();
    }

    QDBusReply<bool> reply = m_data->iface()->call("IsConnected", addr.toString());
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<bool>();
    }

    return reply.value();*/
}

/*!
    Requests the local device to scan for all discoverable devices in the vicinity.
    Returns true if the device is not already discovering and the process was started
    successfully.  Returns false on error, setting error accordingly.

    Once discovery process is in process, the discoveryStarted() signal is emitted.
    When the discovery process completes, the discoveryCompleted() signal is emitted.

    The clients can subscribe to the discovery information in one of two ways.
    If the client wants to receive information about a device as it is received,
    they should subscribe to the remoteDeviceFound() signal.  Note that the clients
    should be prepared to receive multiple signals with information about
    the same device, and deal with them accordingly. If the clients wish to
    receive the information wholesale, they should subscribe
    to the remoteDevicesFound() signal.

    \sa remoteDevicesFound(), remoteDeviceFound(), cancelDiscovery()
*/
bool QBluetoothLocalDevice::discoverRemoteDevices()
{
    return m_data->callAdapter<void>("StartDiscovery").isValid();
}

/*!
    Attempts to cancel the discovery of remote devices started
    by discoverRemoteDevices(). In case of error or if no
    discovery process is in progress, an error signal will
    be emitted.  In the case of success, a discoveryCancelled()
    signal will be emitted.  Note that only the QBluetoothLocalDevice instance
    that initiated the discovery can cancel it, and only that instance will receive
    the discoveryCancelled() signal.  All other instances will receive the
    discoveryCompleted signal.

    Returns true if the request could be queued, false otherwise.

    \sa discoverRemoteDevices(), discoveryCancelled()
*/
bool QBluetoothLocalDevice::cancelDiscovery()
{
    QList<QVariant> args;
    QDBusReply<void> reply;
    
    return m_data->callAdapter("StopDiscovery", args, reply, true, m_data, SLOT(cancelScanReply(QDBusMessage)));
}

/*!
    Returns the date the remote device with address \a addr was last seen by
    the local device adaptor.  In the case the device has never been seen,
    returns an invalid QDateTime.  If an error occurs, this method returns
    an invalid QDateTime and sets the error() accordingly.

    \sa lastUsed()
*/
QBluetoothReply<QDateTime> QBluetoothLocalDevice::lastSeen(const QBluetoothAddress &addr) const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QDateTime>();
    }

    QDBusReply<QString> reply = m_data->iface()->call("LastSeen", addr.toString());
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QDateTime>();
    }

    QDateTime ret = QDateTime::fromString(reply.value(), Qt::ISODate);
    ret.setTimeSpec(Qt::UTC);
    return ret.toLocalTime();
}

/*!
    Returns the date the remote device with address \a addr was last used by
    the local device adaptor.  In the case the device has never been used,
    returns an invalid QDateTime. If an error occurs, this method returns
    an invalid QDateTime and sets the error() accordingly.

    \sa lastSeen()
 */
QBluetoothReply<QDateTime> QBluetoothLocalDevice::lastUsed(const QBluetoothAddress &addr) const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QDateTime>();
    }

    QDBusReply<QString> reply = m_data->iface()->call("LastUsed", addr.toString());
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QDateTime>();
    }

    QDateTime ret = QDateTime::fromString(reply.value(), Qt::ISODate);
    ret.setTimeSpec(Qt::UTC);
    return ret.toLocalTime();
}

/*!
    Updates the information about the remote device, based on the local device
    cache.  Some information is generally not provided by the local adaptor
    until a low-level connection is made to the remote device.  Thus devices
    which are found by discoverRemoteDevices() will not contain the full
    information about the device. The remote device is given
    by \a device.

    Returns true on successful completion of the request; otherwise returns false.

    \sa discoverRemoteDevices(), remoteDevicesFound()
*/
bool QBluetoothLocalDevice::updateRemoteDevice(QBluetoothRemoteDevice &device) const
{
    QDBusReply<QDBusObjectPath> reply;
    QList<QVariant> args;
    args << device.address().toString();
    
    if(!m_data->iface()->btcall("FindDevice", reply, args))
        return false;
    
    QBluetoothDbusIface iface("org.bluez", reply.value().path(),
                              "org.bluez.Device", m_data->iface()->connection());

    uint cls = iface.getProperty("Class").toUInt();
    QString name = iface.getProperty("Name").toString();
    
    quint8 major = (cls >> 8) & 0x1F;
    quint8 minor = (cls >> 2) & 0x3F;
    quint8 service = (cls >> 16) & 0xFF;
 
    device.setDeviceMajor(major_to_device_major(major));
    device.setDeviceMinor(minor);
    device.setServiceClasses(QBluetooth::ServiceClasses(service));   
    device.setName(name);
    
    return true;
    
    /*
    
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return false;
    }

    QDBusReply<QString> reply;
    QString version, revision, manufacturer, company, name;

    reply = m_data->iface()->call("GetRemoteVersion",
                                  device.address().toString());
    if (reply.isValid()) {
        version = reply.value();
    }

    reply = m_data->iface()->call("GetRemoteRevision",
                                  device.address().toString());
    if (reply.isValid()) {
        revision = reply.value();
    }

    reply = m_data->iface()->call("GetRemoteManufacturer",
                                  device.address().toString());
    if (reply.isValid()) {
        manufacturer = reply.value();
    }

    reply = m_data->iface()->call("GetRemoteCompany",
                                  device.address().toString());
    if (reply.isValid()) {
        company = reply.value();
    }

    reply = m_data->iface()->call("GetRemoteName",
                                  device.address().toString());
    if (reply.isValid()) {
        name = reply.value();
    }

    QDBusReply<uint> reply2 = m_data->iface()->call("GetRemoteClass",
                                  device.address().toString());
    if (reply.isValid()) {
        quint8 major = (reply2.value() >> 8) & 0x1F;
        quint8 minor = (reply2.value() >> 2) & 0x3F;
        quint8 service = (reply2.value() >> 16) & 0xFF;
        device.setDeviceMajor(major_to_device_major(major));
        device.setDeviceMinor(minor);
        device.setServiceClasses(QBluetooth::ServiceClasses(service));
    }

    device.setVersion(version);
    device.setRevision(revision);
    device.setManufacturer(manufacturer);
    device.setCompany(company);
    device.setName(name);

    return true;*/
}

/*!
    Registers pairing agent for this adapter. Agent will be used to display PIN
    dialog when remote device connects to this adapter.

    \sa QBluetoothPasskeyAgent
*/
bool QBluetoothLocalDevice::registerAgent(QBluetoothPasskeyAgent *agent)
{
    QList<QVariant> args;
    QDBusReply<void> reply;
    QDBusObjectPath agentPath("/" + agent->name());
    QString capability;

    args << qVariantFromValue(agentPath);
    args << capability;

    return m_data->callAdapter("RegisterAgent", args, reply);
}

/*!
    Requests the local device to pair to a remote device found at address \a addr.
    This method returns true if the pairing request could be started and  false
    otherwise.

    \sa pairedDevices(), pairingCreated(), pairingFailed()
    \sa QBluetoothPasskeyAgent
*/
bool QBluetoothLocalDevice::requestPairing(const QBluetoothAddress &addr)
{
    if (m_data->m_agent == NULL)
        m_data->m_agent = new QBluetoothDefaultPasskeyAgent("DefaultAgent", this);
    
    QList<QVariant> args;
    QDBusReply<QDBusObjectPath> reply;
    QDBusObjectPath agentPath("/" + m_data->m_agent->name());
    QString capability;
    
    args << addr.toString();
    args << qVariantFromValue(agentPath);
    args << capability;

    PairingCancelledProxy *proxy = new PairingCancelledProxy(addr, m_data);
    
    return m_data->callAdapter("CreatePairedDevice", args, reply, true, proxy,
                               SLOT(createBondingReply(QDBusMessage)),
                               SLOT(createBondingError(QDBusError,QDBusMessage)));
}

/*!
    Requests the local device to remove its pairing to a remote device with
    address \a addr.  Returns true if the removal request could be
    queued successfully, false otherwise.  The signal pairingRemoved()
    will be sent if the pairing could be removed successfully.  An error() signal
    will be emitted if the pairing could not be removed.

    \sa pairingRemoved()
 */
bool QBluetoothLocalDevice::removePairing(const QBluetoothAddress &addr)
{
    QDBusReply<void> reply;
    QList<QVariant> args;
    
    QVariant devices = m_data->getProperty("Devices");
    if (!devices.isValid())
        return false;
      
    QList<QDBusObjectPath> list = qdbus_cast<QList<QDBusObjectPath> > (devices.value<QDBusArgument>());
    QString search = addr.toString().replace(':', '_');
    foreach (QDBusObjectPath path, list) {
        if(path.path().endsWith(search)) {
            args << qVariantFromValue(path);
            return m_data->callAdapter("RemoveDevice", args, reply, true);
        }
    }
    
    return false;
}

/*!
    Returns a list of Bluetooth device addresses which are currently paired
    to this device.  Pairing establishes a secret key which is used for encryption
    of all communication between the two devices.  The encryption key is based on
    a PIN which must be entered on both devices.

    Note that each device maintains its own list of paired devices
    and thus it is possible that while the local device trusts the peer,
    the peer might not trust the local device.

    The function will return a list of paired device addresses.  If an error
    occurred during a request, the return value will be an error, and error()
    will be set accordingly.

    \sa isPaired()
*/
QBluetoothReply<QList<QBluetoothAddress> > QBluetoothLocalDevice::pairedDevices() const
{
    QList<QBluetoothAddress> ret;
    
    QVariant devices = m_data->getProperty("Devices");
    if (!devices.isValid())
        return ret;
      
    QList<QDBusObjectPath> list = qdbus_cast<QList<QDBusObjectPath> > (devices.value<QDBusArgument>());
    foreach (QDBusObjectPath path, list) {
        ret.push_back(QBluetoothAddress(btAddr(path)));
    }
    return ret;
}

/*!
    Returns true if the local device is paired to a remote device, and false
    otherwise. In the case of an error, the return value is invalid and the
    error() is set accordingly. The address of the remote device is
    given by \a addr.

    \sa pairedDevices()
*/
QBluetoothReply<bool> QBluetoothLocalDevice::isPaired(const QBluetoothAddress &addr) const
{
    QList<QBluetoothAddress> list = pairedDevices();
    foreach (QBluetoothAddress a, list)
        if(a == addr)
            return true;
    
    return false;
}

/*!
    Attempts to cancel the pairing process.  In case of error,
    an error signal will be emitted.  In the case of success, a
    pairingFailed() signal will be emitted and error() set to
    QBluetoothLocalDevice::AuthenticationCancelled.  The address of
    the remote device is given by \a addr.

    Returns true if the request could be queued, false otherwise.

    \sa pairingFailed(), error()
 */
bool QBluetoothLocalDevice::cancelPairing(const QBluetoothAddress &addr)
{
    return m_data->callAdapter<void>("CancelDeviceCreation", addr.toString()).isValid();
}

/*!
    Returns the alias for a remote device.  If the alias is set, it should
    be used in preference to the device display name.  On success,
    returns the alias as a string.  The address of the remote device is given
    by \a addr.

    \sa setRemoteAlias(), removeRemoteAlias()
 */
QBluetoothReply<QString> QBluetoothLocalDevice::remoteAlias(const QBluetoothAddress &addr) const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QString>();
    }

    QDBusReply<QString> reply = m_data->iface()->call("GetRemoteAlias",
            addr.toString());

    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QString>();
    }

    return reply.value();
}

/*!
    Sets the alias for a remote device given by \a addr to alias \a alias.
    Returns true if the alias could be set, and false otherwise.

    \sa remoteAlias(), removeRemoteAlias()
*/
bool QBluetoothLocalDevice::setRemoteAlias(const QBluetoothAddress &addr,
                                           const QString &alias)
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return false;
    }

    QDBusReply<void> reply = m_data->iface()->call("SetRemoteAlias",
            addr.toString(), alias);

    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return false;
    }

    return true;
}

/*!
    Removes the alias for a remote device given by \a addr.
    Returns true if the alias could be removed, and false otherwise.

    \sa setRemoteAlias(), remoteAlias()
 */
bool QBluetoothLocalDevice::removeRemoteAlias(const QBluetoothAddress &addr)
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return false;
    }

    QDBusReply<void> reply = m_data->iface()->call("ClearRemoteAlias",
            addr.toString());

    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return false;
    }

    return true;
}

/*!
    Enables or Disables the Periodic Discovery Mode according to \a enabled.
    When in Periodic Discovery Mode the device will periodically run a device inquiry
    and report the results.  Returns true if the mode change operation succeeded,
    and false otherwise.

    \sa isPeriodicDiscoveryEnabled()
*/
bool QBluetoothLocalDevice::setPeriodicDiscoveryEnabled(bool enabled)
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return false;
    }

    QString meth = enabled ? "StartPeriodicDiscovery" : "StopPeriodicDiscovery";
    QDBusReply<void> reply = m_data->iface()->call(meth);

    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return false;
    }

    return true;
}

/*!
    Returns true if the device is currently in Periodic Discovery Mode.

    \sa setPeriodicDiscoveryEnabled()
*/
QBluetoothReply<bool> QBluetoothLocalDevice::isPeriodicDiscoveryEnabled() const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return false;
    }

    QDBusReply<bool> reply = m_data->iface()->call("IsPeriodicDiscovery");
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return false;
    }

    return reply.value();
}

/*!
    Returns the pin code length used when the device was paired to \a addr.  If the device
    is not paired, an invalid QBluetoothReply is returned.
*/
QBluetoothReply<uchar> QBluetoothLocalDevice::pinCodeLength(const QBluetoothAddress &addr) const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<uchar>();
    }

    QDBusReply<uchar> reply = m_data->iface()->call("GetPinCodeLength", addr.toString());

    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<uchar>();
    }

    return reply.value();
}

/*!
    Requests the Bluetooth device to disconnect the underlying low level connection to the
    remote bluetooth device at address \a addr.  This call will most likely require system
    administrator privileges.  The actual disconnection will happen several seconds later.
    First the remoteDeviceDisconnectRequested signal will be sent.  This method returns
    true if the disconnectRemoteDevice request succeeded, and false otherwise.

    \sa remoteDeviceDisconnectRequested()
*/
bool QBluetoothLocalDevice::disconnectRemoteDevice(const QBluetoothAddress &addr)
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return false;
    }

    QDBusReply<void> reply = m_data->iface()->call("DisconnectRemoteDevice", addr.toString());

    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return false;
    }

    return true;
}

/*!
    Returns a device address of the Bluetooth adapter.
    E.g. for "/org/bluez/923/hci0/dev_30_38_55_02_34_21" returns "30:38:55:02:34:21"
    Parameter \a adapterPath is full object path used for bluez dbus calls.
    
    \sa adapterPath()
 */
QString QBluetoothLocalDevice::adapterPathToDevAddr(QString adapterPath)
{
    return adapterPath.right(17).replace('_', ':');     // 
}

/*!
    Returns a device name of the Bluetooth adapter. This is typically 'hci0'.
    E.g. for "/org/bluez/923/hci0/dev_30_38_55_02_34_21" returns "hci0"
    Parameter \a adapterPath is full object path used for bluez dbus calls.

    \sa adapterPath()
 */
QString QBluetoothLocalDevice::adapterPathToDevName(QString adapterPath)
{
    QRegExp rx("hci\\d+");
    if(rx.indexIn(adapterPath) >= 0) {
        return rx.cap();
    }
    return adapterPath;
}

/*!
    Lists the addresses of all known devices, ones that have paired, seen or used.

    \sa lastUsed(), lastSeen()
*/
QBluetoothReply<QList<QBluetoothAddress> >
        QBluetoothLocalDevice::knownDevices() const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QList<QBluetoothAddress> >();
    }

    QDBusReply<QStringList> reply = m_data->iface()->call("ListRemoteDevices");
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QList<QBluetoothAddress> >();
    }

    QList<QBluetoothAddress> ret;

    foreach (QString addr, reply.value()) {
        ret.push_back(QBluetoothAddress(addr));
    }

    return ret;
}

/*!
    Lists the addresses of all known devices, ones that have paired, seen or used after
    the given \a date.

    \sa lastUsed(), lastSeen()
 */
QBluetoothReply<QList<QBluetoothAddress> >
        QBluetoothLocalDevice::knownDevices(const QDateTime &date) const
{
    if (!m_data->iface() || !m_data->iface()->isValid()) {
        return QBluetoothReply<QList<QBluetoothAddress> >();
    }

    // Time format is YYYY-MM-DD HH:MM:SS GMT
    QString strDate = date.toUTC().toString("yyyy-MM-dd hh:mm:ss");

    QDBusReply<QStringList> reply = m_data->iface()->call("ListRecentRemoteDevices", strDate);
    if (!reply.isValid()) {
        m_data->handleError(reply.error());
        return QBluetoothReply<QList<QBluetoothAddress> >();
    }

    QList<QBluetoothAddress> ret;

    foreach (QString addr, reply.value()) {
        ret.push_back(QBluetoothAddress(addr));
    }

    return ret;
}

QBluetoothDbusIface * QBluetoothLocalDevice::iface() const
{
    return m_data->iface();
}

/*!
    \fn void QBluetoothLocalDevice::nameChanged(const QString &name)

    This signal is emitted whenever a device name has been changed.  The
    \a name variable contains the new name.  Note that this signal can
    be triggered by external events (such as another program changing the
    device name).

 */

/*!
    \fn void QBluetoothLocalDevice::stateChanged(QBluetoothLocalDevice::State state)

    This signal is emitted whenever a device has entered a new state.  The
    \a state variable contains the new state.  Note that this signal can be
    triggered externally.
 */

/*!
    \fn void QBluetoothLocalDevice::error(QBluetoothLocalDevice::Error error, const QString &msg)

    This signal is emitted whenever an error has occurred.  The \a error variable
    contains the error that has occurred.  The \a msg variable contains the
    error message.

    \sa error()
 */

/*!
    \fn void QBluetoothLocalDevice::remoteDeviceConnected(const QBluetoothAddress &addr)

    This signal is emitted whenever a remote device has connected to the local
    device.  The \a addr parameter holds the address of the remote device. Note
    that this signal can be triggered externally.
 */

/*!
    \fn void QBluetoothLocalDevice::remoteDeviceDisconnected(const QBluetoothAddress &addr)

    This signal is emitted whenever a remote device has disconnected from the local
    device.  The \a addr parameter holds the address of the remote device.  Note
    that this signal can be triggered externally.
 */

/*!
    \fn void QBluetoothLocalDevice::discoveryStarted()

    This signal is emitted whenever a discovery scan has been initiated.  Note that
    this signal can be triggered by other instances of QBluetoothLocalDevice.

    \sa discoverRemoteDevices(), discoveryCompleted()
 */

/*!
    \fn void QBluetoothLocalDevice::remoteDeviceFound(const QBluetoothRemoteDevice &device)

    This signal is emitted whenever a device is discovered.  The \a device parameter
    contains the remote device discovered.  Note that this signal can be triggered by
    other instances of QBluetoothLocalDevice.

    \sa remoteDevicesFound()
 */

/*!
    \fn void QBluetoothLocalDevice::remoteDeviceDisappeared(const QBluetoothAddress &addr)

    This signal is emitted whenever a periodic discovery cycle has completed, and a previously
    detected device is no longer in range or invisible.  The \a addr parameter holds the
    address of the remote device.

    \sa setPeriodicDiscoveryEnabled()
 */

/*!
    \fn void QBluetoothLocalDevice::remoteDevicesFound(const QList<QBluetoothRemoteDevice> &list)

    This signal is emitted whenever a discovery procedure has finished.  It returns
    all devices discovered by the procedure. The \a list contains the list of all
    remote devices found.

    \sa remoteDeviceFound()
 */

/*!
    \fn void QBluetoothLocalDevice::discoveryCancelled()

    This signal is emitted whenever a discovery scan has been cancelled.

    \sa cancelDiscovery()
 */

/*!
    \fn void QBluetoothLocalDevice::discoveryCompleted()

    This signal is emitted whenever a discovery scan has been completed.

    \sa discoverRemoteDevices(), discoveryStarted()
 */

/*!
    \fn void QBluetoothLocalDevice::pairingCreated(const QBluetoothAddress &addr)

    This signal is emitted whenever a pairing request has completed
    successfully. The \a addr parameter holds the address just paired to.
    Note that this signal could be triggered by external events.
 */

/*!
    \fn void QBluetoothLocalDevice::pairingFailed(const QBluetoothAddress &addr)

    This signal is emitted whenever a pairing request has failed.  The \a addr
    parameter holds the address of the remote device.
 */

/*!
    \fn void QBluetoothLocalDevice::pairingRemoved(const QBluetoothAddress &addr)

    This signal is emitted whenever a pairing has been removed. The \a addr
    parameter holds the address of the remote device.  Note that this signal
    could be triggered by external events.
 */

/*!
    \fn void QBluetoothLocalDevice::remoteAliasChanged(const QBluetoothAddress &addr, const QString &alias)

    This signal is emitted whenever a remote device's alias has been changed.
    The \a addr contains the address of the remote device, and \a alias contains
    the new alias.  This signal can be triggered externally.
*/

/*!
    \fn void QBluetoothLocalDevice::remoteAliasRemoved(const QBluetoothAddress &addr)

    This signal is emitted whenever a remote device's alias has been removed.
    The \a addr contains the address of the remote device.
    This signal can be triggered externally.
 */

/*!
    \fn void QBluetoothLocalDevice::remoteNameUpdated(const QBluetoothAddress &addr, const QString &name)

    This signal is emitted whenever the remote device's name has been changed.  This
    can occur when a never seen device has been discovered and the name was obtained
    (in this case its bluetooth address should be used for display purposes until
    this signal is emitted), or if the device name has changed since last communication.
    The \a addr parameter holds the address of the Bluetooth device.  The \a name
    contains the new name.

    \sa remoteNameFailed()
*/

/*!
    \fn void QBluetoothLocalDevice::remoteNameFailed(const QBluetoothAddress &addr)

    This signal is emitted whenever a request for a remote device's name has failed.  The
    \a addr parameter holds the address of the device.

    \sa remoteNameUpdated()
*/

/*!
    \fn void QBluetoothLocalDevice::remoteClassUpdated(const QBluetoothAddress &addr,  QBluetooth::DeviceMajor major, quint8 minor, QBluetooth::ServiceClasses serviceClasses)

    This signal is emitted whenever a remote device's class has changed.  The \a addr
    parameter holds the address of the device, the \a major, \a minor and \a serviceClasses
    parameters hold the Device Major, Device Minor and Device Service Classes information,
    respectively.
*/

/*!
    \fn void QBluetoothLocalDevice::discoverableTimeoutChanged(uint timeout)

    This signal is emitted whenever a discoverable timeout for a device has been changed.
    The \a timeout parameter holds the new timeout.

    \sa discoverableTimeout(), setDiscoverable()
*/

/*!
    \fn void QBluetoothLocalDevice::remoteDeviceDisconnectRequested(const QBluetoothAddress &addr)

    This signal is emitted whenever a disconnectRemoteDevice request has been issued to
    the Bluetooth system.  Services can act upon this signal in order to shut down the
    connection gracefully to the particular device before the underlying low-level
    connection is terminated.  The actual disconnection will happen several seconds later.
    The \a addr parameter holds the address of the device being disconnected.

    \sa disconnectRemoteDevice()
*/

#include "qbluetoothlocaldevice.moc"
