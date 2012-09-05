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

#include "btpowerservice.h"
#include <qbluetoothlocaldevicemanager.h>
#include <qtopialog.h>
#include "qtopiaserverapplication.h"
#include <QByteArray>
#include <QString>
#include <QTimer>
#include <QObject>
#include <QPhoneProfileManager>
#include <QSettings>
#include <QValueSpaceObject>

#include <qbluetoothaddress.h>

#include <unistd.h>

class BtPowerServicePrivate
{
public:
    BtPowerServicePrivate(const QByteArray &devId);
    ~BtPowerServicePrivate();

    QBluetoothLocalDevice *m_device;
    QPhoneProfileManager *m_phoneProfileMgr;
    bool upRequest;
    QSettings *m_btsettings;
    QValueSpaceObject *m_localDeviceValues;
    QBluetoothLocalDevice::State m_prevState;
    bool m_stateBeforePlaneModeOn;
};

BtPowerServicePrivate::BtPowerServicePrivate(const QByteArray &devId)
{
    m_device = new QBluetoothLocalDevice(devId);
    qLog(Bluetooth) << "BtPowerServicePrivate: Created local device:"
        << devId << m_device->address().toString();

    m_phoneProfileMgr = new QPhoneProfileManager;
    m_btsettings = new QSettings("Trolltech", "Bluetooth");
    m_localDeviceValues = new QValueSpaceObject("/Communications/Bluetooth/LocalDevice");
}

BtPowerServicePrivate::~BtPowerServicePrivate()
{
    delete m_device;
    delete m_phoneProfileMgr;
    delete m_btsettings;
    delete m_localDeviceValues;
}

/*!
    \class BtPowerService
    \inpublicgroup QtBluetoothModule
    \ingroup QtopiaServer::Task::Bluetooth
    \internal
    \brief The BtPowerService class provides the Qt Extended Bluetooth Power service.

    The \i BtPower service enables applications to notify the server
    of Bluetooth device useage, such that the server can intelligently
    manage the bluetooth device for maximum power efficiency.

    The \i BtPower service is typically supplied by the Qt Extended server,
    but the system integrator might change the application that
    implements this service.

    This class is part of the Qt Extended server and cannot be used by other QtopiaApplications.
    \sa QCommDeviceController, QCommDeviceSession
 */

/*!
    Creates a new BtPowerService with the \a serverPath specifying
    the path to use for the underlying UNIX socket.  The \a devId
    specifies the device this BtPowerService is managing.  The QObject
    parent is given by \a parent.
 */
BtPowerService::BtPowerService(const QByteArray &serverPath,
                               const QByteArray &devId, QObject *parent)
    : QAbstractCommDeviceManager(serverPath, devId, parent)
{
    m_data = new BtPowerServicePrivate(devId);

    qLog(Bluetooth) << "Bluetooth Power Service created";

    connect(m_data->m_device, SIGNAL(stateChanged(QBluetoothLocalDevice::State)),
            this, SLOT(stateChanged(QBluetoothLocalDevice::State)));
    connect(m_data->m_device, SIGNAL(error(QBluetoothLocalDevice::Error,QString)),
            this, SLOT(error(QBluetoothLocalDevice::Error,QString)));

    connect(m_data->m_phoneProfileMgr, SIGNAL(planeModeChanged(bool)),
            this, SLOT(planeModeChanged(bool)));

    // init value space values
    m_data->m_localDeviceValues->setAttribute("Enabled", isUp());
    m_data->m_localDeviceValues->setAttribute("Visible", m_data->m_device->discoverable().value());

    if (m_data->m_device->discoverable())
        m_data->m_prevState = QBluetoothLocalDevice::Discoverable;
    else if (m_data->m_device->connectable())
        m_data->m_prevState = QBluetoothLocalDevice::Connectable;
    else
        m_data->m_prevState = QBluetoothLocalDevice::Off;

    // ensure the service is down if plane mode is on
    m_data->m_stateBeforePlaneModeOn = m_data->m_prevState;
//    if (m_data->m_phoneProfileMgr->planeMode()) {
//        bringDown();
//    }
    
    // Always start with bluetooth turned off so that we save power
    bringDown();
}

/*!
    Destructor.
 */
BtPowerService::~BtPowerService()
{
    if (m_data)
        delete m_data;
}

/*!
    \reimp
*/
void BtPowerService::bringUp()
{
    bool res;

    // preserve last known device visibility setting
    // (or default to discoverable if there is no such setting)
    QVariant visibility = m_data->m_btsettings->value("LocalDeviceVisible");
    if (!visibility.isValid() || visibility.toBool())
        res = m_data->m_device->setDiscoverable();
    else
        res = m_data->m_device->setConnectable();

    m_data->upRequest = true;

    if (!res)
        emit upStatus(true, tr("Could not bring up bluetooth device"));
}

/*!
    \reimp
*/
void BtPowerService::bringDown()
{
    bool res = m_data->m_device->turnOff();

    m_data->upRequest = false;

    if (!res)
        emit downStatus(true, tr("Could not bring down bluetooth device"));
}

/*!
    \reimp
*/
bool BtPowerService::isUp() const
{
    return m_data->m_device->isUp();
}

/*!
    \internal
*/
void BtPowerService::stateChanged(QBluetoothLocalDevice::State state)
{
    QBluetoothLocalDevice::State prevState = m_data->m_prevState;
    m_data->m_prevState = state;

    if ( (state == QBluetoothLocalDevice::Connectable) ||
         (state == QBluetoothLocalDevice::Discoverable)) {

        // don't send signal if just changing between connectable <-> discoverable
        if ( (prevState != QBluetoothLocalDevice::Connectable) &&
                (prevState != QBluetoothLocalDevice::Discoverable) ) {
            emit upStatus(false, QString());
        }

        // this is to restore the visibility setting when a device is brought 
        // back up again
        m_data->m_btsettings->setValue("LocalDeviceVisible",
            QVariant((state == QBluetoothLocalDevice::Discoverable)) );

        // this is used for determining the bluetooth status
        // icon in the home screen status bar
        m_data->m_localDeviceValues->setAttribute("Enabled", true);
        m_data->m_localDeviceValues->setAttribute("Visible",
                (state == QBluetoothLocalDevice::Discoverable));

    } else {
        emit downStatus(false, QString());
        m_data->m_localDeviceValues->setAttribute("Enabled", false);
        m_data->m_localDeviceValues->setAttribute("Visible", false);
    }
}

/*!
    \internal
*/
void BtPowerService::error(QBluetoothLocalDevice::Error, const QString& /*msg*/)
{
    if (m_data->upRequest) {
        emit upStatus(true, tr("Could not bring up bluetooth device"));
    }
    else {
        emit downStatus(true, tr("Could not bring down bluetooth device"));
    }
}

/*!
    \internal
*/
void BtPowerService::planeModeChanged(bool enabled)
{
    // switch the device off if plane mode is switched on, and vice-versa
    if (enabled) {
        m_data->m_stateBeforePlaneModeOn = m_data->m_prevState;
        bringDown();
    } else {
        // don't bring up device if it was off before phone went to plane mode
        if (m_data->m_stateBeforePlaneModeOn != QBluetoothLocalDevice::Off)
            bringUp();
    }
}

/*!
    \reimp
*/
bool BtPowerService::shouldBringDown(QUnixSocket *) const
{
    return true;
}

/*!
  \class BtPowerServiceTask
    \inpublicgroup QtBluetoothModule
  \ingroup QtopiaServer::Task::Bluetooth
  \brief The BtPowerServiceTask class provides the BtPowerService.

  The \i BtPower service enables applications to notify the server
  of Bluetooth device useage, such that the server can intelligently
  manage the bluetooth device for maximum power efficiency.

  The \i BtPower service is typically supplied by the Qt Extended server,
  but the system integrator might change the application that
  implements this service.
  
  This class is part of the Qt Extended server and cannot be used by other QtopiaApplications.
*/

/*!
    \internal
*/
BtPowerServiceTask::BtPowerServiceTask(QObject *parent)
    : QObject(parent), m_btPower(0)
{
    QBluetoothLocalDeviceManager *mgr = new QBluetoothLocalDeviceManager(this);

    // get notifications when a local device is added or removed
    connect(mgr, SIGNAL(deviceAdded(QString)),
            SLOT(deviceAdded(QString)));
    connect(mgr, SIGNAL(deviceRemoved(QString)),
            SLOT(deviceRemoved(QString)));
    connect(mgr, SIGNAL(defaultDeviceChanged(QString)),
            SLOT(defaultDeviceChanged(QString)));

    //we start this once the GUI is up and running
    serverWidgetVsi = new QValueSpaceItem("/System/ServerWidgets/Initialized", this);
    connect( serverWidgetVsi, SIGNAL(contentsChanged()), this, SLOT(delayedServiceStart()) );
    delayedServiceStart(); //in case its visible already
}


/*!
    \internal
*/
BtPowerServiceTask::~BtPowerServiceTask()
{
    if (m_btPower) {
        m_btPower->stop();
        delete m_btPower;
        m_btPower = 0;
    }
}

/*!
  \internal
  */
void BtPowerServiceTask::delayedServiceStart()
{
    if ( serverWidgetVsi && serverWidgetVsi->value( QByteArray(), false ).toBool() ) {
        serverWidgetVsi->disconnect();
        serverWidgetVsi->deleteLater();
        serverWidgetVsi = 0;
        QTimer::singleShot( 5000, this, SLOT(startService()) );
    }
}

void BtPowerServiceTask::defaultDeviceChanged(const QString &devName)
{
   qLog(Bluetooth) << "BtPowerServiceTask::defaultDeviceChanged" << devName;
}

/*!
    \internal
*/
void BtPowerServiceTask::deviceAdded(const QString &devName)
{
    qLog(Bluetooth) << "BtPowerServiceTask::deviceAdded" << devName;

    if (!m_btPower)
        QTimer::singleShot(200, this, SLOT(startService()));
}

/*!
    \internal
*/
void BtPowerServiceTask::deviceRemoved(const QString &devName)
{
    qLog(Bluetooth) << "BtPowerServiceTask::deviceRemoved" << devName;

    // stop the power service if its device has been removed
    if (m_btPower && m_btPower->deviceId() == devName && m_btPower->isStarted()) {
        m_btPower->stop();
        delete m_btPower;
        m_btPower = 0;
    }
}

/*!
    \internal
*/
void BtPowerServiceTask::startService()
{
    qLog(Bluetooth) << "BtPowerServiceTask::startService";

    if (!m_btPower) {
        QBluetoothLocalDeviceManager manager;
        QString devName = manager.defaultDevice();
        if (devName.isNull()) {
            qLog(Bluetooth) << "BtPowerServiceTask: cannot start BtPowerService, no local device available";
            return;
        }

        qLog(Bluetooth) << "BtPowerServiceTask: creating btpowerservice...";
        QByteArray path( (Qtopia::tempDir()+"bt_power_"+devName).toLocal8Bit() );

        m_btPower = new BtPowerService(path, devName.toLatin1(), this);
        m_btPower->start();
    }
}

QTOPIA_TASK(BtPowerService, BtPowerServiceTask);
