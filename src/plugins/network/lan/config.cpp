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

#include <ipconfig.h>
#include <proxiesconfig.h>
#include <accountconfig.h>
#include <custom.h>

#include "config.h"
#include "wirelessconfig.h"
#include "wirelessipconfig.h"
#include "encryptionconfig.h"
#include "wirelessscan.h"
#include "roamingconfig.h"

#include <QDebug>
#include <QMap>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>

#include <qtopialog.h>
#include <qvaluespace.h>
#include <qsoftmenubar.h>


class LanUI : public QDialog
{
    Q_OBJECT
public:
    LanUI( LANConfig *c, QWidget* parent, Qt::WFlags flags = 0);
    ~LanUI();

    enum Entry {
        Account,
        IP,
        Proxy
#ifndef NO_WIRELESS_LAN
        , Wireless
        , WirelessEncryption
#if WIRELESS_EXT >13
        , WirelessRoaming
#endif
#endif
    };

public slots:
    void accept();
private slots:
    void optionSelected(QListWidgetItem* item);
    void updateUserHint(QListWidgetItem* cur, QListWidgetItem* prev);

private:
    void init();
    void markConfig();

    QtopiaNetwork::Type type;
    LANConfig* config;
    IPPage *ipPage;
    ProxiesPage* proxiesPage;
    AccountPage* accPage;
#ifndef NO_WIRELESS_LAN
    WirelessPage* wirelessPage;
    WirelessIPPage* wirelessIpPage;
    WirelessEncryptionPage* encryptPage;
    QtopiaNetworkProperties netSettings;
    int lastIndex;
#if WIRELESS_EXT > 13
    RoamingPage* wirelessRoaming;
#endif
#endif //NO_WIRELESS_LAN

    QListWidget* options;
    QStackedWidget* stack;
    QLabel* userHint;
};


#ifndef NO_WIRELESS_LAN
class WLANScanUI : public QDialog
{
    Q_OBJECT
public:
    WLANScanUI( LANConfig *c, QWidget* parent, Qt::WFlags flags = 0);
    ~WLANScanUI();

public slots:
    void accept();
private:
    LANConfig* config;
    WSearchPage* searchPage;
};
#endif

// LANConfig implementation

LANConfig::LANConfig( const QString& confFile )
    : currentConfig( confFile ), cfg( confFile, QSettings::IniFormat)
{
}

LANConfig::~LANConfig()
{
}

QVariant LANConfig::property( const QString& key ) const
{
    cfg.sync();
    QVariant result;
    result = cfg.value(key);

    return result;
}

QStringList LANConfig::types() const
{
    QStringList ui;
    ui << QObject::tr("Properties");

#ifndef NO_WIRELESS_LAN
#if WIRELESS_EXT > 13
    QString type = property("Info/Type").toString();
    if ( type == "wlan" || type == "pcmciawlan" )
        ui << QObject::tr("WLAN detection") + " (default)";
#endif
#endif

    return ui;
}

QDialog* LANConfig::configure( QWidget* parent, const QString& type )
{
    if ( type.isEmpty()  || type == QObject::tr("Properties") )
        return new LanUI( this, parent );
#ifndef NO_WIRELESS_LAN
#if WIRELESS_EXT > 13
    else if ( type.startsWith(QObject::tr("WLAN detection")) )
    {
        QString type = property("Info/Type").toString();
        if ( type == "wlan" || type == "pcmciawlan" )
            return new WLANScanUI( this, parent );
    }
#endif
#endif
    return 0;
}

QtopiaNetworkProperties LANConfig::getProperties() const
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

void LANConfig::writeProperties( const QtopiaNetworkProperties& properties )
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
}

// LanUI implementation

LanUI::LanUI(LANConfig *c, QWidget* parent, Qt::WFlags flags)
    :QDialog(parent, flags), config( c )
{
    type = QtopiaNetwork::toType( config->configFile() );
    init();

    QSoftMenuBar::menuFor( this );
    QSoftMenuBar::setHelpEnabled( this, true );
}

LanUI::~LanUI()
{
}

void LanUI::init()
{
    QVBoxLayout* vBox = new QVBoxLayout( this );
    vBox->setMargin( 0 );
    vBox->setSpacing( 0 );

    QtopiaNetworkProperties knownProp = config->getProperties();

    QString title = knownProp.value("Info/Name").toString();
    if (!title.isEmpty())
        setWindowTitle( title );

    stack = new QStackedWidget( this );

    QWidget* page = new QWidget();
    QVBoxLayout *vb = new QVBoxLayout(page);
    vBox->setMargin( 0 );
    vBox->setSpacing( 0 );
    options = new QListWidget( page );
    options->setSpacing( 1 );
    options->setAlternatingRowColors( true );
    options->setSelectionBehavior( QAbstractItemView::SelectRows );

    QListWidgetItem* item = new QListWidgetItem(tr("Account"), options, Account );
    item->setTextAlignment( Qt::AlignHCenter);
    item->setIcon( QIcon(":icon/netsetup/account") );

#ifndef NO_WIRELESS_LAN
    if ( type & QtopiaNetwork::WirelessLAN) {
        item = new QListWidgetItem( tr("Wireless Networks"), options, Wireless );
        item->setTextAlignment( Qt::AlignHCenter);
        item->setIcon( QIcon(":icon/Network/lan/WLAN-online") );
    }
#endif

    item = new QListWidgetItem( tr("IP Settings", "short for ip address"), options,  IP );
    item->setTextAlignment( Qt::AlignHCenter);
    item->setIcon( QIcon(":icon/netsetup/server") );

    item = new QListWidgetItem( tr("Proxy Settings"), options, Proxy );
    item->setTextAlignment( Qt::AlignHCenter);
    item->setIcon(QIcon(":icon/netsetup/proxies"));

#ifndef NO_WIRELESS_LAN
    if ( type & QtopiaNetwork::WirelessLAN) {
        item = new QListWidgetItem( tr("Wireless Encryption"), options, WirelessEncryption );
        item->setTextAlignment( Qt::AlignHCenter);
        item->setIcon( QIcon(":icon/Network/lan/WLAN-online") );
#if WIRELESS_EXT > 13
        item = new QListWidgetItem( tr("Wireless Roaming"), options, WirelessRoaming );
        item->setTextAlignment( Qt::AlignHCenter);
        item->setIcon( QIcon(":icon/Network/lan/WLAN-online") );
#endif
    }
#endif

    vb->addWidget( options );

    QHBoxLayout* hBox = new QHBoxLayout();
    userHint = new QLabel( page);
    userHint->setWordWrap( true );
    userHint->setMargin( 2 );
    hBox->addWidget( userHint );

    QSpacerItem* spacer = new QSpacerItem( 1, 60,
            QSizePolicy::Minimum, QSizePolicy::Expanding );
    hBox->addItem( spacer );

    vb->addLayout( hBox );

    connect( options, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            this, SLOT(updateUserHint(QListWidgetItem*,QListWidgetItem*)));
    options->setCurrentRow( 0 );

    stack->addWidget( page );

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable( true );
    scroll->setFocusPolicy( Qt::NoFocus );
    if ( type & QtopiaNetwork::WirelessLAN) {
#ifndef NO_WIRELESS_LAN
        wirelessIpPage = new WirelessIPPage( knownProp );
        scroll->setWidget( wirelessIpPage );
        ipPage = 0;
#endif
    } else {
        ipPage = new IPPage( knownProp );
        scroll->setWidget( ipPage );
    }
    stack->addWidget( scroll );

    scroll = new QScrollArea();
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

#ifndef NO_WIRELESS_LAN
    if ( type & QtopiaNetwork::WirelessLAN) {
        scroll = new QScrollArea();
        scroll->setWidgetResizable( true );
        scroll->setFocusPolicy( Qt::NoFocus );
        wirelessPage = new WirelessPage( knownProp );
        scroll->setWidget( wirelessPage );
        stack->addWidget( scroll );

        scroll = new QScrollArea();
        scroll->setWidgetResizable( true );
        scroll->setFocusPolicy( Qt::NoFocus );
        encryptPage = new WirelessEncryptionPage( knownProp );
        scroll->setWidget( encryptPage );
        stack->addWidget( scroll );

#if WIRELESS_EXT > 13
        scroll = new QScrollArea();
        //scroll->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
        scroll->setWidgetResizable( true );
        scroll->setFocusPolicy( Qt::NoFocus );
        wirelessRoaming = new RoamingPage( knownProp );
        scroll->setWidget( wirelessRoaming );
        stack->addWidget( scroll );
#endif

        netSettings = encryptPage->properties(); //filter all non WirelessNetworks keys out
        lastIndex = 0;
   }
#endif

    stack->setCurrentIndex( 0 );
    setObjectName("lan-menu");
    vBox->addWidget( stack );
    connect(options, SIGNAL(itemActivated(QListWidgetItem*)),
            this, SLOT(optionSelected(QListWidgetItem*)));
}

void LanUI::accept()
{
    if (stack->currentIndex() == 0) {
        QtopiaNetworkProperties props;
        if ( !(type & QtopiaNetwork::WirelessLAN))
            props = ipPage->properties();
        config->writeProperties(props);
        props = proxiesPage->properties();
        config->writeProperties(props);
        props = accPage->properties();
        config->writeProperties(props);
#ifndef NO_WIRELESS_LAN
        if ( type & QtopiaNetwork::WirelessLAN) {
            // the wireless pages share their settings
            config->writeProperties(netSettings);
        }
#endif
        markConfig();
        QDialog::accept();
    } else {
#ifndef NO_WIRELESS_LAN
        if ( lastIndex == 1 && (type & QtopiaNetwork::WirelessLAN ))
            netSettings = wirelessIpPage->properties();
        else if ( lastIndex == 4 )
            netSettings = wirelessPage->properties();
        else if ( lastIndex == 5 )
            netSettings = encryptPage->properties();
#if WIRELESS_EXT > 13
        else if ( lastIndex == 6 )
            netSettings = wirelessRoaming->properties();
#endif
        lastIndex = 0;
#endif
        stack->setCurrentIndex( 0 );
        setObjectName("lan-menu");
    }
}

void LanUI::optionSelected(QListWidgetItem* item)
{
    if (item) {
        switch( item->type() )
        {
            case Account:
                stack->setCurrentIndex( 3 );
                break;
            case IP:
#ifndef NO_WIRELESS_LAN
                if ( type & QtopiaNetwork::WirelessLAN) {
                    wirelessIpPage->setProperties( netSettings );
                    setObjectName("tcpip");
                }
#endif
                stack->setCurrentIndex(1);
                break;
            case Proxy:
                stack->setCurrentIndex( 2 );
                break;
#ifndef NO_WIRELESS_LAN
            case Wireless:
                if ( type & QtopiaNetwork::WirelessLAN) {
                    wirelessPage->setProperties( netSettings );
                    stack->setCurrentIndex( 4 );
                    setObjectName("wireless");
                }
                break;
            case WirelessEncryption:
                if ( type & QtopiaNetwork::WirelessLAN) {
                    encryptPage->setProperties( netSettings );
                    stack->setCurrentIndex( 5 );
                    setObjectName("wireless-encryption");
                }
                break;
#if WIRELESS_EXT > 13
            case WirelessRoaming:
                if ( type & QtopiaNetwork::WirelessLAN) {
                    wirelessRoaming->setProperties( netSettings );
                    stack->setCurrentIndex( 6 );
                    setObjectName("wireless-roaming");
                }
#endif
#endif
            default:
                break;
        }
#ifndef NO_WIRELESS_LAN
        lastIndex = stack->currentIndex();
#endif
    }
}

void LanUI::updateUserHint(QListWidgetItem* cur, QListWidgetItem* /*prev*/)
{
    if (!cur)
        return;

    QString desc;
    switch (cur->type()) {
        case Account:
            desc = tr("General account information.");
            break;
        case IP:
            desc = tr("IP settings such as DNS, "
                    "gateway, broadcast and subnet details.");
            break;
        case Proxy:
            desc = tr("Proxy details used for HTTP and FTP data.");
            break;
#ifndef NO_WIRELESS_LAN
        case Wireless:
            if ( type & QtopiaNetwork::WirelessLAN)
                desc = tr("Wireless LAN access point parameter");
            break;
        case WirelessEncryption:
            if ( type & QtopiaNetwork::WirelessLAN)
                desc = tr("Wireless LAN encryption details");
            break;
#if WIRELESS_EXT > 13
        case WirelessRoaming:
            if ( type & QtopiaNetwork::WirelessLAN)
                desc = tr("Wireless LAN reconnection/roaming parameter");
            break;
#endif
#endif
        default:
            break;
    }
    userHint->setText( desc );
}

/*
   We need the interface name in order to write all system dependent files.
   However the name of the interface is not known until LanImpl::isAvailable()
   has been called which will not happen until we have created this config.

   The config files will be written by LanImpl::start(). We have to set
   a marker as notification of the missing config files.
*/
void LanUI::markConfig()
{
    QtopiaNetworkProperties p;
    p.insert("Info/WriteToSystem", true);
    config->writeProperties( p );
}

#ifndef NO_WIRELESS_LAN

WLANScanUI::WLANScanUI( LANConfig *c, QWidget* parent, Qt::WFlags flags )
    : QDialog( parent, flags ), config( c )
{
    QVBoxLayout* vbox = new QVBoxLayout( this );
    vbox->setMargin( 2 );
    vbox->setSpacing( 0 );

    searchPage = new WSearchPage( c->configFile() );
    vbox->addWidget( searchPage );
    setFocusPolicy( Qt::NoFocus );

    searchPage->setEditFocus( true );
}

WLANScanUI::~WLANScanUI()
{
}

void WLANScanUI::accept()
{
    searchPage->saveScanResults();
    QDialog::accept();
}

#endif

#include "config.moc"
