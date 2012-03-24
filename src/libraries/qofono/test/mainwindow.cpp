#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ofonoutil.h"

MainWindow::MainWindow(QWidget *parent, Qt::WFlags) :
        QMainWindow(parent),
        ui(new Ui::MainWindow),
        oManager("org.ofono", "/", QDBusConnection::systemBus(), this),
        oModem("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oCellBroadcast("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oConnMan("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oMessageManager("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oNetReg("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oPhoneBook("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oPushNotify("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oRadio("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oSim("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oSuplServices("org.ofono", "/hso_0", QDBusConnection::systemBus(), this),
        oVoiceCallManager("org.ofono", "/hso_0", QDBusConnection::systemBus(), this)
{
    ui->setupUi(this);

    connect(&oModem, SIGNAL(PropertyChanged(const QString, const QDBusVariant)), this, SLOT(modemPropertyChanged(const QString, const QDBusVariant)));
    connect(&oNetReg, SIGNAL(PropertyChanged(const QString, const QDBusVariant)), this, SLOT(netRegPropertyChanged(const QString, const QDBusVariant)));
    connect(&oVoiceCallManager, SIGNAL(PropertyChanged(const QString, const QDBusVariant)), this, SLOT(voiceCallManagerPropertyChanged(const QString, const QDBusVariant)));
    connect(&oVoiceCallManager, SIGNAL(CallAdded(const QDBusObjectPath &, const QVariantMap &)), this, SLOT(voiceCallAdded(const QDBusObjectPath &, const QVariantMap &)));
    connect(&oVoiceCallManager, SIGNAL(CallRemoved(const QDBusObjectPath &)), this, SLOT(voiceCallRemoved(const QDBusObjectPath &)));
    connect(&oMessageManager, SIGNAL(IncomingMessage(const QString &, const QVariantMap &)), this, SLOT(incomingMessage(const QString &, const QVariantMap &)));
    connect(&oMessageManager, SIGNAL(ImmediateMessage(const QString &, const QVariantMap &)), this, SLOT(immediateMessage(const QString &, const QVariantMap &)));

    QTimer::singleShot(1000, this, SLOT(refresh()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void MainWindow::checkIface(QDBusAbstractInterface *iface)
{
    QString status = (iface->isValid() ? "OK" : "INVALID");
    ui->tbInit->append(status + " " + iface->service() + " " + iface->path());
}

QString variantMapToStr(QVariantMap map)
{
    QString str;
    for(int i = 0; i < map.count(); i++)
    {
        QString key = map.keys().at(i);
        str += key + ": " + map.value(key).toString() + "\n";
    }
    return str;
}

QString ofonoObjectToStr(QOFonoObject o)
{
    QString str;
    str.append(o.object.path());
    str.append("\n");
    str.append(variantMapToStr(o.properties));
    str.append("\n\n");
    return str;
}

QString ofonoObjectListToStr(QOFonoObjectList list)
{
    QString str;
    for(int i = 0; i < list.count(); i++)
    {
        str.append(ofonoObjectToStr(list.at(i)));
    }
    return str;
}

void MainWindow::showOFonoObject(QOFonoObject o, QString caption)
{
    QMessageBox::information(this, caption, ofonoObjectToStr(o));
}

void MainWindow::showOFonoObjectList(QOFonoObjectList list, QString caption)
{
    QMessageBox::information(this, caption, ofonoObjectListToStr(list));
}

void MainWindow::showVariantMap(QVariantMap map, QString caption)
{
    QMessageBox::information(this, caption, variantMapToStr(map));
}

void MainWindow::showVariantMapResult(QOFonoDBusPendingReply<QVariantMap> reply, QString caption)
{
    showVariantMap(reply.value(), caption);
}

void MainWindow::on_cbRedLed_stateChanged(int state)
{

}

void MainWindow::on_cbBlueLed_stateChanged(int state)
{

}

void MainWindow::on_cbOrangeLed_stateChanged(int state)
{

}

void MainWindow::refresh()
{
    // Init tab
    if(ui->tabInit->isVisible())
    {
        ui->tbInit->clear();
        checkIface(&oManager);
        checkIface(&oConnMan);
        checkIface(&oMessageManager);
        checkIface(&oNetReg);
        checkIface(&oPhoneBook);
        checkIface(&oPushNotify);
        checkIface(&oRadio);
        checkIface(&oSim);
        checkIface(&oSuplServices);
        checkIface(&oVoiceCallManager);
    }
    if(ui->tabManager->isVisible())
    {
        QOFonoDBusPendingReply<QOFonoObjectList> reply = oManager.GetModems();
        if(checkReply2(reply, false, true))
        {
            QOFonoObjectList modems = reply.value();
            ui->lModems->setText(ofonoObjectListToStr(modems));
        }
    }
    if(ui->tabNetwork->isVisible())
    {
        QOFonoDBusPendingReply<QVariantMap> reply = oNetReg.GetProperties();
        if(checkReply2(reply, false, true))
        {
            ui->lNetwork->setText(variantMapToStr(reply.value()));
        }
    }
    if(ui->tabVoiceCall->isVisible())
    {
        QListWidgetItem *item = ui->lwCalls->currentItem();
        if(item == NULL)
        {
            ui->lCallProperties->setText("");
        }
        else
        {
            OrgOfonoVoiceCallInterface oVoiceCall("org.ofono", item->text(), QDBusConnection::systemBus(), this);
            QOFonoDBusPendingReply<QVariantMap> reply = oVoiceCall.GetProperties();
            if(checkReply2(reply, false, true))
            {
                ui->lCallProperties->setText(variantMapToStr(reply.value()));
            }
        }
    }
    if(ui->tabRadio->isVisible())
    {
        QOFonoDBusPendingReply<QVariantMap> reply = oRadio.GetProperties();
        if(checkReply2(reply, false, true))
        {
            ui->lRadio->setText(variantMapToStr(reply.value()));
        }
    }

    QTimer::singleShot(1000, this, SLOT(refresh()));
}

void MainWindow::on_bCall_clicked()
{
}

void MainWindow::on_bGsmFeatures_clicked()
{
}

void MainWindow::gsmCallStatusChange(int id, const QString &status, const QVariantMap &properties)
{
}

void MainWindow::on_bActivateCall_clicked()
{
}

void MainWindow::on_bActivateConference_clicked()
{
}

void MainWindow::on_bRelease_clicked()
{
}

void MainWindow::on_bHoldActive_clicked()
{
}

void MainWindow::on_bJoin_clicked()
{
}

void MainWindow::on_bReleaseHeld_clicked()
{
}

void MainWindow::on_bReleaseAll_clicked()
{
}

void MainWindow::on_bTransfer_clicked()
{
}

void MainWindow::incomingMessage(const QString & message, const QVariantMap & info)
{
    QMessageBox::information(this, "Incomming message",
                             message + "\n\n" + variantMapToStr(info));
}

void MainWindow::immediateMessage(const QString & message, const QVariantMap & info)
{
    QMessageBox::information(this, "Immediate message",
                             message + "\n\n" + variantMapToStr(info));
}

void MainWindow::on_bSend_clicked()
{
}

void MainWindow::on_bGetFunctionality_clicked()
{
}

void MainWindow::on_bGetStatus_clicked()
{
}

void MainWindow::on_tbSmsContent_textChanged()
{
}

void MainWindow::gsmMessageSizeFinished(QOFonoDBusPendingCall & call)
{
}

void MainWindow::on_bQueryMessages_clicked()
{
}

void MainWindow::on_bSimInfo_clicked()
{
}

void MainWindow::on_bSetCredentials_clicked()
{
}

void MainWindow::on_tbActivateContext_clicked()
{
}

void MainWindow::on_bDeactivateContext_clicked()
{
}

void MainWindow::on_bRetrieveTextMessages_clicked()
{
}

void MainWindow::on_bPowerOnModem_clicked()
{
    QOFonoDBusPendingReply<> reply = oModem.SetProperty("Powered", QDBusVariant(true));
    checkReply2(reply, true, true);
}

void MainWindow::on_bOnlineModem_clicked()
{
    QOFonoDBusPendingReply<> reply = oModem.SetProperty("Online", QDBusVariant(true));
    checkReply2(reply, true, true);
}

void MainWindow::on_bRegister_clicked()
{
    QOFonoDBusPendingReply<> reply = oNetReg.Register();
    checkReply2(reply, true, true);
}

void MainWindow::on_bGetOperators_clicked()
{
    QOFonoDBusPendingReply<QOFonoObjectList> reply = oNetReg.GetOperators();
    if(checkReply2(reply, false, true))
    {
        showOFonoObjectList(reply.value(), "GetOperators");
    }
}

void MainWindow::on_bScan_clicked()
{
    QOFonoDBusPendingReply<QOFonoObjectList> reply = oNetReg.Scan();
    if(checkReply2(reply, false, true))
    {
        showVariantMapResult(reply, "Scan");
    }
}

void MainWindow::modemPropertyChanged(const QString &name, const QDBusVariant &value)
{
    ui->tbModemPropertyChanges->append(name + "->" + value.variant().toString());
}

void MainWindow::netRegPropertyChanged(const QString &name, const QDBusVariant &value)
{
    ui->tbNetRegPropertyChanges->append(name + "->" + value.variant().toString());
}

void MainWindow::voiceCallManagerPropertyChanged(const QString &name, const QDBusVariant &value)
{
}

void MainWindow::on_bDial_clicked()
{
    QOFonoDBusPendingReply<QDBusObjectPath> reply = oVoiceCallManager.Dial(ui->tbDialNum->text(), "");
    checkReply2(reply, true, true);
}

void MainWindow::on_bHangupAll_clicked()
{
    QOFonoDBusPendingReply<> reply = oVoiceCallManager.HangupAll();
    checkReply2(reply, true, true);
}

void MainWindow::voiceCallAdded(const QDBusObjectPath &path, const QVariantMap &properties)
{
    QListWidget *lw = ui->lwCalls;
    lw->addItem(path.path());
    lw->setCurrentRow(lw->count() - 1);
}

void MainWindow::voiceCallRemoved(const QDBusObjectPath &path)
{
    for(int i = 0; i < ui->lwCalls->count(); i++)
    {
        QListWidgetItem *item = ui->lwCalls->item(i);
        if(item->text() == path.path())
        {
            ui->lwCalls->takeItem(i);
            delete(item);
            return;
        }
    }
    qWarning() << "voiceCallRemoved: call not found " << path.path();
}

void MainWindow::on_lwCalls_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    refresh();
}

void MainWindow::on_bAnswer_clicked()
{
    QListWidgetItem *item = ui->lwCalls->currentItem();
    if(item == NULL)
    {
        return;
    }
    OrgOfonoVoiceCallInterface oVoiceCall("org.ofono", item->text(), QDBusConnection::systemBus(), this);
    QOFonoDBusPendingReply<> reply = oVoiceCall.Answer();
    checkReply2(reply, true, true);
}

void MainWindow::on_bHangup_clicked()
{
    QListWidgetItem *item = ui->lwCalls->currentItem();
    if(item == NULL)
    {
        return;
    }
    OrgOfonoVoiceCallInterface oVoiceCall("org.ofono", item->text(), QDBusConnection::systemBus(), this);
    QOFonoDBusPendingReply<> reply = oVoiceCall.Hangup();
    checkReply2(reply, true, true);
}

void MainWindow::on_bGetCalls_clicked()
{
    QOFonoDBusPendingReply<QOFonoObjectList> reply = oVoiceCallManager.GetCalls();
    if(checkReply2(reply, true, true))
    {
        QListWidget *lw = ui->lwCalls;
        QOFonoObjectList list = reply.value();
        lw->clear();
        for(int i = 0; i < list.count(); i++)
        {
            lw->addItem(list.at(i).object.path());
        }
    }
}

void MainWindow::on_bGetMessages_clicked()
{
    QOFonoDBusPendingReply<QOFonoObjectList> reply = oMessageManager.GetMessages();
    if(checkReply2(reply, false, true))
    {
        showOFonoObjectList(reply.value(), "GetMessages");
    }
}

void MainWindow::on_bSendMessage_clicked()
{
    QOFonoDBusPendingReply<QDBusObjectPath> reply = oMessageManager.SendMessage(ui->tbSmsPhoneNumber->text(), ui->tbSmsContent->toPlainText());
    if(checkReply2(reply, false, true))
    {
        qDebug() << reply.value().path();
        QMessageBox::information(this, "SendMessage", reply.value().path());
    }
}

void MainWindow::on_bImport_clicked()
{
    QOFonoDBusPendingReply<QString> reply = oPhoneBook.Import();
    if(checkReply2(reply, false, true))
    {
        QString str = reply.value();
        qDebug() << str;
        QMessageBox::information(this, "Phonebook import", str);

        int index = 0;
        while((index = str.indexOf("\nFN:", index)) > 0)
        {
            index += 4;
            int end = str.indexOf("\n", index) - 1;
            if(end < 0)
            {
                break;
            }
            QString name = str.mid(index, end - index);
            index = str.indexOf("TEL;TYPE=VOICE:+", index);
            if(index < 0)
            {
                break;
            }
            index += 15;
            end = str.indexOf("\n", index) - 1;
            if(end < 0)
            {
                break;
            }
            QString number = str.mid(index, end - index);
            qDebug() << "name=" << name << ", number=" << number;
        }
    }
}

void MainWindow::on_bSimProperties_clicked()
{
    QOFonoDBusPendingReply<QVariantMap> reply = oSim.GetProperties();
    if(checkReply2(reply, false, true))
    {
        QVariantMap map = reply.value();
        qDebug() << variantMapToStr(map);
        showVariantMap(map, "Sim properties");
        QStringList numbers = map.value("SubscriberNumbers").toStringList();
        QString str;
        for(int i = 0; i < numbers.count(); i++)
        {
            str.append(numbers.at(i) + "\n");
        }
        qDebug() << str;
        QMessageBox::information(this, "SubscriberNumbers", str);
    }
}

void MainWindow::on_bDisableUmts_clicked()
{
    QOFonoDBusPendingReply<> reply = oRadio.SetProperty("TechnologyPreference", QDBusVariant("gsm"));
    checkReply2(reply, true, true);
}

void MainWindow::on_bEnableUmts_clicked()
{
    QOFonoDBusPendingReply<> reply = oRadio.SetProperty("TechnologyPreference", QDBusVariant("umts"));
    checkReply2(reply, true, true);
}
