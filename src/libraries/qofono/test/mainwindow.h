#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDebug>
#include <QMainWindow>
#include <QtDBus>
#include <QTimer>
#include <QMessageBox>
#include <QLabel>
#include <QListWidget>

#include "qofonoutil.h"
#include "qofonomanager.h"
#include "qofonomodem.h"
#include "qofonocellbroadcast.h"
#include "qofonodbuspendingreply.h"
#include "qofonoobjectlist.h"
#include "qofonosimmanager.h"
#include "qofonovoicecallmanager.h"
#include "qofonoconnectionmanager.h"
#include "qofonophonebook.h"
#include "qofonostringmap.h"
#include "qofonodbusabstractinterface.h"
#include "qofonomessagemanager.h"
#include "qofonopushnotification.h"
#include "qofonosupplementaryservices.h"
#include "qofonodbuspendingcall.h"
#include "qofononetworkregistration.h"
#include "qofonoradiosettings.h"
#include "qofonovoicecall.h"
#include "qofononetworkoperator.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0, Qt::WFlags f = 0);
    ~MainWindow();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::MainWindow *ui;
    OrgOfonoManagerInterface oManager;
    OrgOfonoModemInterface oModem;
    OrgOfonoCellBroadcastInterface oCellBroadcast;
    OrgOfonoConnectionManagerInterface oConnMan;
    OrgOfonoMessageManagerInterface oMessageManager;
    OrgOfonoNetworkRegistrationInterface oNetReg;
    OrgOfonoPhonebookInterface oPhoneBook;
    OrgOfonoPushNotificationInterface oPushNotify;
    OrgOfonoRadioSettingsInterface oRadio;
    OrgOfonoSimManagerInterface oSim;
    OrgOfonoSupplementaryServicesInterface oSuplServices;
    OrgOfonoVoiceCallManagerInterface oVoiceCallManager;

    void checkIface(QDBusAbstractInterface *iface);
    void showVariantMap(QVariantMap map, QString caption);
    void showVariantMapResult(QOFonoDBusPendingReply<QVariantMap> reply, QString caption);
    void showOFonoObject(QOFonoObject o, QString caption);
    void showOFonoObjectList(QOFonoObjectList list, QString caption);

    template <class T, class T2, class T3>
            bool checkReply2(QOFonoDBusPendingReply<T,T2,T3> & reply,
                             bool okBox,
                             bool waitForFinished,
                             QLabel * label = NULL);

    template <class T, class T2, class T3>
            void watchCallWatcher(QDBusPendingReply<T,T2,T3> & reply,
                                  QDBusPendingCallWatcher *watcher,
                                  const char * finishedMethod);

Q_SIGNALS:
    void finished(QDBusPendingReply<> *reply);

private slots:
    void on_bEnableUmts_clicked();
    void on_bDisableUmts_clicked();
    void on_bSimProperties_clicked();
    void on_bImport_clicked();
    void on_bSendMessage_clicked();
    void on_bGetMessages_clicked();
    void on_bHangup_clicked();
    void on_bGetCalls_clicked();
    void on_bAnswer_clicked();
    void on_lwCalls_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void on_bHangupAll_clicked();
    void on_bDial_clicked();
    void on_bScan_clicked();
    void on_bGetOperators_clicked();
    void on_bRegister_clicked();
    void on_bOnlineModem_clicked();
    void on_bPowerOnModem_clicked();
    void on_bRetrieveTextMessages_clicked();
    void on_bDeactivateContext_clicked();
    void on_tbActivateContext_clicked();
    void on_bSetCredentials_clicked();
    void on_bSimInfo_clicked();
    void on_bQueryMessages_clicked();
    void on_tbSmsContent_textChanged();
    void on_bGetStatus_clicked();
    void on_bGetFunctionality_clicked();
    void on_bSend_clicked();
    void on_bTransfer_clicked();
    void on_bReleaseAll_clicked();
    void on_bReleaseHeld_clicked();
    void on_bJoin_clicked();
    void on_bHoldActive_clicked();
    void on_bRelease_clicked();
    void on_bActivateConference_clicked();
    void on_bActivateCall_clicked();
    void on_bGsmFeatures_clicked();
    void on_bCall_clicked();
    void on_cbOrangeLed_stateChanged(int );
    void on_cbBlueLed_stateChanged(int );
    void on_cbRedLed_stateChanged(int );
    void refresh();
    void modemPropertyChanged(const QString &name, const QDBusVariant &value);
    void netRegPropertyChanged(const QString &name, const QDBusVariant &value);
    void voiceCallManagerPropertyChanged(const QString &name, const QDBusVariant &value);
    void voiceCallAdded(const QDBusObjectPath &path, const QVariantMap &properties);
    void voiceCallRemoved(const QDBusObjectPath &path);
    void gsmCallStatusChange(int id, const QString &status, const QVariantMap &properties);
    void gsmMessageSizeFinished(QOFonoDBusPendingCall & call);
    void incomingMessage(const QString & message, const QVariantMap & info);
    void immediateMessage(const QString & message, const QVariantMap & info);
};

template <class T, class T2, class T3>
        bool MainWindow::checkReply2(QOFonoDBusPendingReply<T, T2, T3> & reply,
                                     bool okBox,
                                     bool waitForFinished,
                                     QLabel * label)
{
    if(waitForFinished)
    {
        reply.waitForFinished();
    }

    //    qWarning() << "reply.isFinished()=" << reply.isFinished() <<
    //            ", reply.isValid()=" << reply.isValid() <<
    //            ", reply.isError()=" << reply.isError() <<
    //            ", fn=" << fn;

    if(reply.isError())
    {
        QString err = reply.error().message();
        qWarning() << "Error in " << reply.methodCall << err;
        if(label != NULL)
        {
            label->setText(reply.method + ": " + err);
        }
        else
        {
            QMessageBox::critical(this, reply.method, reply.method + " failed: " + err);
        }
        return false;
    }
    if(reply.isFinished() && reply.isValid())
    {
        if(okBox)
        {
            QMessageBox::information(this, reply.method, reply.method + " ok");
        }
        return true;
    }
    if(label != NULL)
    {
        label->setText(label->text() + ".");
    }
    return false;
}

template <class T, class T2, class T3>
        void MainWindow::watchCallWatcher(QDBusPendingReply<T,T2,T3> & reply,
                                          QDBusPendingCallWatcher *watcher,
                                          const char * finishedMethod)
{
    watcher->~QDBusPendingCallWatcher();
    watcher = new (watcher)QDBusPendingCallWatcher(reply, this);
    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                     this, finishedMethod);
}

#endif // MAINWINDOW_H
