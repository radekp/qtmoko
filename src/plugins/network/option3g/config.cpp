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

#include <proxiesconfig.h>
#include <accountconfig.h>
#include "config.h"
#include "dialing.h"
#include "dialstring.h"

#include <QDebug>
#include <QMap>
#include <QLayout>
#include <QListWidget>
#include <QProcess>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>

#include <qtopialog.h>
#include <qtopianamespace.h>
#include <qsoftmenubar.h>

#include <sys/types.h>
#include <unistd.h>
#include <time.h>

class DialupUI : public QDialog
{
    Q_OBJECT
public:
    DialupUI( DialupConfig *c, QWidget* parent, Qt::WFlags flags = 0);
    ~DialupUI();

    enum Entry {
        Account, Dialing, Proxy
    };

public slots:
    void accept();
private slots:
    void optionSelected(QListWidgetItem* item);
    void updateUserHint(QListWidgetItem* cur, QListWidgetItem* prev);

private:
    void init();
    void createPeerId();
    int writeSystemFiles();


    DialupConfig* config;
    ProxiesPage* proxiesPage;
    AccountPage* accPage;
    DialingPage* dialingPage;
    QListWidget* options;
    QStackedWidget* stack;
    QLabel* userHint;
    QString errorText;
};

DialupConfig::DialupConfig( const QString& confFile )
    : currentConfig( confFile )
    , cfg( confFile, QSettings::IniFormat)
#ifndef QTOPIA_NO_FSO
    , gsmPdp("org.freesmartphone.ogsmd", "/org/freesmartphone/GSM/Device", QDBusConnection::systemBus())
#endif    
{
}

DialupConfig::~DialupConfig()
{
}

QVariant DialupConfig::property( const QString& key ) const
{
    cfg.sync();
    QVariant result;
    result = cfg.value(key);

    return result;
}

QStringList DialupConfig::types() const
{
    QStringList uis;
    uis << QObject::tr("Properties");
    //for cost monitoring (future feature)
    //uis << QObject::tr("Cost monitor","shows costs of online sessions") )
    return uis;
}


QDialog* DialupConfig::configure( QWidget* parent, const QString& type)
{
    if ( type.isEmpty() || type == QObject::tr("Properties") )
        return new DialupUI( this, parent );
    else if ( type == QObject::tr("Cost monitor","shows costs of online sessions") )
        return 0; //future feature
    return 0;
}

QtopiaNetworkProperties DialupConfig::getProperties() const
{
    QtopiaNetworkProperties prop;
    cfg.sync();

    QStringList allKeys = cfg.allKeys();
    foreach (QString key, allKeys) {
        QVariant v = cfg.value( key );
        if ( v.isValid() )
            prop.insert(key, v);
    }

    return prop;
}

void DialupConfig::writeProperties( const QtopiaNetworkProperties& properties )
{
    cfg.beginGroup("Properties"); //by default write to Properties
    QMapIterator<QString,QVariant> i(properties);
    QString key;
    QString group;
    QString value;
    while ( i.hasNext() ) {
        i.next();
        key = i.key();
        int groupIndex = key.indexOf('/');
        if ( groupIndex >= 0) {
            group = key.left( groupIndex );
            value = key.mid( groupIndex + 1 );
            cfg.endGroup();
            cfg.beginGroup( group );
            cfg.setValue( value, i.value() );
            cfg.endGroup();
            cfg.beginGroup("Properties");
        } else {
            cfg.setValue( key, i.value() );
        }

        group = value = QString();
    }
    cfg.endGroup();
    cfg.sync();
    
#ifndef QTOPIA_NO_FSO
    QString apn = properties["Serial/APN"].toString();
    if(!apn.isEmpty() && !strcmp("Fso", getenv( "QTOPIA_PHONE" ))) {
        QString username = properties["Properties/Password"].toString();
        QString password = properties["Properties/UserName"].toString();
        QFsoDBusPendingReply<> reply = gsmPdp.SetCredentials(apn, username,
                                                             password);
        checkReply(reply);
    }
#endif
}

// DialupUI implementation

DialupUI::DialupUI(DialupConfig *c, QWidget* parent, Qt::WFlags flags)
    :QDialog(parent, flags), config( c )
{
    init();
    QSoftMenuBar::menuFor( this );
    QSoftMenuBar::setHelpEnabled( this , true );
    setObjectName("dialup-menu");
}

DialupUI::~DialupUI()
{
}

void DialupUI::init()
{
    QVBoxLayout* vBox = new QVBoxLayout( this );
    vBox->setMargin( 0 );
    vBox->setSpacing( 0 );

    QtopiaNetworkProperties knownProp = config->getProperties();
    QString title = knownProp.value("Info/Name").toString();
    if (!title.isEmpty())
        setWindowTitle( title );

    QtopiaNetwork::Type type = QtopiaNetwork::toType( config->configFile() );
    stack = new QStackedWidget( this );

    QWidget* page = new QWidget();
    QVBoxLayout *vb = new QVBoxLayout(page);
    options = new QListWidget( page );
    options->setSpacing( 1 );
    options->setAlternatingRowColors( true );
    options->setSelectionBehavior( QAbstractItemView::SelectRows );

    QListWidgetItem* item = new QListWidgetItem( tr("Account"), options, Account );
    item->setTextAlignment( Qt::AlignHCenter);
    item->setIcon( QIcon(":icon/netsetup/account") );

    item = new QListWidgetItem( tr("Proxy Settings"), options, Proxy );
    item->setTextAlignment( Qt::AlignHCenter);
    item->setIcon(QIcon(":icon/netsetup/proxies"));

    item = new QListWidgetItem( tr("Network"), options, Dialing );
    item->setTextAlignment( Qt::AlignHCenter);
    item->setIcon( QIcon(":icon/netsetup/server") );

    vb->addWidget( options );

    QHBoxLayout* hBox = new QHBoxLayout();

    userHint = new QLabel( page );
    userHint->setMargin( 2 );
    userHint->setWordWrap( true );
    hBox->addWidget( userHint );

    QSpacerItem* spacer = new QSpacerItem( 1, 60, QSizePolicy::Minimum,
           QSizePolicy::Expanding);
    hBox->addItem( spacer );
    vb->addLayout( hBox );

    connect( options, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            this, SLOT(updateUserHint(QListWidgetItem*,QListWidgetItem*)));
    options->setCurrentRow( 0 );
    stack->addWidget( page );

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable( true );
    scroll->setFocusPolicy( Qt::NoFocus );
    proxiesPage = new ProxiesPage( knownProp );
    scroll->setWidget( proxiesPage );
    stack->addWidget( scroll );

    scroll = new QScrollArea();
    scroll->setWidgetResizable( true );
    scroll->setFocusPolicy( Qt::NoFocus );
    accPage = new AccountPage( type,
            knownProp );
    scroll->setWidget( accPage );
    stack->addWidget( scroll );

    scroll = new QScrollArea();
    scroll->setWidgetResizable( true );
    scroll->setFocusPolicy( Qt::NoFocus );
    dialingPage = new DialingPage( knownProp );
    scroll->setWidget( dialingPage );
    stack->addWidget( scroll );

    stack->setCurrentIndex( 0 );

    vBox->addWidget( stack );
    connect(options, SIGNAL(itemActivated(QListWidgetItem*)),
            this, SLOT(optionSelected(QListWidgetItem*)));
}

void DialupUI::accept()
{
    if (stack->currentIndex() == 0) {
        QtopiaNetworkProperties props = proxiesPage->properties();
        config->writeProperties(props);
        props = accPage->properties();
        config->writeProperties(props);
        props = dialingPage->properties();
        config->writeProperties(props);

        createPeerId();
        int ret = writeSystemFiles();
        if ( ret == 0 )
            QDialog::accept();
        else {
            QMessageBox::warning(this, tr("Error"), "<qt>"+errorText+"</qt>",
                    QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton);
            if ( ret > 0 )
                stack->setCurrentIndex( ret );
            else
                QDialog::accept();
        }
    } else {
        stack->setCurrentIndex( 0 );
    }
}

void DialupUI::createPeerId( )
{
    //create peer id if it doesn't already exist
    QString peerId = config->property( "Serial/PeerID" ).toString();
    if (peerId.isEmpty()) {
        //create a uniquish id
        peerId = config->property("Info/Type").toString();
        peerId.replace( QRegExp("[^A-Za-z]"), "" );
        peerId += QString::number(time(0)^(getpid()<<3));
        qLog(Network) << "Creating new peer ID: " << peerId;
        QtopiaNetworkProperties p;
        p.insert("Serial/PeerID", peerId);
        config->writeProperties( p );
    }
}

/*
  Return values:
    -1 -> general error not connected to specific configuration
    0  -> no error
    >0 -> returned number gives indication of widget that could fix the problem
  */
int DialupUI::writeSystemFiles()
{
    qLog(Network) << "Writing system files";
    const QtopiaNetworkProperties prop = config->getProperties();

    // this map is eventually passed to QtopiaNetworkProperties
    QStringList peerFile;

    //select content for ppp peers file
#ifdef QTOPIA_CELL
    const bool external = prop.value("Serial/Type").toString() == "external";
#else
    const bool external = true; // doesn't have internal modem
#endif

    if ( !external )
        peerFile << prop.value("Serial/Speed").toString();

    const QString peerName = prop.value("Serial/PeerID").toString();
    const bool demandDial = prop.value("Properties/Autostart").toString() == "y";
    const bool gprs = prop.value("Serial/GPRS").toString() == "y";

    // write chat scripts
    QString st;
    st = "\"\" \"\\K\\K\\K\\d+++ATH\"\n" // No tr
        "ABORT \"NO CARRIER\"\n" // No tr
        "ABORT \"NO DIALTONE\"\n" // No tr
        "ABORT \"BUSY\"\n" // No tr
        "TIMEOUT 30 \n" // No tr
        "OK "; // No tr

    QString dial;
    if ( gprs ) {
        dial = GPRSDialString();
        dial = dial.arg(prop.value("Serial/APN").toString());
        st += dial;
    }

    st.replace( QString("OK"), QString("\nOK") );

    const QString path = Qtopia::applicationFileName("Network", "chat");
    QDir chatDir( path );
    chatDir.mkdir( path );
    QString connectF = path+"/connect-"+peerName;
    QString disconnectF = path+"/disconnect-"+peerName;

    if ( external ) {
        QString connect = QString("connect '/usr/sbin/chat -s -v -f %1'");
        connect = connect.arg(connectF);
        QString disconnect = QString("disconnect '/usr/sbin/chat -s -v -f %1'");
        disconnect = disconnect.arg(disconnectF);
        peerFile << connect;
        peerFile << disconnect;
    }

    //write chat files
    QFile fc( connectF );
    if ( fc.open(QFile::WriteOnly | QFile::Truncate) ) {
        fc.write( st.toUtf8() );
        fc.close();
    } else {
        errorText = tr("Cannot write chat file");
        qLog(Network) << "Cannot write chat file" << connectF;
        return -1;
    }

    QFile fd( disconnectF );
    if ( fd.open(QFile::WriteOnly | QFile::Truncate) ) {
        if ( gprs )
            //fd.write( GPRSDisconnectString().toUtf8() );
            fd.write( "\"\"" );
        else
            fd.write( QString("\"\" \\d+++\\d\\c OK\nATH0 OK").toUtf8() );
        fd.close();
    } else {
        qLog(Network) << "Cannot write disconnect file" << disconnectF;
        errorText = tr("Cannot write disconnect file");
        //delete connect chat file
        fc.remove();
        return -1;
    }

    // GPRS modems require the the following pppd options
    if ( gprs ){
        // Most GRPS modems do not respond to LCP echo's
        peerFile << "lcp-echo-failure 0";   // No tr
        peerFile << "lcp-echo-interval 0"; // No tr
        // Compression does not make sense on a GPRS link
        peerFile << "novj"; // No tr
        peerFile <<"nobsdcomp"; // No tr
        peerFile <<"novjccomp"; // No tr
        peerFile <<"nopcomp"; // No tr
        peerFile <<"noaccomp"; // No tr
    }

    if ( prop.value("Serial/Crtscts").toString() == "n" )
        peerFile << "nocrtscts";
    else
        peerFile << "crtscts";

    // Accept peers idea of our local address
    peerFile << "ipcp-accept-local";
    peerFile << "noipdefault";
    // Use modem control lines, otherwise change the following "modem" to "local"
    peerFile << "modem";

    const QString user = prop.value("Properties/UserName").toString();
    if ( !user.isEmpty() )
        peerFile << ("user " + Qtopia::stringQuote(user));

    if ( demandDial ) {
        peerFile << "demand";
    }

    const int idleTime = prop.value("Serial/Timeout").toInt();
    if ( idleTime )
        peerFile << "idle " + QString::number(idleTime) ;

    if ( prop.value("Serial/UsePeerDNS").toString() != "n" )
        peerFile << "usepeerdns";
    if ( prop.value("Serial/DefaultRoute").toString() != "n" )
        peerFile << "defaultroute";

    const int delay = prop.value("Serial/ConnectDelay").toInt();
    if ( delay )
        peerFile << ("connect-delay " + QString::number(delay));

    peerFile << ("remotename " + peerName);

    //install peer file
    const QString peerFileName = Qtopia::tempDir() + peerName;
    QFile tmpPeer( peerFileName );
    if ( tmpPeer.open(QFile::WriteOnly | QFile::Truncate) ) {
        QTextStream s(&tmpPeer);
        for (int i = 0; i<peerFile.count(); i++)
            s << peerFile[i] << endl;
        tmpPeer.close();
    } else {
        qLog(Network) << "Cannot write peer file" << peerFileName;
        errorText = tr("Cannot write peer file");
        fc.remove(); //delete connect chat
        fd.remove(); //delete disconnect chat
        return -1; //cannot write peer file
    }

    QStringList params;
    params << "install";
    params << "peer";
    params << peerFileName;

    // write peer file
    qLog(Network) << "Saving peers file " << peerName;
    //installing the peer shouldn't take to long -> hence we can block on its execution
    QProcess::execute(Qtopia::qtopiaDir()+"bin/ppp-network", params);
    QFile::remove( peerFileName );
    //TODO write password files (pap, chap)

    return 0;
}

void DialupUI::optionSelected(QListWidgetItem* item)
{
    if (item) {
        switch( item->type() )
        {
            case Account:
                stack->setCurrentIndex( 2 );
                break;
            case Dialing:
                stack->setCurrentIndex( 3 );
                break;
            case Proxy:
                stack->setCurrentIndex( 1 );
                break;
            default:
                break;
        }
    }
}

void DialupUI::updateUserHint(QListWidgetItem* cur, QListWidgetItem* /*prev*/)
{
    if (!cur)
        return;

    QString desc;
    switch( cur->type() ) {
        case Account:
            desc = tr("General account information.");
            break;
        case Dialing:
            desc = tr("General dial-up parameter.");
            break;
        case Proxy:
            desc = tr("Proxy details used for HTTP and FTP data.");
            break;
        default:
            break;
    }
    userHint->setText( desc );
}

#include "config.moc"
