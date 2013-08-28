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

#include "qbluetoothabstractservice.h"

#include <qbluetoothdbus.h>
#include <qbluetoothaddress.h>
#include <qbluetoothsdprecord.h>
#include <qbluetoothlocaldevice.h>
#include "qsdpxmlgenerator_p.h"
#include <qtopiaipcadaptor.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusError>
#include <QDBusMessage>

#include <QTimer>
#include <QFile>
#include <QBuffer>

#include <QDebug>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>


/*
    This class passes messages (over IPC) between a QBluetoothAbstractService 
    and the BluetoothServiceManager (in the QPE server).
 */
class Messenger : public QtopiaIpcAdaptor
{
    friend class QBluetoothAbstractService;
    Q_OBJECT
public:
    Messenger(QBluetoothAbstractService *service);

    QBluetoothAbstractService *m_service;

public slots:
    void startService(const QString &name);
    void stopService(const QString &name);
    void setSecurityOptions(const QString &name, QBluetooth::SecurityOptions options);

private slots:
    void registerSelf();
    void started(bool error, const QString &);
    void stopped();

signals:
    void registerService(const QString &name, const QString &translatableName);
    void serviceStarted(const QString &name, bool error, const QString &errorDesc);
    void serviceStopped(const QString &name);
};

Messenger::Messenger(QBluetoothAbstractService *service)
    : QtopiaIpcAdaptor("QPE/BluetoothServiceProviders", service),
      m_service(service)
{
    publishAll(SignalsAndSlots);

    QObject::connect(service, SIGNAL(started(bool,QString)),
                     SLOT(started(bool,QString)));
    QObject::connect(service, SIGNAL(stopped()), SLOT(stopped()));

    // register the service at the end of this run loop
    QTimer::singleShot(0, this, SLOT(registerSelf()));
}

void Messenger::startService(const QString &name)
{
    if (name == m_service->name())
        m_service->start();
}

void Messenger::stopService(const QString &name)
{
    if (name == m_service->name())
        m_service->stop();
}

void Messenger::setSecurityOptions(const QString &name, QBluetooth::SecurityOptions options)
{
    if (name == m_service->name())
        m_service->setSecurityOptions(options);
}

void Messenger::registerSelf()
{
    emit registerService(m_service->name(), m_service->displayName());
}

void Messenger::started(bool error, const QString &desc)
{
    emit serviceStarted(m_service->name(), error, desc);
}

void Messenger::stopped()
{
    emit serviceStopped(m_service->name());
}


// ========================================

class QBluetoothAbstractServicePrivate : public QObject
{
    Q_OBJECT

public:
    QBluetoothAbstractServicePrivate(const QString &name,
                                     const QString &displayName,
                                     const QString &description,
                                     QBluetoothAbstractService *parent);
    ~QBluetoothAbstractServicePrivate();

    quint32 registerRecord(const QString &record);
    bool updateRecord(quint32 handle, const QString &record);
    bool unregisterRecord(quint32 handle);

    bool requestAuthorization(const QBluetoothAddress &addr, const QString &uuid);
    bool cancelAuthorization(const QBluetoothAddress &addr, const QString &uuid);

    void handleError(const QDBusError &error);

    QString m_name;
    QString m_displayName;
    QString m_description;
    QBluetoothDbusIface *m_ifc;
    QBluetoothAbstractService::Error m_error;
    QString m_errorString;
    bool m_isRegistered;

private slots:
    void serviceStarted(bool error, const QString &description);
    void serviceStopped();

    void authorizationFailed(const QDBusError &error, const QDBusMessage &msg);
    void authorizationSucceeded(const QDBusMessage &msg);
};

QBluetoothAbstractServicePrivate::QBluetoothAbstractServicePrivate(const QString &name,
                                                                   const QString &displayName,
                                                                   const QString &description,
                                                                   QBluetoothAbstractService *parent)
    : QObject(parent),
      m_name(name),
      m_displayName(displayName),
      m_description(description),
      m_ifc(NULL),
      m_error(QBluetoothAbstractService::NoError),
      m_isRegistered(false)
{
    QDBusConnection dbc = QDBusConnection::systemBus();

    QBluetoothDbusIface manager("org.bluez", "/", "org.bluez.Manager", dbc);

    QDBusReply<QDBusObjectPath> reply;
    if(manager.btcall("DefaultAdapter", reply)) {
        m_ifc = new QBluetoothDbusIface("org.bluez", reply.value().path(),
                                        "org.bluez.Service", dbc);
    }

    QObject::connect(parent, SIGNAL(started(bool,QString)),
                     SLOT(serviceStarted(bool,QString)));
    QObject::connect(parent, SIGNAL(stopped()), SLOT(serviceStopped()));
}

QBluetoothAbstractServicePrivate::~QBluetoothAbstractServicePrivate()
{
    if (m_ifc && m_ifc->isValid() && m_isRegistered) {
        QDBusReply<void> reply = m_ifc->call("UnregisterService", m_name);
        if (!reply.isValid()) {
            qWarning() << "Unable to unregister with BlueZ service manager...";
            handleError(reply.error());
        }
    }

    delete m_ifc;
}

void QBluetoothAbstractServicePrivate::serviceStarted(bool error, const QString &)
{
    if (error)
        return;

    if (!m_ifc || !m_ifc->isValid())
        return;

    QDBusReply<void> reply = m_ifc->call("RegisterService", m_name, m_displayName, m_description);
    if (!reply.isValid()) {
        qWarning() << "Unable to register with BlueZ service manager...";
        handleError(reply.error());
    }

    m_isRegistered = true;
}

void QBluetoothAbstractServicePrivate::serviceStopped()
{
    if (!m_ifc || !m_ifc->isValid())
        return;

    QDBusReply<void> reply = m_ifc->call("UnregisterService", m_name);
    if (!reply.isValid()) {
        qWarning() << "Unable to unregister with BlueZ service manager...";
        handleError(reply.error());
    }

    m_isRegistered = false;
}

struct bluez_error_mapping
{
    const char *name;
    QBluetoothAbstractService::Error error;
};

static bluez_error_mapping bluez_errors[] = {
    { "org.bluez.Error.InvalidArguments", QBluetoothAbstractService::InvalidArguments },
    { "org.bluez.Error.NotConnected", QBluetoothAbstractService::NotConnected },
    { "org.bluez.Error.NotAuthorized", QBluetoothAbstractService::NotAuthorized },
    { "org.bluez.Error.NotAvailable", QBluetoothAbstractService::NotAvailable },
    { "org.bluez.Error.DoesNotExist", QBluetoothAbstractService::DoesNotExist },
    { "org.bluez.Error.Failed", QBluetoothAbstractService::Failed },
    { "org.bluez.Error.Rejected", QBluetoothAbstractService::Rejected },
    { "org.bluez.Error.Canceled", QBluetoothAbstractService::Canceled },
    { NULL, QBluetoothAbstractService::NoError }
};

void QBluetoothAbstractServicePrivate::handleError(const QDBusError &error)
{
    m_error = QBluetoothAbstractService::UnknownError;

    int i = 0;
    while (bluez_errors[i].name) {
        if (error.name() == bluez_errors[i].name) {
            m_error = bluez_errors[i].error;
            break;
        }
        i++;
    }

    m_errorString = error.message();
}

quint32 QBluetoothAbstractServicePrivate::registerRecord(const QString &record)
{
    if(!m_ifc)
       return 0;

    QDBusReply<quint32> reply;
    QList<QVariant> args;
    args << record;
    
    if(m_ifc->btcall("AddRecord", reply, args))
        return reply.value();

    handleError(reply.error());
    return 0;
}

bool QBluetoothAbstractServicePrivate::updateRecord(quint32 handle, const QString &record)
{
    QDBusReply<void> reply;
    QList<QVariant> args;
    args << handle;
    args << record;    
    
    if(m_ifc->btcall("UpdateRecord", reply, args))
        return true;

    handleError(reply.error());
    return false;
}

bool QBluetoothAbstractServicePrivate::unregisterRecord(quint32 handle)
{
    QDBusReply<void> reply;
    QList<QVariant> args;
    args << handle;

    if(m_ifc->btcall("RemoveRecord", reply, args))
        return true;
    
    handleError(reply.error());
    return false;
}

void QBluetoothAbstractServicePrivate::authorizationFailed(const QDBusError &error, const QDBusMessage &)
{
    QBluetoothAbstractService *p = static_cast<QBluetoothAbstractService *>(parent());
    handleError(error);
    emit p->authorizationFailed();
}

void QBluetoothAbstractServicePrivate::authorizationSucceeded(const QDBusMessage &)
{
    QBluetoothAbstractService *p = static_cast<QBluetoothAbstractService *>(parent());
    emit p->authorizationSucceeded();
}

bool QBluetoothAbstractServicePrivate::requestAuthorization(const QBluetoothAddress &addr, const QString &uuid)
{
    Q_UNUSED(uuid)  // Not used in BlueZ right now

    if (!m_ifc || !m_ifc->isValid())
        return false;

    QList<QVariant> args;
    args << addr.toString();
    args << uuid;

    return m_ifc->callWithCallback("RequestAuthorization", args, this,
                                    SLOT(authorizationSucceeded(QDBusMessage)),
                                    SLOT(authorizationFailed(QDBusError,QDBusMessage)));
}

bool QBluetoothAbstractServicePrivate::cancelAuthorization(const QBluetoothAddress &addr, const QString &uuid)
{
    if (!m_ifc || !m_ifc->isValid())
        return false;

    QDBusReply<void> reply = m_ifc->call("CancelAuthorizationRequest",
                                         addr.toString(), uuid);

    if (!reply.isValid()) {
        handleError(reply.error());
        return false;
    }

    return true;
}

// ============================================================

/*!
    \class QBluetoothAbstractService
    \inpublicgroup QtBluetoothModule

    \brief  The QBluetoothAbstractService class provides a base interface class for Bluetooth services within Qtopia.

    To create a Qt Extended Bluetooth service, subclass QBluetoothAbstractService
    and implement the start(), stop() and setSecurityOptions() methods. Your
    service will automatically be registered and accessible as a Bluetooth
    service within Qtopia. This means the service will be accessible to
    external parties through QBluetoothServiceController. It will also be
    shown in the list of local services in the Bluetooth settings
    application, allowing end users to modify the service's settings.

    Naturally, it is possible to implement Bluetooth services outside of
    Qt Extended by using the BlueZ libraries and standard Linux tools. However,
    such services will not be accessible through Qtopia.

    \sa {Tutorial: Creating a Bluetooth service}

    \ingroup qtopiabluetooth
*/


/*!
    Constructs a Bluetooth service. The \a name is a unique name that identifies
    the service, and \a displayName is a user-friendly, internationalized name
    for this service that can be displayed to the end user. The \a parent is
    the QObject parent for this service.
 */
QBluetoothAbstractService::QBluetoothAbstractService(const QString &name, const QString &displayName, QObject *parent)
    : QObject(parent),
      m_data(new QBluetoothAbstractServicePrivate(name, displayName, QString(), this))
{
    new Messenger(this);
}

/*!
    Constructs a Bluetooth service.  The \a name is a unique name that identifies
    the service.  The \a displayName is a user-friendly, internationalized name
    for this service that can be displayed to the end user.  The \a description
    is a translated description that might be displayed to the user.  The \a parent
    is the QObject parent of this object.
*/
QBluetoothAbstractService::QBluetoothAbstractService(const QString &name, const QString &displayName, const QString &description, QObject *parent)
    : QObject(parent),
        m_data(new QBluetoothAbstractServicePrivate(name, displayName, description, this))
{
    new Messenger(this);
}

/*!
    Destroys a Bluetooth service.
 */
QBluetoothAbstractService::~QBluetoothAbstractService()
{
}

/*!
    Registers the SDP service record \a record for this Bluetooth service
    and returns the service record handle of the newly registered service.
    Returns zero if the registration failed.

    \sa unregisterRecord(), updateRecord()
 */
quint32 QBluetoothAbstractService::registerRecord(const QBluetoothSdpRecord &record)
{
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QSdpXmlGenerator::generate(record, &buffer);

    buffer.seek(0);
    return m_data->registerRecord(QString::fromUtf8(buffer.readAll()));
}

/*!
    Uses the XML data from the file \a filename to register the SDP service
    record for this Bluetooth service and returns the service record handle of 
    the newly registered service. Returns zero if the registration failed.

    See \l {Tutorial: Creating a Bluetooth service#Using a XML-formatted SDP record}{Using a XML-formatted SDP record} for details on how to
    generate a XML-formatted SDP reord.

    \warning The given file must be UTF-8 encoded to be parsed correctly.

    \sa unregisterRecord(), updateRecord()
 */
quint32 QBluetoothAbstractService::registerRecord(const QString &filename)
{
    int fd = ::open(QFile::encodeName(filename), O_RDONLY);
    if (fd == -1) {
        m_data->m_errorString = tr("File could not be opened");
        return 0;
    }

    quint32 result = 0;

    // from QResource
    struct stat st;
    if (fstat(fd, &st) != -1) {
        uchar *ptr;
        ptr = reinterpret_cast<uchar *>(
                mmap(0, st.st_size,             // any address, whole file
                     PROT_READ,                 // read-only memory
                     MAP_FILE | MAP_PRIVATE,    // swap-backed map from file
                     fd, 0));                   // from offset 0 of fd
        if (ptr && ptr != reinterpret_cast<uchar *>(MAP_FAILED)) {
            // register the record
            result = m_data->registerRecord( QString::fromUtf8((const char*)ptr) );

            // unmap to clean up
            munmap(ptr, st.st_size);
        }
    }
    ::close(fd);

    return result;
}

/*!
    Updates the already registered SDP service record with \a handle to the
    contents of the new \a record.

    Returns true if the update succeeded, and false otherwise.

    \sa unregisterRecord(), registerRecord()
 */
bool QBluetoothAbstractService::updateRecord(quint32 handle, const QBluetoothSdpRecord &record)
{
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QSdpXmlGenerator::generate(record, &buffer);

    buffer.seek(0);
    return m_data->updateRecord(handle, QString::fromUtf8(buffer.readAll()));
}

/*!
    Uses the XML data from the file \a filename to update the already
    registered SDP record with \a handle.  Returns true if the update
    succeeded and false otherwise.

    \warning The given file must be UTF-8 encoded to be parsed correctly.

    \sa unregisterRecord(), registerRecord()
 */
bool QBluetoothAbstractService::updateRecord(quint32 handle, const QString &filename)
{
    int fd = ::open(QFile::encodeName(filename), O_RDONLY);
    if (fd == -1) {
        qWarning("QBluetoothAbstractService: error opening file %s, cannot register SDP record", filename.toLatin1().constData());
        return 0;
    }

    quint32 result = 0;

    // from QResource
    struct stat st;
    if (fstat(fd, &st) != -1) {
        uchar *ptr;
        ptr = reinterpret_cast<uchar *>(
                mmap(0, st.st_size,             // any address, whole file
                     PROT_READ,                 // read-only memory
                     MAP_FILE | MAP_PRIVATE,    // swap-backed map from file
                     fd, 0));                   // from offset 0 of fd
        if (ptr && ptr != reinterpret_cast<uchar *>(MAP_FAILED)) {
            // register the record
            result = m_data->updateRecord(handle, QString::fromUtf8((const char*)ptr) );

            // unmap to clean up
            munmap(ptr, st.st_size);
        }
    }
    ::close(fd);

    return result;
}

/*!
    Unregisters the SDP service record with the service record handle
    \a handle.

    Returns whether the record was successfully unregistered.

    \sa registerRecord(), updateRecord()
 */
bool QBluetoothAbstractService::unregisterRecord(quint32 handle)
{
    return m_data->unregisterRecord(handle);
}

/*!
    Returns the unique name that identifies this service.

    \sa displayName(), description()
 */
QString QBluetoothAbstractService::name() const
{
    return m_data->m_name;
}

/*!
    Returns the description for this service.

    \sa name(), displayName()
*/
QString QBluetoothAbstractService::description() const
{
    return m_data->m_description;
}

/*!
    Returns the user-friendly, internationalized name for this service that can
    be displayed to the end user.

    \sa name(), description()
 */
QString QBluetoothAbstractService::displayName() const
{
    return m_data->m_displayName;
}

/*!
    Request authorization for the use of this service by Bluetooth device
    with \a remote address.  The \a uuid parameter can be used to identify the
    service (e.g. can be the UUID of the service as used in the SDP service
    record).

    Returns true if the request could be started, and false otherwise.

    \sa authorizationSucceeded(), cancelAuthorization(), authorizationFailed()
*/
bool QBluetoothAbstractService::requestAuthorization(const QBluetoothAddress &remote, const QString &uuid)
{
    return m_data->requestAuthorization(remote, uuid);
}

/*!
    Cancel the previous authorization request.  The \a remote and \a uuid
    parameters must match the parameters sent with the authorization request.

    Returns true if the request could be canceled.

    \sa requestAuthorization(), authorizationFailed()
*/
bool QBluetoothAbstractService::cancelAuthorization(const QBluetoothAddress &remote, const QString &uuid)
{
    return m_data->cancelAuthorization(remote, uuid);
}

/*!
    \fn void QBluetoothAbstractService::authorizationSucceeded()

    This signal will be emitted when the authorization request has been
    granted.

    \sa authorizationFailed()
*/

/*!
    \fn void QBluetoothAbstractService::authorizationFailed()

    This signal will be emitted when the authorization request has denied.

    \sa authorizationSucceeded(), error(), errorString()
*/

/*!
    \enum QBluetoothAbstractService::Error
    \brief Possible errors that might occur.

    \value NoError No error.
    \value InvalidArguments Invalid arguments were passed.
    \value NotConnected The remote device is not connected.
    \value NotAuthorized The service trying to perform the action does not have permission to manipulate the resource.
    \value NotAvailable The resource is not available.
    \value DoesNotExist The requested resource does not exist.
    \value Failed Operation failed.
    \value Rejected Authorization request was rejected by the user or it was not accepted in due time.
    \value Canceled Authorization request was canceled.
    \value UnknownError Unknown error.
*/

/*!
    Returns the last error that has occurred.

    \sa errorString()
*/
QBluetoothAbstractService::Error QBluetoothAbstractService::error() const
{
    return m_data->m_error;
}

/*!
    Returns an error message for the last error that has occurred.

    \sa error()
*/
QString QBluetoothAbstractService::errorString() const
{
    return m_data->m_errorString;
}

/*!
    \fn void QBluetoothAbstractService::start()

    Starts this service.

    This method will be called by Qt Extended when the service should be started.
    This may be because an external party has required that the service be
    started (for example, through QBluetoothServiceController, or through the
    Bluetooth Settings application) or because Qt Extended has been configured to
    start the service automatically.

    Subclasses must override this to start the service appropriately. The
    subclass must emit started() when the service has started, or failed
    while trying to start, to announce the result of the start() invocation.

    \warning This function must be implementated in such a way that any intermediate
    objects (which have been created up to the point where the error occurred)
    are cleaned up before the error signal is emitted.

    \sa started(), stop()
 */

/*!
    \fn void QBluetoothAbstractService::stop()

    Stops this service.

    This method will be called by Qt Extended when the service should be stopped.

    Subclasses must override this to stop the service appropriately. The
    subclass must emit stopped() to announce that the service has stopped.

    \sa stopped(), start()
 */

/*!
    \fn void QBluetoothAbstractService::setSecurityOptions(QBluetooth::SecurityOptions options)

    Sets the security options for this service to the given \a options.

    This method will be called by Qt Extended when the security options should be
    changed.

    Subclasses must override this to set the security options for this service.
 */

/*!
    \fn void QBluetoothAbstractService::started(bool error, const QString &description)

    When implementing the start() function, this signal must be emitted by the
    subclass when the service has started or failed while attempting to start,
    to announce the result of the start() invocation.

    If the service failed to start, \a error should be true and \a description 
    should be a human-readable description of the error. Otherwise, \a error 
    should be false and \a description should be a null QString.

    \sa start(), stopped()
 */

/*!
    \fn void QBluetoothAbstractService::stopped()

    This signal must be emitted by the subclass inside the implementation of
    the stop() function, to announce that the service has stopped.

    \sa started(), stop()
 */

#include "qbluetoothabstractservice.moc"
