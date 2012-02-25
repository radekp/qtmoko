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

#include <qcontact.h>
#include <qcontactmodel.h>

#include <qtopianamespace.h>
#include <qtimestring.h>
#include "vobject_p.h"
#include <qthumbnail.h>

#include <QSettings>
#include <QObject>
#include <QApplication>
#include <QRegExp>
#include <QTextDocument>
#include <QFileInfo>
#include <QImage>
#include <QIcon>
#include <QBuffer>
#include <QTextCodec>
#include <QDataStream>
#include <QPixmapCache>
#include <QPixmap>
#include <QImageReader>
#include <QDesktopWidget>
#include <QGlobalPixmapCache>
#include <QDebug>

#include <stdio.h>
#include <time.h>

#include "qcontactsqlio_p.h"

//#define PRINT_UNRECOGNIZED_VCARD_DATA

const int PortraitWidth = 60;
const int PortraitHeight = 75;

const int ThumbnailWidth = 24;
const int ThumbnailHeight = 30;


class QContactData : public QSharedData
{
public:
    QStringList emails;
    int gender;

    QDate birthday;
    QDate anniversary;

    QContact::PhoneType defaultPhoneNumber;
    QMap<QContact::PhoneType, QString> phoneNumbers;

    QMap<QContact::Location, QContactAddress> address;

    // string fields
    QMap<int, QString> mMap;

    // for pim record
    QUniqueId mUid;
    QList<QString> mCategories;
    QMap<QString, QString> customMap;

    enum Field {
        Street,
        City,
        State,
        Zip,
        Country
    };

    void setAddress(QContact::Location l, Field f, const QString &str) {
        switch(f) {
            case Street:
                address[l].street = str;
                break;
            case City:
                address[l].city = str;
                break;
            case State:
                address[l].state = str;
                break;
            case Zip:
                address[l].zip = str;
                break;
            case Country:
                address[l].country = str;
                break;
        }
        if (address[l].isEmpty())
            address.remove(l);
    }

};

extern QTOPIAPIM_EXPORT int q_DontDecodeBase64Photo;

QString emailSeparator() { return " "; }

/* Return an absolute path (or qrc path) for a contacts portraitfile */
static QString resolvePortraitPath(QString portraitFile)
{
    if (portraitFile.isEmpty())
        return QString();
    // This would be handy, but too prone to abuse
//    if (portraitFile.startsWith('/'))
//        return portraitFile;
    if (portraitFile.startsWith(':'))
        return portraitFile;

    QString baseDir = Qtopia::applicationFileName( "addressbook", "contactimages/" );
    return baseDir + portraitFile;
}

/*!
  \class QContact
    \inpublicgroup QtUiModule
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtTelephonyModule
    \inpublicgroup QtPimModule

  \ingroup pim
  \brief The QContact class holds the data of an address book entry.

  This data includes information the name of the contact, phone numbers
  and email addresses, postal address information, and business
  information such as department and job title.

  It also allows a portrait picture (and corresponding thumbnail) to
  be specified, provides functions for matching a contact against a
  regular expression, and converting to and from vCard format.

  \sa QContactAddress, {Pim Library}
*/

/*!
  Constructs a new QContact.
*/
QContact::QContact() : QPimRecord()
{
    d = new QContactData;
    d->defaultPhoneNumber = HomePhone;
    d->gender = 0;
}
/*!
  Constructs a new QContact that is a copy of \a contact.
*/
QContact::QContact(const QContact &contact) : QPimRecord(contact)
{
    d = contact.d;
}

/*!
  Destroys the contact.
*/
QContact::~QContact()
{
}

/*!
  Makes a copy of \a contact and assigns it to this QContact.
*/
QContact &QContact::operator=(const QContact &contact)
{
    d = contact.d;
    return *this;
}

/*!
  Returns a rich text formatted QString of the QContact.
*/
QString QContact::toRichText() const
{
    QString text;
    QString value, comp, state;

    // name, jobtitle and company
    if ( !(value = label()).isEmpty() )
        text += "<b>" + Qt::escape(value) + "</b><br>";
    // also part of name is how to pronounce it.

    if ( !(value = firstNamePronunciation()).isEmpty() )
        text += "<b>( " + Qt::escape(value) + " )</br><br>";

    if ( !(value = lastNamePronunciation()).isEmpty() )
        text += "<b>( " + Qt::escape(value) + " )</br><br>";

    if ( !(value = jobTitle()).isEmpty() )
        text += Qt::escape(value) + "<br>";

    comp = company();
    if ( !(value = department()).isEmpty() ) {
        text += Qt::escape(value);
        if ( !comp.isEmpty() )
            text += ", ";
        else
            text += "<br>";
    }
    if ( !comp.isEmpty() )
        text += Qt::escape(comp) + "<br>";
    if ( !(value = companyPronunciation()).isEmpty() )
        text += "<b>( " + Qt::escape(value) + " )</br><br>";


    // business address
    if ( !businessStreet().isEmpty() || !businessCity().isEmpty() ||
         !businessZip().isEmpty() || !businessCountry().isEmpty() ) {
        text += "<br>";
        text += "<b>" + qApp->translate( "QtopiaPim",  "Work Address: " ) + "</b>";
        text +=  "<br>";
    }

    if ( !(value = businessStreet()).isEmpty() )
        text += Qt::escape(value) + "<br>";
    state =  businessState();
    if ( !(value = businessCity()).isEmpty() ) {
        text += Qt::escape(value);
        if ( !state.isEmpty() )
            text += ", " + Qt::escape(state);
        text += "<br>";
    } else if ( !state.isEmpty() )
        text += Qt::escape(state) + "<br>";
    if ( !(value = businessZip()).isEmpty() )
        text += Qt::escape(value) + "<br>";
    if ( !(value = businessCountry()).isEmpty() )
        text += Qt::escape(value) + "<br>";

    // home address
    if ( !homeStreet().isEmpty() || !homeCity().isEmpty() ||
         !homeZip().isEmpty() || !homeCountry().isEmpty() ) {
        text += "<br>";
        text += "<b>" + qApp->translate( "QtopiaPim",  "Home Address: " ) + "</b>";
        text +=  "<br>";
    }

    if ( !(value = homeStreet()).isEmpty() )
        text += Qt::escape(value) + "<br>";
    state =  homeState();
    if ( !(value = homeCity()).isEmpty() ) {
        text += Qt::escape(value);
        if ( !state.isEmpty() )
            text += ", " + Qt::escape(state);
        text += "<br>";
    } else if (!state.isEmpty())
        text += Qt::escape(state) + "<br>";
    if ( !(value = homeZip()).isEmpty() )
        text += Qt::escape(value) + "<br>";
    if ( !(value = homeCountry()).isEmpty() )
        text += Qt::escape(value) + "<br>";

    // the others...
    QString str;
    str = emailList().join(", ");
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim", "Email Addresses:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = homePhone();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Home Phone:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = homeFax();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Home Fax:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = homeMobile();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Home Mobile:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = homeVOIP();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Home VOIP:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = homeWebpage();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Home Web Page:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = businessWebpage();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Business Web Page:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = office();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Office:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = businessPhone();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Business Phone:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = businessFax();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Business Fax:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = businessMobile();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Business Mobile:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = businessVOIP();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Business VOIP:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = businessPager();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Business Pager:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = profession();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Profession:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = assistant();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Assistant:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = manager();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Manager:") + " </b>"
                + Qt::escape(str) + "<br>";
    str = gender();
    if ( !str.isEmpty() && str.toInt() != 0 ) {
        if ( str.toInt() == 1 )
            str = qApp->translate( "QtopiaPim", "Male" );
        else if ( str.toInt() == 2 )
            str = qApp->translate( "QtopiaPim", "Female" );
        text += "<b>" + qApp->translate( "QtopiaPim","Gender:") + " </b>" + str + "<br>";
    }
    str = spouse();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Spouse:") + " </b>"
                + Qt::escape(str) + "<br>";
    if ( birthday().isValid() ) {
        str = QTimeString::localYMD( birthday() );
        if ( !str.isEmpty() )
            text += "<b>" + qApp->translate( "QtopiaPim","Birthday:") + " </b>"
                + Qt::escape(str) + "<br>";
    }
    if ( anniversary().isValid() ) {
        str = QTimeString::localYMD( anniversary() );
        if ( !str.isEmpty() )
            text += "<b>" + qApp->translate( "QtopiaPim","Anniversary:") + " </b>"
                + Qt::escape(str) + "<br>";
    }
    str = nickname();
    if ( !str.isEmpty() )
        text += "<b>" + qApp->translate( "QtopiaPim","Nickname:") + " </b>"
                + Qt::escape(str) + "<br>";

    // notes last
    if ( !(value = notes()).isEmpty() ) {
        text += "<br>" + Qt::convertFromPlainText(value) + "<br>";
    }
    return text;
}

/*!
  \internal
*/
void QContact::insert( int key, const QString &v )
{
    QString value = v.trimmed();
    if ( value.isEmpty() )
        d->mMap.remove( key );
    else
        d->mMap.insert( key, value );
}

/*!
  \internal
*/
void QContact::replace( int key, const QString & v )
{
    QString value = v.trimmed();
    if ( value.isEmpty() )
        d->mMap.remove( key );
    else
        d->mMap.insert( key, value );
}

/*!
  \internal
*/
QString QContact::find( int key ) const
{
    return d->mMap[key];
}

/*!
  \internal
*/
QString QContact::displayAddress( const QString &street,
                                 const QString &city,
                                 const QString &state,
                                 const QString &zip,
                                 const QString &country ) const
{
    QString s = street;
    if ( !street.isEmpty() )
        s+= "\n";
    s += city;
    if ( !city.isEmpty() && !state.isEmpty() )
        s += ", ";
    s += state;
    if ( !state.isEmpty() && !zip.isEmpty() )
        s += "  ";
    s += zip;
    if ( !country.isEmpty() && !s.isEmpty() )
        s += "\n";
    s += country;
    return s;
}

/*!
  \fn QString QContact::displayHomeAddress() const
  Returns a formatted string with the contact's home address.
*/

/*!
  \fn QString QContact::displayBusinessAddress() const
  Returns a formatted string with the contact's business address.
*/

/*!
  Returns a formatted string with the contact's address of type \a location.
*/
QString QContact::displayAddress(Location location) const
{
    QContactAddress a = address(location);
    return displayAddress( a.street, a.city,
                           a.state, a.zip,
                           a.country );
}

/*!
  Returns a list of phone types that can be assigned as a contact
  phone number.
*/
QList<QContact::PhoneType> QContact::phoneTypes()
{
    static QList<QContact::PhoneType> result;
    if (result.count() == 0)
    {
        //result.append(OtherPhone);
        result.append(HomePhone);
        result.append(BusinessPhone);
        //result.append(Mobile);
        //result.append(Fax);
        //result.append(Pager);
        result.append(HomeMobile);
        result.append(HomeFax);
        result.append(HomeVOIP);
        //result.append(HomePager);
        result.append(BusinessMobile);
        result.append(BusinessFax);
        result.append(BusinessPager);
        result.append(BusinessVOIP);
    }
    return result;
}

/*!
   Returns an icon representing the phone number \a type.
   Returns a null icon if no icon is available for the
   phone number type.

   \sa phoneIconResource()
*/
QIcon QContact::phoneIcon(PhoneType type)
{
    QString r = phoneIconResource(type);
    if (r.isEmpty())
        return QIcon();
    return QIcon(r);
}

/*!
  Returns the resource location (e.g. \c ":icon/addressbook/homephone")
  for an icon representing the phone number \a type.  Returns a null
  string if no icon is available for the phone number \a type.

   \sa phoneIcon()
*/
QString QContact::phoneIconResource(PhoneType type)
{
    /* TODO more icons... */
    switch (type) {
        case HomePhone:
            return ":icon/addressbook/homephone";
        case HomeMobile:
            return ":icon/addressbook/homemobile";
        case HomeVOIP:
            return ":icon/addressbook/homevoip";
        case HomeFax:
            return ":icon/addressbook/homefax";
        case BusinessPhone:
            return ":icon/addressbook/businessphone";
        case BusinessMobile:
            return ":icon/addressbook/businessmobile";
        case BusinessVOIP:
            return ":icon/addressbook/businessvoip";
        case BusinessFax:
            return ":icon/addressbook/businessfax";
        case BusinessPager:
            return ":icon/addressbook/businesspager";
    default:
        return QString();
    }
}

/*!
  Returns the contact's list of phone numbers and their types.
*/
QMap<QContact::PhoneType, QString> QContact::phoneNumbers() const
{
    return d->phoneNumbers;
}

/*!
  Returns the contact's list of addresses with their types.
*/
QMap<QContact::Location, QContactAddress> QContact::addresses() const
{
    return d->address;
}

/*!
    Sets the set of phone numbers of the contact to the given \a numbers
*/
void QContact::setPhoneNumbers(const QMap<PhoneType, QString> &numbers)
{
    d->phoneNumbers = numbers;
    if (!d->phoneNumbers.isEmpty()
        && !d->phoneNumbers.keys().contains(d->defaultPhoneNumber))
    {
        d->defaultPhoneNumber = d->phoneNumbers.keys().first();
    }
}

/*!
  Sets the contact addresses to the given \a addresses
*/
void QContact::setAddresses(const QMap<Location, QContactAddress> &addresses)
{
    d->address = addresses;
}

/*!
  Clears the phone number list of the contact, including the default phone number.
*/
void QContact::clearPhoneNumbers()
{
    d->phoneNumbers.clear();
}

/*!
  Clears the addresses of the contact.
*/
void QContact::clearAddresses()
{
    d->address.clear();
}

/*!
  Return the default phone number of the contact.
  If no default phone number is set, this returns the null string.
*/
QString QContact::defaultPhoneNumber() const
{
    if (d->phoneNumbers.contains(d->defaultPhoneNumber))
        return d->phoneNumbers.value(d->defaultPhoneNumber);

    return QString();
}

/*!
  If the contact has an existing phone number that matches \a str,
  it will be set as the default phone number.
*/
void QContact::setDefaultPhoneNumber(const QString &str)
{
    QMapIterator<PhoneType, QString> i(d->phoneNumbers);
    while (i.hasNext()) {
        i.next();
        if (i.value() == str) {
            d->defaultPhoneNumber = i.key();
            return;
        }
    }
}

/*!
  If the contact has an existing phone number of a type that matches \a type,
  it will be set as the default phone number.
*/
void QContact::setDefaultPhoneNumber(PhoneType type)
{
    if (d->phoneNumbers.contains(type))
        d->defaultPhoneNumber = type;
}

/*!
  Sets the phone number of type \a type to \a str.

  If there is no default phone number set, this will set
  the given number as the default phone number.
*/
void QContact::setPhoneNumber(PhoneType type, const QString &str)
{
    if (str.simplified().isEmpty()) {
        d->phoneNumbers.remove(type);
        if (d->defaultPhoneNumber == type) {
            QMapIterator<PhoneType, QString> i(d->phoneNumbers);
            if (i.hasNext()) {
                i.next();
                d->defaultPhoneNumber = i.key();
            }
        }
    } else {
        if (d->phoneNumbers.isEmpty())
            d->defaultPhoneNumber = type;
        d->phoneNumbers.insert(type, str.simplified());
    }
}

/*!
  Sets the contact's gender to the enum value specified in \a g.
*/
void QContact::setGender( GenderType g )
{
    d->gender = (int)g;
}

/*! \enum QContact::GenderType
  This enum describes the three possible choices for gender.

  \value UnspecifiedGender
  \value Male
  \value Female
*/

/*!
  Returns the gender of the contact as type GenderType.
*/
QContact::GenderType QContact::gender() const
{
    return (GenderType)d->gender;
}

/*!
  Sets the contact's birthday to \a date.
*/
void QContact::setBirthday( const QDate &date )
{
    d->birthday = date;
}

/*!
  Return the contact's birthday.
  If this has not been set, the returned date
  will be null.
*/
QDate QContact::birthday() const
{
    return d->birthday;
}

/*!
  Sets the contact's anniversary to \a date.
*/
void QContact::setAnniversary( const QDate &date )
{
    d->anniversary = date;
}

/*!
  Return the contact's anniversary date.
  If this has not been set, the returned date
  will be null.
*/
QDate QContact::anniversary() const
{
    return d->anniversary;
}

/*!
  Adds \a email to the list of email address for the contact.
  If no default email is set, the default email will
  be set to \a email.
*/
void QContact::insertEmail( const QString &email )
{
    QString e = email.simplified();
    if (e.isEmpty())
        return;
    QString def = defaultEmail();

    // if no default, set it as the default email and don't insert
    if ( def.isEmpty() ) {
        setDefaultEmail( e ); // will insert into the list for us
        return;
    }

    if (!d->emails.contains(e))
        d->emails.append(e);
}

/*!
  Removes \a email from the list of email address for the contact.
  If the email is the default email, the default email will
  be set to the first one in the list.
*/
void QContact::removeEmail( const QString &email )
{
    QString e = email.simplified();
    QString def = defaultEmail();

    // otherwise, must first contain it
    if ( !d->emails.contains( e ) )
        return;

    d->emails.removeAll( e );

    if ( def == e ) {
        if ( d->emails.count() )
            setDefaultEmail( d->emails.first() );
    }
}
/*!
  Clear the email list for the contact, including
  the default email.
*/
void QContact::clearEmailList()
{
    d->emails.clear();
    d->mMap.remove(DefaultEmail);
}

/*!
  Sets the format for contact labels to that described by \a f.

  The format is a set of pattern separated by '|'s.  Each pattern is
  a set of space separated tokens.  A token can either be _ for a space,
  an identifier as from identifierKey(), or any string.  The format used for label()
  will be the first pattern for which all fields specified are non null for the contact.

  For example:
  \c {lastname , _ firstname | lastname | firstname | company}

  \sa labelFormat(), labelFields(), label()
*/

void QContact::setLabelFormat(const QString &f)
{
    ContactSqlIO::setFormat(f);
}

/*!
  Returns the format for contact labels.

  The format is a set of pattern separated by '|'s.  Each pattern is
  a set of space separated tokens.  A token can either be _ for a space,
  an identifier as from identifierKey(), or any string.  The format for label
  will the first pattern for which all fields specified are non null for the contact.

  For example:
  \c {lastname , _ firstname | lastname | firstname | company}

  \sa setLabelFormat(), labelFields(), label()
*/
QString QContact::labelFormat()
{
    return ContactSqlIO::format();
}

/*!
  Returns the string identifiers suitable for describing the format for the contact
  label.

  \sa labelFormat(), label()
*/
QStringList QContact::labelFields()
{
    return ContactSqlIO::labelIdentifiers();
}

/*!
  Returns a suitable display string for the contact.  This is built up
  by using fields of the contact and the format set for generating
  the contact labels.

  \sa labelFormat(), labelFields()
*/
QString QContact::label() const
{
    return ContactSqlIO::formattedLabel(*this);
}

/*!
  Returns the phone number of \a type for the contact.
  If the contact does not have a corresponding phone number,
  this will return a null QString.
*/
QString QContact::phoneNumber(PhoneType type) const
{
    if (d->phoneNumbers.contains(type))
        return d->phoneNumbers.value(type);
    return QString();
}

/*!
  Sets the contact's address for the given \a location to \a address.
  If the address is empty, this will effectively remove that \c Location
  type for this contact.

  \sa address()
*/
void QContact::setAddress(Location location, const QContactAddress &address)
{
    if (address.isEmpty()) {
        d->address.remove(location);
    } else {
        d->address.insert(location, address);
    }
}

/*!
  Returns the contact's address for the specified \a location.

  \sa setAddress()
*/
QContactAddress QContact::address(Location location) const
{
    if (d->address.contains(location))
        return d->address.value(location);
    return QContactAddress();
}

/*!
  Returns a list of email addresses belonging to the contact, including
  the default email address.

  \sa defaultEmail()
*/
QStringList QContact::emailList() const
{
    return d->emails;
}

/*!
  Sets the default email to \a v.
  If the given email address \a v is not already in the contact's
  list of email addresses, this function adds it.

  \sa defaultEmail(), setEmailList()
*/
void QContact::setDefaultEmail( const QString &v )
{
    QString e = v.simplified();
    if (e.isEmpty())
        return;

    replace( DefaultEmail, e );
    if (!d->emails.contains(e))
        d->emails.append(e);
}

/*!
  Sets the email list to \a emails
  \sa emailList(), defaultEmail()
*/
void QContact::setEmailList( const QStringList &emails )
{
    QStringList e = emails;
    // remove all nulls and empties.
    e.removeAll(QString());
    e.removeAll("");
    QString de = defaultEmail();
    d->emails = e;
    if (e.count() > 0) {
        if (de.isEmpty() || !e.contains(de))
            setDefaultEmail(e[0]);
    } else {
        d->mMap.remove( DefaultEmail );
    }
}

/*!
  Returns the portrait of the contact, scaled down to the thumbnail size.

  If there is no portrait set for the contact, this will return a default pixmap
  based on the contact's fields.

  \sa thumbnailSize()
*/
QPixmap QContact::thumbnail() const
{
    QPixmap result;

    QString pFile = portraitFile();

    if( !pFile.isEmpty() ) {
        QString key("pimcontact" + uid().toString() + "-cfl-thumb");
        QPixmap *cached = QPixmapCache::find( key );
        if( cached ) {
            result = *cached;
        } else {
            QSize tsize = thumbnailSize();
            QString pPath = resolvePortraitPath(pFile);
            QThumbnail thumbnail( pPath );
            result = thumbnail.pixmap( tsize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (tsize != result.size()) {
                // Make a transparent version, centered (we scale first, so not too many pixels)
                QImage img = result.toImage();
                if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_ARGB32_Premultiplied)
                    img = img.convertToFormat(QImage::Format_ARGB32);
                img = img.copy( ( result.width() - tsize.width() ) / 2,
                                  ( result.height() - tsize.height() ) / 2,
                                  tsize.width(), tsize.height() );
                result = QPixmap::fromImage(img);
            }
            QPixmapCache::insert( key, result );
        }
    }

    if (result.isNull()) {
        QLatin1String key = QLatin1String("pimcontact-generic-personal-thumb");
        QString filename(":image/addressbook/generic-personal-contact");

        if (nameTitle().isEmpty() && firstName().isEmpty()
                && middleName().isEmpty() && lastName().isEmpty()
                && suffix().isEmpty() && !company().isEmpty()) {
            key = QLatin1String("pimcontact-generic-corporation-contact-thumb");
            filename = ":image/addressbook/generic-corporation-contact";
        } else if (d->mCategories.contains("Business")) { // no tr
            key = QLatin1String("pimcontact-generic-thumb");
            filename = ":image/addressbook/generic-contact";
        }

        if (!QGlobalPixmapCache::find(key, result)) {
            QImageReader reader(filename);
            reader.setQuality( 49 ); // Otherwise Qt smooth scales
            reader.setScaledSize(thumbnailSize());
            QImage img = reader.read();
            if (!img.isNull()) {
                result = QPixmap::fromImage(img);
                QGlobalPixmapCache::insert(key, result);
            }
        }
    }

    return result;
}

/*!
  Returns the portrait of the contact as an icon.
  If there is no portrait set for the contact will return a default icon.

  \sa portrait(), thumbnailSize()
*/
QIcon QContact::icon() const
{

    if( !portraitFile().isEmpty() ) {
        QString pPath = resolvePortraitPath(portraitFile());
        QIcon i(pPath);
        if (!i.isNull())
            return i;
    }

    if (nameTitle().isEmpty() && firstName().isEmpty()
            && middleName().isEmpty() && lastName().isEmpty()
            && suffix().isEmpty() && !company().isEmpty()) {
        return corporationIcon();
    } else if (d->mCategories.contains("Business")) { // no tr
        return genericIcon();
    } else {
        return personalIcon();
    }
}

Q_GLOBAL_STATIC_WITH_ARGS(QIcon, cIcon, (QLatin1String(":icon/addressbook/generic-corporation-contact")));
Q_GLOBAL_STATIC_WITH_ARGS(QIcon, gIcon, (QLatin1String(":icon/addressbook/generic-contact")));
Q_GLOBAL_STATIC_WITH_ARGS(QIcon, pIcon, (QLatin1String(":icon/addressbook/generic-personal-contact")));

QIcon &QContact::corporationIcon()
{
    return *cIcon();
}

QIcon &QContact::genericIcon()
{
    return *gIcon();
}

QIcon &QContact::personalIcon()
{
    return *pIcon();
}

/*!
  Returns the size of the thumbnail of a contact's portrait.

  \sa thumbnail(), portraitSize()
*/
QSize QContact::thumbnailSize()
{
    static QSize size;

    if (!size.isValid()) {
        QDesktopWidget *desktop = QApplication::desktop();
        if (desktop) {
            int dpi = desktop->screen(desktop->primaryScreen())->logicalDpiY();
            // ThumbnailWidth x ThumbnailHeight at 100dpi
            size = QSize(ThumbnailWidth * dpi / 100, ThumbnailHeight * dpi / 100);
        } else {
            // We must not have constructed QApplication yet
            return QSize(ThumbnailWidth, ThumbnailHeight);
        }
    }

    return size;
}

/*!
  Returns the portrait of the contact as a pixmap.
  If there is no portrait set for the contact, this will return a default pixmap
  based on the contact's fields.

  \sa icon(), portraitSize()
*/
QPixmap QContact::portrait() const
{
    QPixmap result;

    if( !portraitFile().isEmpty() ) {
        QPixmap *cached = QPixmapCache::find( "pimcontact" + uid().toString() + "-cfl" );
        if( cached ) {
            result = *cached;
        } else {
            QString pPath = resolvePortraitPath(portraitFile());
            result.load( pPath );
            if (!result.isNull())
                QPixmapCache::insert( "pimcontact" + uid().toString() + "-cfl", result );
        }
    }

    if (result.isNull()) {
        QString filename(":image/addressbook/generic-personal-contact");

        if (nameTitle().isEmpty() && firstName().isEmpty()
                && middleName().isEmpty() && lastName().isEmpty()
                && suffix().isEmpty() && !company().isEmpty()) {
            filename = ":image/addressbook/generic-corporation-contact";
        } else if (d->mCategories.contains("Business")) { // no tr
            filename = ":image/addressbook/generic-contact";
        }

        QImageReader reader(filename);
        reader.setQuality( 49 ); // Otherwise Qt smooth scales
        reader.setScaledSize(portraitSize());
        QImage img = reader.read();
        if (!img.isNull())
            result = QPixmap::fromImage(img);
    }

    return result;
}

/*!
  Returns the size of the portrait pixmap of a contact.

  \sa portrait(), thumbnailSize()
*/
QSize QContact::portraitSize()
{
    static QSize size;

    if (!size.isValid()) {
        QDesktopWidget *desktop = QApplication::desktop();
        if (desktop) {
            int dpi = desktop->screen(desktop->primaryScreen())->logicalDpiY();
            // PortraitWidth x PortraitHeight at 100dpi
            size = QSize(PortraitWidth * dpi / 100, PortraitHeight * dpi / 100);
        } else {
            // We must not have constructed QApplication yet
            return QSize(PortraitWidth, PortraitHeight);
        }
    }

    return size;
}

// In pimrecord.cpp
void qpe_startVObjectInput();
void qpe_endVObjectInput();
void qpe_startVObjectOutput();
void qpe_setVObjectProperty(const QString&, const QString&, const char* type, QPimRecord*);
void qpe_endVObjectOutput(VObject *,const char* type,const QPimRecord*);
VObject *qpe_safeAddPropValue( VObject *o, const char *prop, const QString &value );
static inline VObject *safeAddPropValue( VObject *o, const char *prop, const QString &value )
{ return qpe_safeAddPropValue(o,prop,value); }
VObject *qpe_safeAddProp( VObject *o, const char *prop);
static inline VObject *safeAddProp( VObject *o, const char *prop)
{ return qpe_safeAddProp(o,prop); }


static void safeAddAddress( VObject* vcard, const char* prop, const QString& bs, const QString& bc,
    const QString& bst, const QString& bz, const QString& bco)
{
    // Not really needed now, since parse handles empty values
    if ( !bs.isEmpty() || !bc.isEmpty() || !bst.isEmpty()
            || !bz.isEmpty() || !bco.isEmpty() ) {
        VObject *adr= safeAddProp( vcard, VCAdrProp );
        safeAddProp( adr, prop );
        safeAddPropValue( adr, VCStreetAddressProp, bs );
        safeAddPropValue( adr, VCCityProp, bc );
        safeAddPropValue( adr, VCRegionProp, bst );
        safeAddPropValue( adr, VCPostalCodeProp, bz );
        safeAddPropValue( adr, VCCountryNameProp, bco );
    }
}

/*!
   \internal
*/
VObject *QContact::createVObject() const
{
    qpe_startVObjectOutput();

    VObject *vcard = newVObject( VCCardProp );
    safeAddPropValue( vcard, VCVersionProp, "2.1" );
    safeAddPropValue( vcard, VCLastRevisedProp, QDateTime::currentDateTime().toUTC().toString(Qt::ISODate) );
    // XXX might be better to use local context string here..
    safeAddPropValue( vcard, VCUniqueStringProp, uid().toString() );

    // full name
    safeAddPropValue( vcard, VCFullNameProp, label() );

    // name properties
    VObject *name = safeAddProp( vcard, VCNameProp );
    safeAddPropValue( name, VCFamilyNameProp, lastName() );
    safeAddPropValue( name, VCGivenNameProp, firstName() );
    safeAddPropValue( name, VCAdditionalNamesProp, middleName() );
    safeAddPropValue( name, VCNamePrefixesProp, nameTitle() );
    safeAddPropValue( name, VCNameSuffixesProp, suffix() );

    safeAddPropValue( vcard, VCPronunciationProp, firstNamePronunciation() );
    safeAddPropValue( vcard, "X-Qtopia-LNSOUND", lastNamePronunciation() );

    // home properties
    safeAddAddress( vcard, VCHomeProp, homeStreet(), homeCity(),
        homeState(), homeZip(), homeCountry() );

    VObject *home_phone = safeAddPropValue( vcard, VCTelephoneProp, homePhone() );
    safeAddProp( home_phone, VCHomeProp );
    home_phone = safeAddPropValue( vcard, VCTelephoneProp, homeMobile() );
    safeAddProp( home_phone, VCHomeProp );
    safeAddProp( home_phone, VCCellularProp );
    home_phone = safeAddPropValue( vcard, VCTelephoneProp, homeFax() );
    safeAddProp( home_phone, VCHomeProp );
    safeAddProp( home_phone, VCFaxProp );

    VObject *url = safeAddPropValue( vcard, VCURLProp, homeWebpage() );
    safeAddProp( url, VCHomeProp );

    // work properties
    safeAddAddress( vcard, VCWorkProp, businessStreet(), businessCity(),
        businessState(), businessZip(), businessCountry() );

    VObject *work_phone = safeAddPropValue( vcard, VCTelephoneProp, businessPhone() );
    safeAddProp( work_phone, VCWorkProp );
    work_phone = safeAddPropValue( vcard, VCTelephoneProp, businessMobile() );
    safeAddProp( work_phone, VCWorkProp );
    safeAddProp( work_phone, VCCellularProp );
    work_phone = safeAddPropValue( vcard, VCTelephoneProp, businessFax() );
    safeAddProp( work_phone, VCWorkProp );
    safeAddProp( work_phone, VCFaxProp );
    work_phone = safeAddPropValue( vcard, VCTelephoneProp, businessPager() );
    safeAddProp( work_phone, VCWorkProp );
    safeAddProp( work_phone, VCPagerProp );

    url = safeAddPropValue( vcard, VCURLProp, businessWebpage() );
    safeAddProp( url, VCWorkProp );

    VObject *title = safeAddPropValue( vcard, VCTitleProp, jobTitle() );
    safeAddProp( title, VCWorkProp );


    QStringList emails = emailList();
    //emails.prepend( defaultEmail() );
    for( QStringList::Iterator it = emails.begin(); it != emails.end(); ++it ) {
        VObject *email = safeAddPropValue( vcard, VCEmailAddressProp, *it );
        safeAddProp( email, VCInternetProp );
        if (*it == defaultEmail())
            safeAddProp( email, VCPreferredProp );
    }

    safeAddPropValue( vcard, VCNoteProp, notes() );

    safeAddPropValue( vcard, VCBirthDateProp, birthday().toString("yyyyMMdd"));


    if ( !company().isEmpty() || !department().isEmpty() || !office().isEmpty() ) {
        VObject *org = safeAddProp( vcard, VCOrgProp );
        safeAddPropValue( org, VCOrgNameProp, company() );
        safeAddPropValue( org, VCOrgUnitProp, department() );
        safeAddPropValue( org, VCOrgUnit2Prop, office() );
    }
    safeAddPropValue( vcard, "X-Qtopia-CSOUND", companyPronunciation() );

    //photo
    QString pfn = portraitFile();
    if ( !pfn.isEmpty() ) {
        pfn = resolvePortraitPath(pfn);

        const char* format = "JPEG";

        QByteArray buf;
        if (QImageReader(pfn, format).canRead()) {
            // In correct format so copy file losslessly.
            QFile f(pfn);
            if (f.open(QIODevice::ReadOnly))
                buf = f.readAll();
        } else {
            // load-and-save (allows non-JPEG images)
            QImage sp;
            if( !sp.load( pfn ) ) {
                qWarning("QContact::createVObject - Unable to load contact photo %s", pfn.toLatin1().constData());
            } else {
                QBuffer buffer;
                buffer.open(QIODevice::WriteOnly);
                if (sp.save(&buffer,format))
                    buf = buffer.buffer();
            }
        }
        if ( buf.size() ) {
            VObject *po = addPropSizedValue( vcard, VCPhotoProp, buf.data(), buf.size() );
            safeAddPropValue( po, "TYPE", format );
            // should really be done by vobject.cpp, since it decides to use base64
            // XXX The correct value may be "b", not "BASE64".
            safeAddPropValue( po, VCEncodingProp, "BASE64" );
        }
    }

    // some values we have to export as custom fields
    safeAddPropValue( vcard, "X-Qtopia-Profession", profession() );
    safeAddPropValue( vcard, "X-Qtopia-Manager", manager() );
    safeAddPropValue( vcard, "X-Qtopia-Assistant", assistant() );

    safeAddPropValue( vcard, "X-Qtopia-Spouse", spouse() );
    if ( gender() != QContact::UnspecifiedGender )
        safeAddPropValue( vcard, "X-Qtopia-Gender", QString::number( (int)gender() ) );
    safeAddPropValue( vcard, "X-Qtopia-Anniversary", anniversary().toString("yyyyMMdd"));
    safeAddPropValue( vcard, "X-Qtopia-Nickname", nickname() );
    safeAddPropValue( vcard, "X-Qtopia-Children", children() );

    qpe_endVObjectOutput(vcard, "Address Book", this); // No tr

    return vcard;
}

static QContact parseVObject( VObject *obj )
{
    QContact c;
//    int ds = 0;
//    char *photodata = 0;

    VObjectIterator it;
    initPropIterator( &it, obj );
    while( moreIteration( &it ) ) {
        VObject *o = nextVObject( &it );
        QString name = vObjectName( o );

        // check this key/value for a CHARSET field.
        VObjectIterator tnit;
        initPropIterator( &tnit, o );
        QTextCodec *tc = 0;
        while( moreIteration( &tnit ) ) {
            VObject *otc = nextVObject( &tnit );
            if ( qstrcmp(vObjectName(otc), VCCharSetProp ) == 0) {
                tc = QTextCodec::codecForName(vObjectStringZValue(otc));
                break;
            }
        }
        QString value;
        if (tc)
            value = tc->toUnicode( vObjectStringZValue( o ) );
        else
            value = vObjectStringZValue( o );

        if ( name == VCNameProp ) {
            VObjectIterator nit;
            initPropIterator( &nit, o );
            while( moreIteration( &nit ) ) {
                VObject *o = nextVObject( &nit );
                QString name = vObjectTypeInfo( o );

                if (tc) {
                    value = tc->toUnicode( vObjectStringZValue( o ) );
                }
                else
                    value = vObjectStringZValue( o );
                //QString value = vObjectStringZValue( o );
                if ( name == VCNamePrefixesProp )
                    c.setNameTitle( value );
                else if ( name == VCNameSuffixesProp )
                    c.setSuffix( value );
                else if ( name == VCFamilyNameProp ) {
                    c.setLastName( value );
                } else if ( name == VCGivenNameProp ) {
                    c.setFirstName( value );
                } else if ( name == VCAdditionalNamesProp )
                    c.setMiddleName( value );
            }
        }
        else if ( name == VCPronunciationProp )
            c.setFirstNamePronunciation( value );
        else if ( name == "X-Qtopia-LNSOUND" )
            c.setLastNamePronunciation( value );
        else if ( name == VCAdrProp ) {
            bool work = true; // default address is work address
            QString street;
            QString city;
            QString region;
            QString postal;
            QString country;

            VObjectIterator nit;
            initPropIterator( &nit, o );
            while( moreIteration( &nit ) ) {
                VObject *o = nextVObject( &nit );
                QString name = vObjectName( o );
                if (tc)
                    value = tc->toUnicode( vObjectStringZValue( o ) );
                else
                    value = vObjectStringZValue( o );
                //QString value = vObjectStringZValue( o );
                if ( name == VCHomeProp )
                    work = false;
                else if ( name == VCWorkProp )
                    work = true;
                else if ( name == VCStreetAddressProp )
                    street = value;
                else if ( name == VCCityProp )
                    city = value;
                else if ( name == VCRegionProp )
                    region = value;
                else if ( name == VCPostalCodeProp )
                    postal = value;
                else if ( name == VCCountryNameProp )
                    country = value;
            }
            if ( work ) {
                c.setBusinessStreet( street );
                c.setBusinessCity( city );
                c.setBusinessCountry( country );
                c.setBusinessZip( postal );
                c.setBusinessState( region );
            } else {
                c.setHomeStreet( street );
                c.setHomeCity( city );
                c.setHomeCountry( country );
                c.setHomeZip( postal );
                c.setHomeState( region );
            }
        }
        else if ( name == VCTelephoneProp ) {
            enum {
                HOME = 0x01,
                WORK = 0x02,
                VOICE = 0x04,
                CELL = 0x08,
                FAX = 0x10,
                PAGER = 0x20,
                UNKNOWN = 0x80
            };
            int type = 0;

            VObjectIterator nit;
            initPropIterator( &nit, o );
            while( moreIteration( &nit ) ) {
                VObject *o = nextVObject( &nit );
                QString name = vObjectTypeInfo( o );
                if ( name == VCHomeProp )
                    type |= HOME;
                else if ( name == VCWorkProp )
                    type |= WORK;
                else if ( name == VCVoiceProp )
                    type |= VOICE;
                else if ( name == VCCellularProp )
                    type |= CELL;
                else if ( name == VCFaxProp )
                    type |= FAX;
                else if ( name == VCPagerProp )
                    type |= PAGER;
                else if ( name == VCPreferredProp )
                    ;
                else if ( name == VCEncodingProp )
                    ;
                else
                    type |= UNKNOWN;
            }
            // evil if, indicates that if there was even one property we didn't know, don't store this value.
            if ( (type & UNKNOWN) == UNKNOWN ) {
                qWarning("found unknown attribute in vobject, %s", (const char *)name.toLocal8Bit());
            }

            if ( ( type & (HOME|WORK) ) == 0 ) // default
                type |= HOME;
            if ( ( type & (VOICE|CELL|FAX|PAGER) ) == 0 ) // default
                type |= VOICE;

            if ( (type & (VOICE|HOME) ) == (VOICE|HOME) )
                c.setHomePhone( value );
            if ( ( type & (FAX|HOME) ) == (FAX|HOME) )
                c.setHomeFax( value );
            if ( ( type & (CELL|HOME) ) == (CELL|HOME) )
                c.setHomeMobile( value );
            if ( ( type & (VOICE|WORK) ) == (VOICE|WORK) )
                c.setBusinessPhone( value );
            if ( ( type & (FAX|WORK) ) == (FAX|WORK) )
                c.setBusinessFax( value );
            if ( ( type & (CELL|WORK) ) == (CELL|WORK) )
                c.setBusinessMobile( value );
            if ( ( type & (PAGER|WORK) ) == (PAGER|WORK) )
                c.setBusinessPager( value );
        }
        else if ( name == VCEmailAddressProp ) {
            QString email;
            if (tc)
                email = tc->toUnicode( vObjectStringZValue( o ) );
            else
                email = vObjectStringZValue( o );
            //QString email = vObjectStringZValue( o );
            VObjectIterator nit;
            initPropIterator( &nit, o );
            bool isDefaultEmail = false;
            while( moreIteration( &nit ) ) {
                VObject *o = nextVObject( &nit );
                QString name = vObjectTypeInfo( o );
                if ( name == VCPreferredProp)
                    isDefaultEmail = true;
            }

            if ( isDefaultEmail )
                c.setDefaultEmail( email );
            else
                c.insertEmail( email );
        }
        else if ( name == VCURLProp ) {
            VObjectIterator nit;
            initPropIterator( &nit, o );
            while( moreIteration( &nit ) ) {
                VObject *o = nextVObject( &nit );
                QString name = vObjectTypeInfo( o );
                if ( name == VCHomeProp )
                    c.setHomeWebpage( value );
                else if ( name == VCWorkProp )
                    c.setBusinessWebpage( value );
            }
        }
        else if ( name == VCOrgProp ) {
            VObjectIterator nit;
            initPropIterator( &nit, o );
            while( moreIteration( &nit ) ) {
                VObject *o = nextVObject( &nit );
                QString name = vObjectName( o );
                if (tc)
                    value = tc->toUnicode( vObjectStringZValue( o ) );
                else
                    value = vObjectStringZValue( o );
                if ( name == VCOrgNameProp ) {
                    c.setCompany( value );
                } else if ( name == VCOrgUnitProp )
                    c.setDepartment( value );
                else if ( name == VCOrgUnit2Prop )
                    c.setOffice( value );
            }
        }
        //support for photos
        else if( name == VCPhotoProp )
        {
            VObject *po;
            VObjectIterator pit;
            initPropIterator( &pit, o );
            while( moreIteration( &pit ) )
            {
                po = nextVObject( &pit );

                QString pName( vObjectName( po ) );


                /* Don't care what type it is,
                   we're going to convert it to jpeg
                   in any case.
                if( pName == "TYPE" )
                    pmt = vObjectStringZValue( po );

                if( pName == VCDataSizeProp )
                    ds = vObjectIntegerValue( po );
                */

                //TODO : Is quoted printable encoding ever used?
            }

            c.setCustomField( "phototmp", QString(vObjectStringZValue( o )) );
            //actually save it in AddressBook::recieveFile when we know if it's a new contact
           // or a duplicate
        }
        else if ( name == "X-Qtopia-CSOUND" )
            c.setCompanyPronunciation( value );
        else if ( name == VCTitleProp ) {
            c.setJobTitle( value );
        }
        else if ( name == VCNoteProp ) {
            c.setNotes( value );
        }
        else if (name == VCBirthDateProp ) {
            c.setBirthday( QDate::fromString(value, "yyyyMMdd") );
        }
        else if ( name == "X-Qtopia-Profession" ) {
            c.setProfession( value );
        }
        else if ( name == "X-Qtopia-Manager" ) {
            c.setManager( value );
        }
        else if ( name == "X-Qtopia-Assistant" ) {
            c.setAssistant( value );
        }
        else if ( name == "X-Qtopia-Spouse" ) {
            c.setSpouse( value );
        }
        else if ( name == "X-Qtopia-Gender" ) {
            c.setGender( (QContact::GenderType) value.toInt() );
        }
        else if ( name == "X-Qtopia-Anniversary" ) {
            c.setAnniversary( QDate::fromString(value, "yyyyMMdd") );
        }
        else if ( name == "X-Qtopia-Nickname" ) {
            c.setNickname( value );
        }
        else if ( name == "X-Qtopia-Children" ) {
            c.setChildren( value );
        }
        else {
            qpe_setVObjectProperty(name,value,"Address Book",&c); // No tr
        }

#ifdef PRINT_UNRECOGNIZED_VCARD_DATA
        else {
            printf("Name: %s, value=%s\n", name.data(), vObjectStringZValue( o ) );
            VObjectIterator nit;
            initPropIterator( &nit, o );
            while( moreIteration( &nit ) ) {
                VObject *o = nextVObject( &nit );
                QString name = vObjectName( o );
                QString value = vObjectStringZValue( o );
                printf(" subprop: %s = %s\n", name.data(), value.toLatin1().constData() );
            }
        }
#endif
    }
    return c;
}

/*!
  Compares to \a other contact and returns true if it is equivalent.
  Otherwise returns false.
*/
bool QContact::operator==(const QContact &other) const
{
    // covers custom fields
    if (!QPimRecord::operator==(other))
        return false;

    QMap<int, QString>::ConstIterator it;
    for (it = d->mMap.begin(); it != d->mMap.end(); ++it)
    {
        if (!other.d->mMap.contains(it.key()) || other.d->mMap[it.key()] != it.value())
            return false;
    }

    // need to check both maps.
    for (it = other.d->mMap.begin(); it != other.d->mMap.end(); ++it)
    {
        if (!d->mMap.contains(it.key()))
            return false;
    }

    if (d->emails.count() != other.d->emails.count())
        return false;
    if (QSet<QString>::fromList(d->emails) != QSet<QString>::fromList(other.d->emails))
        return false;
    if (defaultEmail() != other.defaultEmail())
        return false;
    if (d->gender != other.d->gender)
        return false;
    if (d->birthday != other.d->birthday)
        return false;
    if (d->anniversary != other.d->anniversary)
        return false;

    if (d->defaultPhoneNumber != other.d->defaultPhoneNumber)
        return false;
    if (d->phoneNumbers != other.d->phoneNumbers)
        return false;
    if (d->address != other.d->address)
        return false;

    return true;
}

/*!
  Returns false if \a other is identical to this contact. Otherwise return true.
*/
bool QContact::operator!=(const QContact &other) const
{
    return !(*this == other);
}

/*!
  Writes the given list of \a contacts to the given \a device as vCards.

  Returns true on success.
  \sa readVCard()
*/
bool QContact::writeVCard( QIODevice *device, const QList<QContact> &contacts )
{
    foreach (QContact c, contacts)
        if (!writeVCard(device, c))
            return false;
    return true;
}

/*!
  Writes the given \a contact to the given \a device as vCards.

  Returns true on success.
  \sa readVCard()
*/
bool QContact::writeVCard( QIODevice *device, const QContact &contact )
{
    VObject *obj = contact.createVObject();
    writeVObject( device, obj );
    cleanVObject( obj );
    cleanStrTbl();
    return true;
}

/*!
  Reads a list of vCards from the given \a device and returns the
  equivalent set of contacts.

  \sa writeVCard()
*/
QList<QContact> QContact::readVCard( QIODevice *device )
{
    QList<QContact> contacts;

    QBuffer *buffer = qobject_cast<QBuffer *>(device);
    QFile *file = qobject_cast<QFile *>(device);
    if (file) {
        int handle = file->handle();
        FILE *input = fdopen(handle, "r");
        if (input) {
            q_DontDecodeBase64Photo++;
            contacts = readVCard( Parse_MIME_FromFile( input ) );
            q_DontDecodeBase64Photo--;
        }
    } else if (buffer) {
        q_DontDecodeBase64Photo++;
        contacts = readVCard( Parse_MIME( (const char*)buffer->data(), buffer->data().count() ) );
        q_DontDecodeBase64Photo--;
    } else {
        const QByteArray bytes = device->readAll();
        q_DontDecodeBase64Photo++;
        contacts = readVCard( Parse_MIME( (const char*)bytes, bytes.count() ) );
        q_DontDecodeBase64Photo--;
    }

    return contacts;
}

/*!
  \deprecated
   Write the list of \a contacts as vCard objects to the file
   specified by \a filename.

   Returns true on success, false on failure.

   \sa readVCard()
*/
bool QContact::writeVCard( const QString &filename, const QList<QContact> &contacts)
{
    QFile f( filename );
    if( ! f.open( QIODevice::WriteOnly ) ) {
        qWarning( "Unable to open vcard file for write!" );
        return false;
    }
    return writeVCard( &f, contacts);
}

/*!
  \deprecated
   Writes the contact as a vCard object to the file specified
   by \a filename.

   Returns true on success, false on failure.

   \sa readVCard()
*/
bool QContact::writeVCard( const QString &filename ) const
{
    QFile f( filename );
    if( ! f.open( QIODevice::WriteOnly ) ) {
        qWarning( "Unable to open vcard file for write!" );
        return false;
    }
    return writeVCard( &f, *this);
}

/*!
  \deprecated

   Writes the contact as a vCard object to the given \a file,
   which must be already open for writing.

   \sa readVCard()
*/
void QContact::writeVCard( QFile &file ) const
{
    writeVCard( &file, *this );
}

/*!
  \deprecated

   Writes the contact as a vCard object to the given \a stream,
   which must be writable.

   \sa readVCard()
*/
void QContact::writeVCard( QDataStream *stream ) const
{
    writeVCard(stream->device(), *this);
}

/*!
  \deprecated

  Reads the file specified by \a filename as a list of vCard objects
  and returns the list of contacts that correspond to the vCard objects.

  Note:  the fields stored by QContact may not correspond exactly to the vCard data.

  \sa writeVCard()
*/
QList<QContact> QContact::readVCard( const QString &filename )
{
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly))
        return readVCard(&file);

    return QList<QContact>();
}

/*!
  \deprecated

  Reads the given vCard data in \a vcard, and returns the list of
  corresponding contacts.

  Note:  the fields stored by QContact may not correspond exactly to the vCard data.

  \sa writeVCard()
*/
QList<QContact> QContact::readVCard( const QByteArray &vcard )
{
    QBuffer buffer;
    buffer.setData(vcard.constData(), vcard.size());
    return readVCard(&buffer);
}

/*!
  Reads the given vCard data in \a vobject, and returns the list of
  corresponding contacts.

  Note:  the fields stored by QContact may not correspond exactly to the vCard data.

  \sa writeVCard()
*/
QList<QContact> QContact::readVCard( VObject* vobject )
{
    QList<QContact> contacts;

    q_DontDecodeBase64Photo++;
    //qpe_setNameFormatCache(true);
    qpe_startVObjectInput();
    while ( vobject ) {
        contacts.append( parseVObject( vobject ) );

        VObject *t = vobject;
        vobject = nextVObjectInList(vobject);
        cleanVObject( t );
    }
    qpe_endVObjectInput();
    //qpe_setNameFormatCache(false);
    q_DontDecodeBase64Photo--;

    return contacts;
}

/*!
  \overload
  Returns true if one of this contact's fields matches the given \a expression.

  The function does not compare the gender, unique identifier, birthday or anniversary of the contact.
*/
bool QContact::match( const QString &expression ) const
{
    return match(QRegExp(expression));
}

/*!
  Returns true if one of this contact's fields matches the given \a expression.

  The function does not compare the gender, unique identifier, birthday or anniversary of the contact.
*/
bool QContact::match( const QRegExp &expression ) const
{
    QMap<int, QString>::ConstIterator it;
    for ( it = d->mMap.begin(); it != d->mMap.end(); ++it ) {
        if ( (*it).contains( expression ) )
            return true;
    }
    QListIterator<QString> eit(d->emails);
    while(eit.hasNext()) {
        if (eit.next().contains(expression))
            return true;
    }
    QMapIterator<QContact::PhoneType, QString> pit(d->phoneNumbers);
    while(pit.hasNext()) {
        pit.next();
        if (pit.value().contains(expression))
            return true;
    }
    QMapIterator<QContact::Location, QContactAddress> ait(d->address);
    while(ait.hasNext()) {
        ait.next();
        QContactAddress address = ait.value();
        if (address.street.contains(expression))
            return true;
        if (address.city.contains(expression))
            return true;
        if (address.state.contains(expression))
            return true;
        if (address.zip.contains(expression))
            return true;
        if (address.country.contains(expression))
            return true;
    }
    QListIterator<QString> cit(d->mCategories);
    while(cit.hasNext()) {
        if (cit.next().contains(expression))
            return true;
    }
    if (notes().contains(expression))
        return true;
    return false;
}

/*!
  \reimp
*/
QUniqueId &QContact::uidRef() { return d->mUid; }

/*!
  \reimp
*/
const QUniqueId &QContact::uidRef() const { return d->mUid; }

/*!
  \reimp
*/
QList<QString> &QContact::categoriesRef() { return d->mCategories; }

/*!
  \reimp
*/
const QList<QString> &QContact::categoriesRef() const { return d->mCategories; }

/*!
  \reimp
*/
QMap<QString, QString> &QContact::customFieldsRef() { return d->customMap; }

/*!
  \reimp
*/
const QMap<QString, QString> &QContact::customFieldsRef() const { return d->customMap; }

/*!
    Sets the path for the portrait file of the contact to \a str.

    The string is interpreted as follows:
    - if the path starts with a ':' character, it is interpreted as a
    Qt Resource system path
    - otherwise, the string will be interpreted as a path relative to
    the "contactimages" subdirectory of the "addressbook" application
    storage directory (retrieved using Qtopia::applicationFileName).
*/
void QContact::setPortraitFile( const QString &str )
{
    replace(Portrait, str);
}


/*!
  \internal
  Removes the existing portrait file and clears pixmap caches
*/
void QContact::removeExistingPortrait()
{
    QString photoFile = portraitFile();
    QString baseDir = Qtopia::applicationFileName( "addressbook", "contactimages/" );

    QDir dir( baseDir );
    if( !dir.exists() )
        dir.mkdir( baseDir );

    // We don't delete files that don't live under our base directory..
    // so we don't go through resolvePortraitFile
    if( !photoFile.isEmpty()  && !photoFile.startsWith(QChar(':'))) {
        QFile pFile( baseDir + photoFile );
        if( pFile.exists() )
            pFile.remove();
    }

    // Clear out any cached pixmaps
    QString key("pimcontact" + uid().toString() + "-cfl");
    QPixmapCache::remove(key);
    QPixmapCache::remove(key + "-thumb");
}

/*!
  \internal
  Saves the given \a image as the portrait.  Assumes the image has already
  been scaled appropriately.
*/
void QContact::saveScaledPortrait(const QImage &scaled)
{
    QString extension;
    const char *format;
    if (scaled.format() == QImage::Format_ARGB32_Premultiplied
            || scaled.format() == QImage::Format_ARGB32 || scaled.format() == QImage::Format_RGB16)
    {
        extension = QLatin1String(".png");
        format = "PNG";
    } else {
        extension = QLatin1String(".jpg");
        format = "JPEG";
    }

    // Always generate a new name for the file, since it could be a
    // different file type.
    QString baseFileName = "ci-" + QString::number( ::time(0) ) + "-";
    int i = 0;
    QFile pFile;

    QString photoFile;
    QString baseDir = Qtopia::applicationFileName( "addressbook", "contactimages/" );
    do {
        photoFile = baseFileName + QString::number( i++ ) + extension;
        pFile.setFileName( baseDir + photoFile );
    } while( pFile.exists() );

    //  Save the image, scaled to the maximum image size
    if( scaled.save( baseDir + photoFile, format, 50 ) ) {
        setPortraitFile( photoFile );
        return;
    } else {
        qWarning("Unable to store contact photo to: %s", (const char *)photoFile.toLocal8Bit());
    }
}

/*!
  Changes the contacts portrait to the given pixmap \a p. The pixmap is saved
  to the contactimages directory in the Qtopia "addressbook" application
  storage directory, and removes the file previously used as the contact's
  portrait.  The corresponding filename will be set as the \c Portrait field of this contact.

  Note: the previous image is removed immediately if it is in the "addressbook"
  application data storage area, but the database will not
  be updated until \c QContactModel::updateContact is called.
*/
void QContact::changePortrait( const QPixmap &p )
{
    changePortrait(p.toImage());
}

/*!
  Changes the contacts portrait to the given \a image. The image is saved
  to the contactimages directory in the Qtopia "addressbook" application
  storage directory, and removes the file previously used as the contact's
  portrait.  The corresponding filename will be set as the \c Portrait field of this contact.

  Note: the previous image is removed immediately if it is in the "addressbook"
  application data storage area, but the database will not
  be updated until \c QContactModel::updateContact is called.
*/
void QContact::changePortrait( const QImage &image )
{
    removeExistingPortrait();

    if( !image.isNull() ) {
        // only scale the image if its bigger than the recommended portrait size.
        if (image.size().boundedTo(portraitSize()) != image.size())
            saveScaledPortrait(image.scaled( portraitSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation ));
        else
            saveScaledPortrait(image);
    } else {
        setPortraitFile( QString() );
    }
}

/*!
  Changes the contacts portrait to the image at the given \a imageLocation.
  The image is loaded, scaled and then saved to the contactimages directory
  in the Qtopia "addressbook" application storage directory.  If the
  specified clip rectangle \a rect is not null, only the clip area of the
  image specified is used.  The file previously used as the contact's
  portrait is removed.  The corresponding filename will be set as
  the \c Portrait field of this contact.

  Note: the previous image is removed immediately if it is in the "addressbook"
  application data storage area, but the database will not
  be updated until \c QContactModel::updateContact is called.
*/
void QContact::changePortrait( const QString &imageLocation, const QRect &rect )
{
    removeExistingPortrait();

    QImageReader reader(imageLocation);

    QSize imageSize = reader.size();
    if (rect.isNull())
        reader.setClipRect(rect);

    // only scale the image if its bigger than the recommended portrait size.
    if (imageSize.isNull())
        imageSize = portraitSize();
    else if (imageSize.boundedTo(portraitSize()) != imageSize) {
        imageSize.scale(portraitSize(), Qt::KeepAspectRatio);
        reader.setQuality( 49 ); // Otherwise Qt smooth scales
    }
    reader.setScaledSize(portraitSize());

    QImage image = reader.read();

    if( !image.isNull() )
        saveScaledPortrait(image);
    else
        setPortraitFile( QString() );
}

/*!
  Parses the given \a text using contact name formatting rules and merges the
  information with \a contact.  Returns the merged contact.

  Formatting rules include using () for the nick name, "" for pronunciation,
  standard name titles and suffixes as well as detecting first, middle and last name ordering,
  similar to the default label formatting.

  \sa label()
*/
QContact QContact::parseLabel(const QString &text, const QContact &contact)
{
    //QRegExp pronunciationRE("\\s*\\((?!\\(+)\\)\\s*");
    //QRegExp nickRE("\\s*\"(?!\"+)\"\\s*");
    static QRegExp pronunciationRE("\\((.+)\\)");
    pronunciationRE.setMinimal(true);
    static QRegExp nickRE("\"(.+)\"");
    nickRE.setMinimal(true);


    static QRegExp fmlRE("^\\s*(\\S+)\\s+(\\S+)\\s+(\\S+)\\s*$");
    static QRegExp lfmRE("^\\s*(\\S+),\\s+(\\S+)\\s+(\\S+)\\s*$");
    static QRegExp flRE("^\\s*(\\S+)\\s+(\\S+)\\s*$");
    static QRegExp lfRE("^\\s*(\\S+),\\s+(\\S+)\\s*$");

    QContact c = contact;
    QString label = text;

    /* PRONUNCIATION */
    int index = pronunciationRE.indexIn(label);
    if (index != -1) {
        // remove from label, QString::replace replaces all, not first.  We are only interested in one match.
        label = label.left(index) + " " + label.mid(index+pronunciationRE.matchedLength());
        QString p = pronunciationRE.cap(1);
        if (lfRE.indexIn(p) != -1) {
            c.setLastNamePronunciation(lfRE.cap(1));
            c.setFirstNamePronunciation(lfRE.cap(2));
        } else if (flRE.indexIn(p) != -1) {
            c.setFirstNamePronunciation(flRE.cap(1));
            c.setLastNamePronunciation(flRE.cap(2));
        } else {
            c.setFirstNamePronunciation(p);
        }
    }

    /* NICKNAME */
    index = nickRE.indexIn(label);
    if (index != -1) {
        label = label.left(index) + " " + label.mid(index+nickRE.matchedLength());
        c.setNickname(nickRE.cap(1).simplified());
    }

    /* NAME TITLE AND SUFFIX */
    static QList<QRegExp *> prefixesRE;
    static QList<QRegExp *> suffixesRE;
    static QStringList prefixes(QContactModel::localeNameTitles());
    static QStringList suffixes(QContactModel::localeSuffixes());

    if (prefixesRE.count() != prefixes.count()) {
        foreach(QString affix, prefixes) {
            QRegExp *re = new QRegExp("^\\s*\\b"+QRegExp::escape(affix)+"\\b\\.?\\s+");
            re->setCaseSensitivity(Qt::CaseInsensitive);
            prefixesRE.append(re);
        }
    }
    if (suffixesRE.count() != suffixes.count()) {
        foreach(QString affix, suffixes) {
            QRegExp *re = new QRegExp("\\b"+QRegExp::escape(affix)+"\\b\\.?");
            re->setCaseSensitivity(Qt::CaseInsensitive);
            suffixesRE.append(re);
        }
    }

    int i;
    for (i = 0; i < prefixesRE.count(); i++) {
        index = prefixesRE[i]->indexIn(label);
        if (index != -1) {
            QString affix = prefixes[i];
            label = label.left(index) + " " + label.mid(index+prefixesRE[i]->matchedLength());
            c.setNameTitle(affix);
            break;
        }
    }
    for (i = 0; i < suffixesRE.count(); i++) {
        index = suffixesRE[i]->indexIn(label);
        if (index != -1) {
            QString affix = suffixes[i];
            label = label.left(index) + " " + label.mid(index+suffixesRE[i]->matchedLength());
            c.setSuffix(affix);
            break;
        }
    }

    /* LAST, MIDDLE AND FIRSTNAME */
    if (lfmRE.indexIn(label) != -1) {
        c.setLastName(lfmRE.cap(1));
        c.setFirstName(lfmRE.cap(2));
        c.setMiddleName(lfmRE.cap(3));
    } else if (lfRE.indexIn(label) != -1) {
        c.setLastName(lfRE.cap(1));
        c.setFirstName(lfRE.cap(2));
    } else if (fmlRE.indexIn(label) != -1) {
        c.setFirstName(fmlRE.cap(1));
        c.setMiddleName(fmlRE.cap(2));
        c.setLastName(fmlRE.cap(3));
    } else if (flRE.indexIn(label) != -1) {
        c.setFirstName(flRE.cap(1));
        c.setLastName(flRE.cap(2));
    } else {
        // don't throw away name if you can't parse it.
        // if we got one name and a title, assume surname
        if (c.nameTitle().isEmpty())
            c.setFirstName(label.simplified());
        else
            c.setLastName(label.simplified());
    }

    return c;
}

/*!
    \fn void QContact::deserialize(Stream &value)

    \internal

    Deserializes the QContact instance out to a template
    type \c{Stream} \a stream.
 */

template <typename Stream> void QContact::deserialize(Stream &s)
{
    s >> d->mUid;
    s >> d->mCategories;
    s >> d->customMap;
    s >> d->emails;
    s >> d->gender;
    s >> d->birthday;
    s >> d->anniversary;
    s >> d->mMap;
}

/*!
    \fn void QContact::serialize(Stream &value) const

    \internal

    Serializes the QContact instance out to a template
    type \c{Stream} \a stream.
 */

template <typename Stream> void QContact::serialize(Stream &s) const
{
    s << d->mUid;
    s << d->mCategories;
    s << d->customMap;
    s << d->emails;
    s << d->gender;
    s << d->birthday;
    s << d->anniversary;
    s << d->mMap;
}

/*!
  \enum QContact::Location

  Enumerates the types of addresses a contact can have.

  \value Home
    Residential contact address
  \value Business
    Business contact address
  \value Other
    Contact address that does not fit into other Location types
*/


/*!
  \enum QContact::PhoneType

  Enumerates the types of phone numbers that can be assigned to a
  contact.

  \value HomePhone
    A non-business land line.
  \value HomeMobile
    A non-business mobile phone number.
  \value HomeFax
    A non-business fax number.
  \value HomePager
    A non-business pager number.
  \value HomeVOIP
    A non-business VOIP URL
  \value BusinessPhone
    A business land line.
  \value BusinessMobile
    A business mobile phone number.
  \value BusinessFax
    A business fax number.
  \value BusinessPager
    A business pager number.
  \value BusinessVOIP
    A business VOIP URL
  \value OtherPhone
    A land line.  Whether the phone number is for Home or Business use
    is unspecified.
  \value Mobile
    A mobile phone number.  Whether the phone is for Home or Business
    use is unspecified.
  \value Fax
    A fax number.  Whether the phone is for Home or Business
    use is unspecified.
  \value Pager
    A pager number.  Whether the phone is for Home or Business
    use is unspecified.
  \value VOIP
    A VOIP URL.  Whether the URL is for Home or Business
    use is unspecified.
*/

/*! \fn void QContact::setNameTitle( const QString &str )
  Sets the title of the contact to \a str.
*/

/*! \fn void QContact::setFirstName( const QString &str )
  Sets the first name of the contact to \a str.
*/

/*! \fn void QContact::setMiddleName( const QString &str )
  Sets the middle name of the contact to \a str.
*/

/*! \fn void QContact::setLastName( const QString &str )
  Sets the last name of the contact to \a str.
*/

/*! \fn void QContact::setFirstNamePronunciation( const QString &str )
  Sets the pronunciation of the contact's first name to \a str.
*/

/*!
  \fn QString QContact::firstNamePronunciation() const
  Returns the pronunciation of the contact's first name.
*/

/*! \fn void QContact::setLastNamePronunciation( const QString &str )
  Sets the pronunciation of the contact's last name to \a str.
*/

/*!
  \fn QString QContact::lastNamePronunciation() const
  Returns the pronunciation of the contact's last name.
*/

/*! \fn void QContact::setCompanyPronunciation( const QString &str )
  Sets the pronunciation of the contact's company name to \a str.
*/

/*!
  \fn QString QContact::companyPronunciation() const
  Returns the pronunciation of the contact's company name.
*/

/*! \fn void QContact::setSuffix( const QString &str )
  Sets the suffix of the contact to \a str.
*/

/*!
  Sets the home street address of the contact to \a str.
*/
void QContact::setHomeStreet( const QString &str )
{
    d->setAddress(Home, QContactData::Street, str);
}

/*!
  Sets the home city of the contact to \a str.
*/
void QContact::setHomeCity( const QString &str )
{
    d->setAddress(Home, QContactData::City, str);
}


/*!
  Sets the home state of the contact to \a str.
*/
void QContact::setHomeState( const QString &str )
{
    d->setAddress(Home, QContactData::State, str);
}


/*!
  Sets the home zip code of the contact to \a str.
*/
void QContact::setHomeZip( const QString &str )
{
    d->setAddress(Home, QContactData::Zip, str);
}


/*!
  Sets the home country of the contact to \a str.
*/
void QContact::setHomeCountry( const QString &str )
{
    d->setAddress(Home, QContactData::Country, str);
}


/*! \fn void QContact::setHomePhone( const QString &str )
  Sets the home phone number of the contact to \a str.
*/

/*! \fn void QContact::setHomeFax( const QString &str )
  Sets the home fax number of the contact to \a str.
*/

/*! \fn void QContact::setHomeMobile( const QString &str )
  Sets the home mobile phone number of the contact to \a str.
*/

/*! \fn void QContact::setHomeVOIP( const QString &str )
  Sets the home VOIP URL of the contact to \a str.  This should
  be in the form of a URL (e.g. 'sip:user@sipserver').
*/

/*! \fn void QContact::setHomeWebpage( const QString &str )
  Sets the home webpage of the contact to \a str.
*/

/*! \fn void QContact::setCompany( const QString &str )
  Sets the company for contact to \a str.
*/

/*! \fn void QContact::setJobTitle( const QString &str )
  Sets the job title of the contact to \a str.
*/

/*! \fn void QContact::setDepartment( const QString &str )
  Sets the department for contact to \a str.
*/

/*! \fn void QContact::setOffice( const QString &str )
  Sets the office for contact to \a str.
*/

/*!
  Sets the business street address of the contact to \a str.
*/
void QContact::setBusinessStreet( const QString &str )
{
    d->setAddress(Business, QContactData::Street, str);
}


/*!
  Sets the business city of the contact to \a str.
*/
void QContact::setBusinessCity( const QString &str )
{
    d->setAddress(Business, QContactData::City, str);
}

/*!
  Sets the business state of the contact to \a str.
*/
void QContact::setBusinessState( const QString &str )
{
    d->setAddress(Business, QContactData::State, str);
}


/*!
  Sets the business zip code of the contact to \a str.
*/
void QContact::setBusinessZip( const QString &str )
{
    d->setAddress(Business, QContactData::Zip, str);
}


/*!
  Sets the business country of the contact to \a str.
*/
void QContact::setBusinessCountry( const QString &str )
{
    d->setAddress(Business, QContactData::Country, str);
}


/*! \fn void QContact::setBusinessPhone( const QString &str )
  Sets the business phone number of the contact to \a str.
*/

/*! \fn void QContact::setBusinessFax( const QString &str )
  Sets the business fax number of the contact to \a str.
*/

/*! \fn void QContact::setBusinessVOIP( const QString &str )
  Sets the business VOIP URL of the contact to \a str.  This should
  be in the form of a URL (e.g. 'sip:user@sipserver').
*/

/*! \fn void QContact::setBusinessMobile( const QString &str )
  Sets the business mobile phone number of the contact to \a str.
*/

/*! \fn void QContact::setBusinessPager( const QString &str )
  Sets the business pager number of the contact to \a str.
*/

/*! \fn void QContact::setBusinessWebpage( const QString &str )
  Sets the business webpage of the contact to \a str.
*/

/*! \fn void QContact::setProfession( const QString &str )
  Sets the profession of the contact to \a str.
*/

/*! \fn void QContact::setAssistant( const QString &str )
  Sets the assistant of the contact to \a str.
*/

/*! \fn void QContact::setManager( const QString &str )
  Sets the manager of the contact to \a str.
*/

/*! \fn void QContact::setSpouse( const QString &str )
  Sets the spouse of the contact to \a str.
*/

/*! \fn void QContact::setNickname( const QString &str )
  Sets the nickname of the contact to \a str.
*/

/*! \fn void QContact::setChildren( const QString &str )
  Sets the children of the contact to \a str.
*/

/*!
  Sets the notes about the contact to \a str.
*/
void QContact::setNotes( const QString &str )
{
    replace( Notes, str );
}

/*! \fn QString QContact::nameTitle() const
  Returns the title of the contact (e.g. Dr., Prof.)
*/

/*! \fn QString QContact::firstName() const
  Returns the first name of the contact.
*/

/*! \fn QString QContact::middleName() const
  Returns the middle name of the contact.
*/

/*! \fn QString QContact::lastName() const
  Returns the last name of the contact.
*/

/*! \fn QString QContact::suffix() const
  Returns the suffix of the contact.
*/

/*! \fn QString QContact::defaultEmail() const
  Returns the default email address of the contact.
*/

/*! \fn QString QContact::homeStreet() const
  Returns the home street address of the contact.
*/

/*! \fn QString QContact::homeCity() const
  Returns the home city of the contact.
*/

/*! \fn QString QContact::homeState() const
  Returns the home state of the contact.
*/

/*! \fn QString QContact::homeZip() const
  Returns the home zip of the contact.
*/

/*! \fn QString QContact::homeCountry() const
  Returns the home country of the contact.
*/

/*! \fn QString QContact::homePhone() const
  Returns the home phone number of the contact.
*/

/*! \fn QString QContact::homeFax() const
  Returns the home fax number of the contact.
*/

/*! \fn QString QContact::homeMobile() const
  Returns the home mobile number of the contact.
*/

/*! \fn QString QContact::homeVOIP() const
  Returns the home VOIP URL of the contact.
*/

/*! \fn QString QContact::homeWebpage() const
  Returns the home webpage of the contact.
*/

/*! \fn QString QContact::company() const
  Returns the company for the contact.
*/

/*! \fn QString QContact::department() const
  Returns the department for the contact.
*/

/*! \fn QString QContact::office() const
  Returns the office for the contact.
*/

/*! \fn QString QContact::jobTitle() const
  Returns the job title of the contact.
*/

/*! \fn QString QContact::profession() const
  Returns the profession of the contact.
*/

/*! \fn QString QContact::assistant() const
  Returns the assistant of the contact.
*/

/*! \fn QString QContact::manager() const
  Returns the manager of the contact.
*/

/*! \fn QString QContact::businessStreet() const
  Returns the business street address of the contact.
*/

/*! \fn QString QContact::businessCity() const
  Returns the business city of the contact.
*/

/*! \fn QString QContact::businessState() const
  Returns the business state of the contact.
*/

/*! \fn QString QContact::businessZip() const
  Returns the business zip of the contact.
*/

/*! \fn QString QContact::businessCountry() const
  Returns the business country of the contact.
*/

/*! \fn QString QContact::businessPhone() const
  Returns the business phone number of the contact.
*/

/*! \fn QString QContact::businessFax() const
  Returns the business fax number of the contact.
*/

/*! \fn QString QContact::businessMobile() const
  Returns the business mobile number of the contact.
*/

/*! \fn QString QContact::businessVOIP() const
  Returns the business VOIP URL of the contact.
*/

/*! \fn QString QContact::businessPager() const
  Returns the business pager number of the contact.
*/

/*! \fn QString QContact::businessWebpage() const
  Returns the business webpage of the contact.
*/

/*! \fn QString QContact::spouse() const
  Returns the spouse of the contact.
*/

/*! \fn QString QContact::nickname() const
  Returns the nickname of the contact.
*/

/*! \fn QString QContact::children() const
  Returns the children of the contact.
*/

/*! \fn QString QContact::portraitFile() const
  Returns the path for the portrait file of the contact.
  This is either a resource file path (e.g. \c ":icon/happyface") or
  the name of a file that is stored in the \c "addressbook/contactimages"
  directory in the Qt Extended application data storage directory.
*/

/*! \fn QString QContact::notes() const
  Returns the notes relating to the the contact.
*/

/*!
   \class QContactAddress
    \inpublicgroup QtUiModule
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtTelephonyModule
    \inpublicgroup QtPimModule

   \ingroup pim
   \brief The QContactAddress class contains an address of a QContact.

   The address is split into a number of fields:
   \list
   \o street address
   \o city
   \o state
   \o zip or postal code
   \o country
   \endlist

   It also provides two functions - \l {QContactAddress::}{isEmpty()} and
   \l {QContactAddress::}{operator==()}.

   \sa QContact, {Pim Library}
*/

/*!
  \fn bool QContactAddress::isEmpty() const
  Returns true if this address is empty (all the fields are empty),
  and false otherwise.
*/

/*!
  \fn bool QContactAddress::operator==(const QContactAddress& other) const
  Returns true if this address is the same as the \a other address,
  and false otherwise.
*/

/*!
  \variable QContactAddress::street
  \brief the street address portion of this address
  This would typically be something like "123 Penny Lane" or
  "Level 5, 123 Wharf St."
*/

/*!
  \variable QContactAddress::city
  \brief the city portion of this address
*/

/*!
  \variable QContactAddress::state
  \brief the state portion of this address
  This would typically be something like "MA", or "Queensland".
*/

/*!
  \variable QContactAddress::zip
  \brief the postal code portion of this address
  This would typically be something like "90210" or "SW1234".
*/

/*!
  \variable QContactAddress::country
  \brief the country portion of this address.
*/

Q_IMPLEMENT_USER_METATYPE(QContact)
