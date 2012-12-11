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

#include "detailspage_p.h"
#include "addressselectorwidget_p.h"
#include <private/accountconfiguration_p.h>

#include <qsoftmenubar.h>
#include <qtopiaapplication.h>
#include <qcontact.h>
#include <qcontactview.h>

#include <qcheckbox.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qcombobox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpixmap.h>
#include <qtoolbutton.h>
#include <qmenu.h>
#include <qdesktopwidget.h>
#include <qevent.h>
#include <qpushbutton.h>
#include <QScrollArea>
#include <QTimer>
#include <QMailStore>
#include <QMailAccount>


static const QString placeholder(QtopiaApplication::translate("DetailsPage", "(no subject)"));

static const int MaximumSmsSubjectLength = 40;
static const int MaximumInstantSubjectLength = 256;


class DetailsLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    DetailsLineEdit(QWidget *parent = 0);

signals:
    void send();
    void done();

protected:
    void focusInEvent(QFocusEvent *);
    void keyPressEvent(QKeyEvent *);
    void inputMethodEvent(QInputMethodEvent *e);

protected slots:
    virtual void updateMenuBar(const QString &text);
};

DetailsLineEdit::DetailsLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    connect(this, SIGNAL(textChanged(QString)),
            this, SLOT(updateMenuBar(QString)));
}

void DetailsLineEdit::focusInEvent(QFocusEvent *e)
{
    QLineEdit::focusInEvent(e);
    setEditFocus(true);
    updateMenuBar(text());
}

void DetailsLineEdit::keyPressEvent(QKeyEvent *e)
{
    static const bool keypadAbsent(style()->inherits("QThumbStyle"));

    if (keypadAbsent) {
        if (e->key() == Qt::Key_Back) {
            if (text().isEmpty())
                emit done();
            else
                emit send();
        } else {
            QLineEdit::keyPressEvent(e);
        }
    } else {
        if (e->key() == Qt::Key_Select)
            emit send();
        else if (e->key() == Qt::Key_Back && text().isEmpty())
            emit done();
        else
            QLineEdit::keyPressEvent(e);
    }
}

void DetailsLineEdit::inputMethodEvent(QInputMethodEvent *e)
{
    QLineEdit::inputMethodEvent(e);
    updateMenuBar(text() + e->preeditString());
}

void DetailsLineEdit::updateMenuBar(const QString &text)
{
    static const bool keypadAbsent(style()->inherits("QThumbStyle"));

    if (keypadAbsent) {
        if (text.isEmpty())
            QSoftMenuBar::setLabel(this, Qt::Key_Back, QSoftMenuBar::Cancel, QSoftMenuBar::EditFocus);
        else
            QSoftMenuBar::setLabel(this, Qt::Key_Back, ":icon/qtmail/enqueue", tr("Send"));
    } else {
        QSoftMenuBar::setLabel(this, Qt::Key_Select, ":icon/qtmail/enqueue", tr("Send"));

        if (text.isEmpty())
            QSoftMenuBar::setLabel(this, Qt::Key_Back, QSoftMenuBar::RevertEdit, QSoftMenuBar::EditFocus);
        else
            QSoftMenuBar::clearLabel(this, Qt::Key_Back);
    }
}

//===========================================================================

class RecipientEdit : public DetailsLineEdit
{
    Q_OBJECT
public:
    RecipientEdit(QWidget *parent = 0);
    ~RecipientEdit();

    void setPhoneNumbersAllowed(bool allow) {
        m_allowPhoneNumbers = allow;
    }
    void setEmailAllowed(bool allow) {
        m_allowEmails = allow;
    }
    void setMultipleAllowed(bool allow) {
        m_allowMultiple = allow;
    }

public slots:
    void editRecipients();

protected:
    virtual void keyPressEvent(QKeyEvent *);

protected slots:
    virtual void updateMenuBar(const QString &text);

private:
    bool m_allowPhoneNumbers;
    bool m_allowEmails;
    bool m_allowMultiple;
};

RecipientEdit::RecipientEdit(QWidget *parent)
    : DetailsLineEdit(parent), m_allowPhoneNumbers(false),
      m_allowEmails(false), m_allowMultiple(false)
{
}

RecipientEdit::~RecipientEdit()
{
}

void RecipientEdit::editRecipients()
{
    QDialog selectionDialog(this);
    selectionDialog.setWindowTitle(tr("Select Contacts"));
    selectionDialog.showMaximized();

    QVBoxLayout *vbl = new QVBoxLayout(&selectionDialog);
    selectionDialog.setLayout(vbl);

    //TODO For MMS messages only phone number selection is possible until address selector has
    //support for multiple address selection per session
    AddressSelectorWidget::SelectionMode selectionMode = AddressSelectorWidget::EmailSelection;
    if(!m_allowEmails)
    {
        if(m_allowPhoneNumbers)
            selectionMode = AddressSelectorWidget::PhoneSelection;
        else
            selectionMode = AddressSelectorWidget::InstantMessageSelection;
    }

    AddressSelectorWidget* addressSelector= new AddressSelectorWidget(selectionMode, &selectionDialog);
    vbl->addWidget(addressSelector);
    addressSelector->setSelectedAddresses(text().split(", "));

    if(QtopiaApplication::execDialog(&selectionDialog) == QDialog::Accepted)
    {
        QStringList selectedAddresses = addressSelector->selectedAddresses();
        setText(addressSelector->selectedAddresses().join(", "));
    }
}

void RecipientEdit::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Select && text().isEmpty())
        editRecipients();
    else
        DetailsLineEdit::keyPressEvent(e);
}

void RecipientEdit::updateMenuBar(const QString &text)
{
    static const bool keypadAbsent(style()->inherits("QThumbStyle"));

    if (keypadAbsent) {
        if (text.isEmpty())
            QSoftMenuBar::setLabel(this, Qt::Key_Back, QSoftMenuBar::Cancel, QSoftMenuBar::EditFocus);
        else
            QSoftMenuBar::setLabel(this, Qt::Key_Back, ":icon/qtmail/enqueue", tr("Send"));
    } else {
        if (text.isEmpty()) {
            QSoftMenuBar::setLabel(this, Qt::Key_Select, ":icon/addressbook/AddressBook", tr("Search"));
            QSoftMenuBar::setLabel(this, Qt::Key_Back, QSoftMenuBar::RevertEdit, QSoftMenuBar::EditFocus);
        } else {
            QSoftMenuBar::setLabel(this, Qt::Key_Select, ":icon/qtmail/enqueue", tr("Send"));
            QSoftMenuBar::clearLabel(this, Qt::Key_Back);
        }
    }
}


class RecipientSelectorButton : public QToolButton
{
    Q_OBJECT

public:
    RecipientSelectorButton(QWidget *parent, QLayout *layout, RecipientEdit *sibling);

private:
    static QIcon s_icon;
};

RecipientSelectorButton::RecipientSelectorButton(QWidget *parent, QLayout *layout, RecipientEdit* sibling)
    : QToolButton(parent)
{
    setFocusPolicy( Qt::NoFocus );
    setText( tr( "..." ) );
    setIcon(QIcon(":icon/addressbook/AddressBook"));

    connect( this, SIGNAL(clicked()), sibling, SLOT(setFocus()) );
    connect( this, SIGNAL(clicked()), sibling, SLOT(editRecipients()) );

    layout->addWidget( this );
}

//===========================================================================

class FromAddressComboBox : public QComboBox
{
    Q_OBJECT
public:
    FromAddressComboBox(QWidget *parent = 0);
    ~FromAddressComboBox();

signals:
    void send();
    void done();

protected:
    void keyPressEvent(QKeyEvent *);
};

FromAddressComboBox::FromAddressComboBox(QWidget *parent)
    : QComboBox(parent)
{
}

FromAddressComboBox::~FromAddressComboBox()
{
}

void FromAddressComboBox::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Back)
        emit done();
    else
        QComboBox::keyPressEvent(e);
}

//===========================================================================

class SendPushButton : public QPushButton
{
    Q_OBJECT
public:
    SendPushButton(const QString & text, QWidget * parent = 0);

signals:
    void done();

protected:
    void keyPressEvent(QKeyEvent *);
};

SendPushButton::SendPushButton(const QString & text, QWidget * parent)
    : QPushButton(text, parent)
{
}

void SendPushButton::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Back)
        emit done();
    else
        QPushButton::keyPressEvent(e);
}

//===========================================================================

static void addActionsFromWidget(QWidget* sourceWidget, QMenu* targetMenu)
{
    if(!sourceWidget) return;
    foreach(QAction* a,sourceWidget->actions())
        targetMenu->addAction(a);
}

DetailsPage::DetailsPage( QWidget *parent, const char *name )
    : QWidget( parent ), m_type( -1 ), m_previousAction(0)
{
    QMenu *menu = QSoftMenuBar::menuFor( this );
    QWidget* p = QWidget::parentWidget();
    while(p)
    {
        addActionsFromWidget(p,menu);
        p = p->parentWidget();
    }

    m_previousAction = new QAction( QIcon(":icon/edit"),tr("Edit message"),this);
    connect( m_previousAction, SIGNAL(triggered()), this, SIGNAL(editMessage()) );
    menu->addAction(m_previousAction);

    m_ignoreFocus = true;
    setObjectName( name );
    QIcon abicon(":icon/addressbook/AddressBook");
//    QMenu *menu = QSoftMenuBar::menuFor( this );
    if( !Qtopia::mousePreferred() )
    {
        menu->addAction( abicon, tr("From contacts", "Find recipient's phone number or email address from Contacts application"),
                         this, SLOT(editRecipients()) );
        menu->addSeparator();
#ifndef QT_NO_CLIPBOARD
        menu->addAction( QIcon(":icon/copy"), tr("Copy"),
                         this, SLOT(copy()) );
        menu->addAction( QIcon(":icon/paste"), tr("Paste"),
                         this, SLOT(paste()) );
#endif
    }

    const int margin = 2;
    setMaximumWidth( qApp->desktop()->width() - 2 * margin );
    QGridLayout *l = new QGridLayout( this );
    int rowCount = 0;

    m_toFieldLabel = new QLabel( this );
    m_toFieldLabel->setText( tr( "To" ) );
    m_toBox = new QHBoxLayout( );
    m_toField = new RecipientEdit( this );
    //setFocusProxy(m_toField);
    m_toBox->addWidget( m_toField );
    m_toFieldLabel->setBuddy(m_toField);
    connect( m_toField, SIGNAL(textChanged(QString)), this, SIGNAL(changed()) );
    connect( m_toField, SIGNAL(send()), this, SIGNAL(sendMessage()) );
    connect( m_toField, SIGNAL(done()), this, SIGNAL(cancel()) );
    l->addWidget( m_toFieldLabel, rowCount, 0 );
    QSoftMenuBar::addMenuTo(m_toField, menu);

    m_toPicker = ( Qtopia::mousePreferred() ? new RecipientSelectorButton(this, m_toBox, m_toField) : 0 );
    l->addLayout( m_toBox, rowCount, 2 );
    ++rowCount;

    m_ccFieldLabel = new QLabel( this );
    m_ccFieldLabel->setText( tr( "CC" ) );
    m_ccBox = new QHBoxLayout( );
    m_ccField = new RecipientEdit( this );
    m_ccBox->addWidget( m_ccField );
    m_ccFieldLabel->setBuddy(m_ccField);
    connect( m_ccField, SIGNAL(textChanged(QString)), this, SIGNAL(changed()) );
    connect( m_ccField, SIGNAL(send()), this, SIGNAL(sendMessage()) );
    connect( m_ccField, SIGNAL(done()), this, SIGNAL(cancel()) );
    l->addWidget( m_ccFieldLabel, rowCount, 0 );
    QSoftMenuBar::addMenuTo( m_ccField, menu );

    m_ccPicker = ( Qtopia::mousePreferred() ? new RecipientSelectorButton(this, m_ccBox, m_ccField) : 0 );
    l->addLayout( m_ccBox, rowCount, 2 );
    ++rowCount;

    m_bccFieldLabel = new QLabel( this );
    m_bccFieldLabel->setText( tr( "BCC" ) );
    m_bccBox = new QHBoxLayout( );
    m_bccField = new RecipientEdit( this );
    m_bccBox->addWidget( m_bccField );
    m_bccFieldLabel->setBuddy(m_bccField);
    connect( m_bccField, SIGNAL(textChanged(QString)), this, SIGNAL(changed()) );
    connect( m_bccField, SIGNAL(send()), this, SIGNAL(sendMessage()) );
    connect( m_bccField, SIGNAL(done()), this, SIGNAL(cancel()) );
    l->addWidget( m_bccFieldLabel, rowCount, 0 );
    QSoftMenuBar::addMenuTo( m_bccField, menu );

    m_bccPicker = ( Qtopia::mousePreferred() ? new RecipientSelectorButton(this, m_bccBox, m_bccField) : 0 );
    l->addLayout( m_bccBox, rowCount, 2 );
    ++rowCount;

    m_subjectFieldLabel = new QLabel( this );
    m_subjectFieldLabel->setText( tr( "Subject" ) );
    m_subjectField = new DetailsLineEdit( this );
    m_subjectFieldLabel->setBuddy(m_subjectField);
    connect( m_subjectField, SIGNAL(textChanged(QString)), this, SIGNAL(changed()) );
    connect( m_subjectField, SIGNAL(send()), this, SIGNAL(sendMessage()) );
    connect( m_subjectField, SIGNAL(done()), this, SIGNAL(cancel()) );
    l->addWidget( m_subjectFieldLabel, rowCount, 0 );
    l->addWidget( m_subjectField, rowCount, 2 );
    ++rowCount;
    QSoftMenuBar::addMenuTo( m_subjectField, menu );

    m_deliveryReportField = new QCheckBox( tr("Delivery report"), this );
    l->addWidget( m_deliveryReportField, rowCount, 0, 1, 3, Qt::AlignLeft );
    ++rowCount;

    m_readReplyField = new QCheckBox( tr("Read reply"), this );
    l->addWidget( m_readReplyField, rowCount, 0, 1, 3, Qt::AlignLeft );
    ++rowCount;

    m_fromFieldLabel = new QLabel( this );
    m_fromFieldLabel->setEnabled( true );
    m_fromFieldLabel->setText( tr( "From" ) );
    m_fromField = new FromAddressComboBox( this );
    m_fromFieldLabel->setBuddy(m_fromField);
    m_fromField->setEnabled( true );
    m_fromField->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum )); // Why not automatic?
    QSoftMenuBar::setLabel(m_fromField, Qt::Key_Back, QSoftMenuBar::Cancel);
    connect( m_fromField, SIGNAL(activated(int)), this, SIGNAL(changed()) );
    connect( m_fromField, SIGNAL(done()), this, SIGNAL(cancel()) );
    l->addWidget( m_fromFieldLabel, rowCount, 0 );
    l->addWidget( m_fromField, rowCount, 2 );
    ++rowCount;
    QSoftMenuBar::addMenuTo( m_fromField, menu );

    QPushButton *sendButton = new SendPushButton( tr("Send"), this );
    QSoftMenuBar::setLabel(sendButton, Qt::Key_Back, QSoftMenuBar::Cancel);
    connect( sendButton, SIGNAL(clicked()), this, SIGNAL(sendMessage()) );
    connect( sendButton, SIGNAL(done()), this, SIGNAL(cancel()) );
    l->addWidget( sendButton, rowCount, 0, 1, 3 );
    ++rowCount;

    QSpacerItem* spacer1 = new QSpacerItem( 4, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
    l->addItem( spacer1, rowCount, 1 );
    ++rowCount;

    QList<QWidget*> tabOrderList;

    tabOrderList.append( m_toField );
    if( Qtopia::mousePreferred() && m_toPicker)
        tabOrderList.append( m_toPicker );
    tabOrderList.append( m_ccField );
    if( Qtopia::mousePreferred() && m_ccPicker)
        tabOrderList.append( m_ccPicker );
    tabOrderList.append( m_bccField );
    if( Qtopia::mousePreferred() && m_bccPicker)
        tabOrderList.append( m_bccPicker );
    tabOrderList.append( m_subjectField );
    tabOrderList.append( m_fromField );

    QListIterator<QWidget*> it( tabOrderList );
    QWidget *prev = 0;
    QWidget *next;
    while ( it.hasNext() ) {
        next = it.next();
        if ( prev )
            setTabOrder( prev, next );
        prev = next;
    }
}

void DetailsPage::setDefaultAccount( const QMailAccountId& defaultId )
{
    QStringList accounts;
    QMailAccountKey accountsKey;

    if ((m_type == QMailMessage::Email) || (m_type == QMailMessage::Instant)) {
        accountsKey = QMailAccountKey(QMailAccountKey::MessageType, m_type);
    } else {
        m_fromField->hide();
        m_fromFieldLabel->hide();
        return;
    }

    // TODO: We should use the account name to allow the user to choose their sending 
    // account, rather than the address.  The address is an implementation detail, 
    // and we shouldn't need the configuration detail merely to allow account selection.

    QMailAccountIdList accountIds = QMailStore::instance()->queryAccounts(accountsKey);
    foreach (const QMailAccountId &id, accountIds) {
        QMailAccount account(id);
        if (account.canSendMail()) {
            if ( id == defaultId )
                accounts.prepend(account.displayName());
            else
                accounts.append(account.displayName());
        }
    }

    bool matching(accounts.count() == m_fromField->count());
    if (matching) {
        int i = 0;
        foreach (const QString& item, accounts) {
            if (item != m_fromField->itemText(i)) {
                matching = false;
                break;
            }
        }
    }

    if (!matching) {
        m_fromField->clear();
        m_fromField->addItems( accounts );
        if (m_fromField->count() < 2) {
            m_fromField->hide();
            m_fromFieldLabel->hide();
        } else {
            m_fromField->show();
            m_fromFieldLabel->show();
        }
    }
}

void DetailsPage::editRecipients()
{
    RecipientEdit *edit = 0;
    if (Qtopia::mousePreferred()) {
        if( sender() == m_toPicker )
            edit = m_toField;
        else if( sender() == m_ccPicker )
            edit = m_ccField;
        else if( sender() == m_bccPicker )
            edit = m_bccField;
    } else {
        QWidget *w = focusWidget();
        if( w && w->inherits("RecipientEdit") )
            edit = static_cast<RecipientEdit *>(w);
    }
    if (edit)
        edit->editRecipients();
    else
        qWarning("DetailsPage::editRecipients: Couldn't find line edit for recipients.");
}

void DetailsPage::setType( int t )
{
    QtopiaApplication::InputMethodHint imHint = QtopiaApplication::Normal;
    if( m_type != t )
    {
        m_allowPhoneNumbers = false;
        m_allowEmails = false;
        m_type = t;
        m_ccField->hide();
        if (m_ccPicker)
            m_ccPicker->hide();
        m_ccFieldLabel->hide();
        m_bccField->hide();
        if (m_bccPicker)
            m_bccPicker->hide();
        m_bccFieldLabel->hide();
        m_subjectField->hide();
        m_subjectFieldLabel->hide();
        m_fromField->hide();
        m_fromFieldLabel->hide();
        m_readReplyField->hide();
        m_deliveryReportField->hide();

        if( t == QMailMessage::Mms )
        {
            m_allowPhoneNumbers = true;
            //m_allowEmails = true; //TODO reenable when address picker supports selection of multiple types
            m_ccFieldLabel->show();
            m_ccField->show();
            if (m_ccPicker)
                m_ccPicker->show();
            m_bccFieldLabel->show();
            m_bccField->show();
            if (m_bccPicker)
                m_bccPicker->show();
            m_subjectField->show();
            m_subjectFieldLabel->show();
            m_readReplyField->show();
            m_deliveryReportField->show();
        }
        else if( t == QMailMessage::Sms )
        {
            m_allowPhoneNumbers = true;

        }
        else if( t == QMailMessage::Email )
        {
            m_allowEmails = true;
            m_ccFieldLabel->show();
            m_ccField->show();
            if (m_ccPicker)
                m_ccPicker->show();
            m_bccFieldLabel->show();
            m_bccField->show();
            if (m_bccPicker)
                m_bccPicker->show();
            m_subjectField->show();
            m_subjectFieldLabel->show();
            if (m_fromField->count() >= 2) {
                m_fromField->show();
                m_fromFieldLabel->show();
            }
        }

        if( m_allowPhoneNumbers )
            imHint = QtopiaApplication::PhoneNumber;
        else if( m_allowEmails )
            imHint = QtopiaApplication::Words;

        foreach (RecipientEdit* field, (QList<RecipientEdit*>() << m_toField << m_ccField << m_bccField)) {
            if (imHint == QtopiaApplication::Words)
                QtopiaApplication::setInputMethodHint(field, QtopiaApplication::Named, "email noautocapitalization");
            else
                QtopiaApplication::setInputMethodHint(field, imHint);

            field->setMultipleAllowed(true);
            field->setPhoneNumbersAllowed(m_allowPhoneNumbers);
            field->setEmailAllowed(m_allowEmails);
        }
    }

    layout()->activate();
}

void DetailsPage::setDetails( const QMailMessage &mail )
{
    setTo(QMailAddress::toStringList(mail.to()).join(", "));
    setCc(QMailAddress::toStringList(mail.cc()).join(", "));
    setBcc(QMailAddress::toStringList(mail.bcc()).join(", "));

    if ((mail.subject() != placeholder) &&
        (m_type != QMailMessage::Instant) &&
        !((m_type == QMailMessage::Sms) && (mail.contentType().content().toLower() == "text/x-vcard"))) {
        setSubject(mail.subject());
    }

    setFrom(mail.from().address());

    if (mail.headerFieldText("X-Mms-Delivery-Report") == "Yes") {
        m_deliveryReportField->setChecked(true);
    }
    if (mail.headerFieldText("X-Mms-Read-Reply") == "Yes") {
        m_readReplyField->setChecked(true);
    }
}

void DetailsPage::getDetails( QMailMessage &mail ) const
{
    if (m_type == QMailMessage::Sms) {
        // For the time being limit sending SMS messages so that they can
        // only be sent to phone numbers and not email addresses
        QString number = to();
        QString n;
        QStringList numbers;
        bool mightBeNumber = true;
        for ( int posn = 0, len = number.length(); posn < len; ++posn ) {
            uint ch = number[posn].unicode();
            // If we know this isn't a number...
            if (!mightBeNumber) {
                // If this is the end of this token, next one might be a number
                if (ch == ' ' || ch == ',') {
                    mightBeNumber = true;
                }
                continue;
            }
            if ( ch >= '0' && ch <= '9' ) {
                n += QChar(ch);
            } else if ( ch == '+' || ch == '#' || ch == '*' ) {
                n += QChar(ch);
            } else if ( ch == '-' || ch == '(' || ch == ')' ) {
                n += QChar(ch);
            } else if ( ch == ' ' ) {
                n += QChar(ch);
            } else if ( ch == ',' || ch == '<' || ch == '>') {
                if (!n.isEmpty())
                    numbers.append( n );
                n = "";
            } else {
                // this token is definitely not a valid number
                mightBeNumber = false;
                n = "";
            }
        }
        if (!n.isEmpty())
            numbers.append( n );
        mail.setTo(QMailAddress::fromStringList(numbers));
    } else {
        mail.setTo(QMailAddress::fromStringList(to()));
    }

    mail.setCc(QMailAddress::fromStringList(cc()));
    mail.setBcc(QMailAddress::fromStringList(bcc()));

    QString subjectText = subject();

    // For SMS, store cosmetic subject data
    if (m_type == QMailMessage::Sms) {
        if (!mail.hasBody()) {
            subjectText = placeholder;
        } else {
            // For VCard data, use a summary line
            if (mail.contentType().content().toLower() == "text/x-vcard") {
                QList<QContact> contacts = QContact::readVCard(mail.body().data(QMailMessageBody::Decoded));
                if (contacts.count() == 0) {
                    // Invalid VCard data, so just show raw data
                } else if (contacts.count() == 1) {
                    QString name = tr( "vCard describing %1", "%1 = Person's name" );
                    QContact& contact = contacts[0];
                    if (!contact.nickname().isEmpty()) {
                        subjectText = name.arg(contact.nickname());
                    } else if (!contact.firstName().isEmpty() && !contact.lastName().isEmpty()) {
                        subjectText = name.arg(contact.firstName() + " " + contact.lastName());
                    } else if (!contact.firstName().isEmpty()) {
                        subjectText = name.arg(contact.firstName());
                    } else {
                        subjectText = tr("vCard describing a contact");
                    }
                } else if ( contacts.count() > 1 ) {
                    subjectText = tr("vCard describing multiple contacts");
                }
            } 

            if (subjectText.isEmpty()) {
                // Use a portion of the message itself
                subjectText = mail.body().data();
                if (subjectText.length() > MaximumSmsSubjectLength) {
                    // No point having a huge subject - elide, using unicode elipsis char
                    subjectText = subjectText.left(MaximumSmsSubjectLength) + QChar(0x2026);
                }
            }
        }
    } else if (m_type == QMailMessage::Instant) {
        // If the message is small and textual, keeping a copy in the subject will save us loading the body
        if (mail.hasBody() && (mail.contentType().type().toLower() == "text")) {
            QString text = mail.body().data();

            if (text.length() > MaximumInstantSubjectLength) {
                // Append elipsis character
                subjectText = text.left(MaximumInstantSubjectLength) + QChar(0x2026);
                mail.appendHeaderField("X-qtopia-internal-truncated-subject", "true");
            } else {
                subjectText = mail.body().data();
            }
        }
    } else {
        // For other messages, show a placeholder if required
        if (subjectText.isEmpty())
            subjectText = placeholder;
    }

    if (!subjectText.isEmpty())
        mail.setSubject(subjectText);

    QMailAccount account = fromAccount();
    if (account.id().isValid()) {
        AccountConfiguration config(account.id());
        mail.setFrom(QMailAddress(config.userName(), config.emailAddress()));
        mail.setParentAccountId(account.id());
    }

    if (m_type == QMailMessage::Mms) {
        if (m_deliveryReportField->isChecked())
            mail.setHeaderField("X-Mms-Delivery-Report", "Yes");
        if (m_readReplyField->isChecked())
            mail.setHeaderField("X-Mms-Read-Reply", "Yes");
    }
}

bool DetailsPage::isDetailsOnlyMode() const
{
    return !m_previousAction->isVisible();
}

void DetailsPage::setDetailsOnlyMode(bool val)
{
    m_previousAction->setVisible(!val);
}

void DetailsPage::setBcc( const QString &a_bcc )
{
    m_bccField->setText( a_bcc );
}

QString DetailsPage::bcc() const
{
    QString text;
    if( !m_bccField->isHidden() )
        text = m_bccField->text();
    return text;
}


void DetailsPage::setCc( const QString &a_cc )
{
    m_ccField->setText( a_cc );
}

QString DetailsPage::cc() const
{
    QString text;
    if( !m_ccField->isHidden() )
        text = m_ccField->text();
    return text;
}

void DetailsPage::setTo( const QString &a_to )
{
    m_toField->setText( a_to );
}

QString DetailsPage::to() const
{
    return m_toField->text();
}

QString DetailsPage::subject() const
{
    return m_subjectField->text();
}

void DetailsPage::setSubject( const QString &sub )
{
    m_subjectField->setText( sub );
}

QString DetailsPage::from() const
{
    return m_fromField->currentText();
}

/*  Find the account matching the email address */
QMailAccount DetailsPage::fromAccount() const
{
    QMailAccountIdList accountIds = QMailStore::instance()->queryAccounts();

    foreach(const QMailAccountId& id, accountIds) {
        QMailAccount account(id);
        if (account.canSendMail() && account.displayName() == from())
            return account;
    }

    return QMailAccount();
}

void DetailsPage::setFrom( const QString &from )
{
    // Find the account matching this address
    QMailAccountKey addressKey(QMailAccountKey::EmailAddress, from);
    QMailAccountIdList accountIds = QMailStore::instance()->queryAccounts(addressKey);

    if (!accountIds.isEmpty()) {
        QMailAccount account(accountIds.first());

        // Find the entry matching this account
        for (int i = 0; i < static_cast<int>(m_fromField->count()); ++i) {
            if (m_fromField->itemText(i) == account.displayName()) {
                m_fromField->setCurrentIndex(i);
                break;
            }
        }
    }
}

void DetailsPage::copy()
{
#ifndef QT_NO_CLIPBOARD 
    QWidget *fw = focusWidget();
    if( !fw )
        return;
    if( fw->inherits( "QLineEdit" ) )
        static_cast<QLineEdit*>(fw)->copy();
    else if( fw->inherits( "QTextEdit" ) )
        static_cast<QTextEdit*>(fw)->copy();
#endif
}

void DetailsPage::paste()
{
#ifndef QT_NO_CLIPBOARD 
    QWidget *fw = focusWidget();
    if( !fw )
        return;
    if( fw->inherits( "QLineEdit" ) )
        static_cast<QLineEdit*>(fw)->paste();
    else if( fw->inherits( "QTextEdit" ))
        static_cast<QTextEdit*>(fw)->paste();
#endif
}

void DetailsPage::clear()
{
    m_toField->clear();
    m_ccField->clear();
    m_bccField->clear();
    m_subjectField->clear();
    m_readReplyField->setChecked( false );
    // don't clear from fields
}

#include <detailspage.moc>
