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
#include <qtimezone.h>
#include <qtopiaapplication.h>
#include <custom.h>

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QHash>
#include <QRegExp>
#include <QSettings>
#include <QTranslator>
#include <QtGlobal>
#include "qconstcstring.h"

#include <stdlib.h>
#include <time.h>


/*******************************************************************
 *
 * TimeZonePrivate
 *
 *******************************************************************/

class TimeZoneLocation;
class TimeZoneData;
class TzCache;

class TimeZonePrivate
{
    mutable TimeZoneLocation *loc;
    mutable TimeZoneData *dat;
    QString ident;

public:
    TimeZonePrivate( QString idLocation ) : loc(0), dat(0), ident(idLocation)
    {
    }

    const QString& id() const { return ident; }
    void setId(const QString id) { ident=id; loc=0; dat=0; }

    TimeZoneLocation *location() const;
    TimeZoneData *data() const;

    static QString zonePath();
    static QString zoneFile();

    static QDateTime setUtcTime( time_t t )
    {
        QDateTime r;
        tm *tM = gmtime( &t );
        if (tM) {
            r.setDate( QDate( tM->tm_year + 1900, tM->tm_mon + 1, tM->tm_mday ) );
            r.setTime( QTime( tM->tm_hour, tM->tm_min, tM->tm_sec ) );
        } else {
            r.setDate(QDate(1970, 1, 1));
            r.setTime(QTime(0,0,0));
        }
        r.setTimeSpec(Qt::UTC);

        return r;
    }

private:
    static QString sZonePath;
    static QString sZoneFile;

};


QString TimeZonePrivate::sZonePath(0);
QString TimeZonePrivate::zonePath()
{
    if ( sZonePath.isNull() ) {
#if defined(QTOPIA_ZONEINFO_PATH)
        sZonePath = QTOPIA_ZONEINFO_PATH;
#elif !defined(Q_OS_MAC)
        sZonePath = "/usr/share/zoneinfo/";
#else
        sZonePath = Qtopia::qtopiaDir() + "etc/zoneinfo/";
#endif
    }
    return sZonePath;
}

QString TimeZonePrivate::sZoneFile(0);
QString TimeZonePrivate::zoneFile()
{
    if ( sZoneFile.isNull() ) {
        sZoneFile = zonePath() + "zone.tab";
    }
    return sZoneFile;
}


/*******************************************************************
 *
 * TimeZoneData
 *
 *******************************************************************/

class TimeZoneData
{
public:
    TimeZoneData() : mId(0) { }
    TimeZoneData( const QString &id );

    bool matchAbbrev( const QDateTime &t,
        QString standardAbbrev,
        QString daylightAbbrev ) const;

    bool matchTime( const QDateTime &t,
        long utcOffset, bool isdst ) const;

    bool isValid() const;
    bool isDaylightSavings( const QDateTime & ) const;
    QDateTime toUtc( const QDateTime & ) const;
    QDateTime fromUtc( const QDateTime & ) const;
    QString id() const { return mId; }
    QString standardAbbreviation() const;
    QString dstAbbreviation() const;

    QString tzSetFormat(int stdIndex, int dstStartIndex, int dstEndIndex) const;
    QString tzSetFormat(const QDateTime &) const;

    static TimeZoneData *null;

private:
    /** find the transition time that should apply to \a t
    */
    int findTranstionTimeIndex( const QDateTime & t, bool utc ) const;
    int findTimeTypeIndex( const QDateTime & t, bool utc ) const;

    QString mId;
    bool mDstRule;

    struct transitionInfo {
        QDateTime time;
        int timeTypeIndex;
    };

    struct ttinfo {
        qint32       utcOffset; // in secs
        bool          isDst;
        unsigned int  abbreviationIndex;
        bool          isWallTime;
        bool          isTransitionLocal;
    };

    QVector<transitionInfo> transitionTimes;
    QVector<ttinfo> timeTypes;
    QMap<uint, QString> abbreviations;
};

TimeZoneData *TimeZoneData::null = new TimeZoneData();

bool TimeZoneData::matchAbbrev( const QDateTime &c,
                          QString standardAbbrev,
                          QString daylightAbbrev ) const
{
    if ( !isValid() )
        return false;

    ttinfo transInfo = timeTypes[ findTimeTypeIndex( c, false ) ];

    if ( mDstRule )
        return ( daylightAbbrev == abbreviations[ transInfo.abbreviationIndex ] );

    return ( standardAbbrev == abbreviations[ transInfo.abbreviationIndex ] );
}


bool TimeZoneData::matchTime( const QDateTime &c,
                          long utcOffset, bool isdst ) const
{
    if ( !isValid() )
        return false;

    ttinfo transInfo = timeTypes[ findTimeTypeIndex( c, false ) ];

    return transInfo.isDst == isdst && utcOffset == transInfo.utcOffset;
}


bool TimeZoneData::isValid() const
{
    return (transitionTimes.count() > 0) || (timeTypes.count() > 0);
}

int TimeZoneData::findTranstionTimeIndex( const QDateTime & c, bool utc ) const
{
    QDateTime ct = c;
    ct.setTimeSpec(Qt::UTC);
    for ( int i = 0; i < (int)transitionTimes.count(); ++i ) {
        transitionInfo transTime = transitionTimes[i];
        ttinfo transInfo = timeTypes[ transTime.timeTypeIndex ];
        QDateTime thisItTime = transTime.time;

        // if comparing with utc, then shift the it time to utc
        if ( !utc )
            thisItTime = thisItTime.addSecs( transInfo.utcOffset );

        if(thisItTime > ct) {
            // gone one too far; we've passed it. return the previous one, if
            // it exists
            if ( i > 0 )
                return i-1;
            return i;
        }
    }
    // must be the last index
    return transitionTimes.count()-1;
}

int TimeZoneData::findTimeTypeIndex( const QDateTime & c, bool utc ) const
{
    int transIndex = findTranstionTimeIndex( c, utc );
    if ( transIndex < 0 )
        return 0;
    return transitionTimes[ transIndex ].timeTypeIndex;
}

QString TimeZoneData::standardAbbreviation() const
{
    QDateTime dt = QDateTime::currentDateTime();
    int transIndex = findTranstionTimeIndex( dt, false );

    while ( transIndex >= 0 ) {
        int timeIndex = transitionTimes[ transIndex ].timeTypeIndex;
        if ( !timeTypes[ timeIndex ].isDst )
            return abbreviations[ timeTypes[ timeIndex ].abbreviationIndex ];

        transIndex--;
    }
    return QString();
}

QString TimeZoneData::dstAbbreviation() const
{
    QDateTime dt = QDateTime::currentDateTime();
    int transIndex = findTranstionTimeIndex( dt, false );

    while ( transIndex >= 0 ) {
        int timeIndex = transitionTimes[ transIndex ].timeTypeIndex;
        if ( timeTypes[ timeIndex ].isDst )
            return abbreviations[ timeTypes[ timeIndex ].abbreviationIndex ];

        transIndex--;
    }
    return QString();
}

bool TimeZoneData::isDaylightSavings( const QDateTime & c ) const
{
    int timeIndex = findTimeTypeIndex( c, false );
    return timeTypes[ timeIndex ].isDst;
}

QDateTime TimeZoneData::toUtc( const QDateTime &thisT ) const
{
    QDateTime dt(thisT.date(), thisT.time(), Qt::UTC);
    if ( !isValid() ) { qWarning("TimeZoneData::toUtc invalid"); return QDateTime(); }
    // find the appropriate utc time
    int timeIndex = findTimeTypeIndex( dt, false );
    dt = dt.addSecs( -1 * timeTypes[ timeIndex ].utcOffset );
    dt.setTimeSpec(Qt::LocalTime);
    return dt;
}

QDateTime TimeZoneData::fromUtc( const QDateTime &utc ) const
{
    QDateTime dt(utc.date(), utc.time(), Qt::UTC);
    if ( !isValid() ) { qWarning("TimeZoneData::fromUtc invalid"); return QDateTime(); }
    // convert from utc to "this" timezone
    int timeIndex = findTimeTypeIndex( dt, true );
    QDateTime rv = dt.addSecs( timeTypes[ timeIndex ].utcOffset );
    rv.setTimeSpec(Qt::LocalTime);
    return rv;
}

TimeZoneData::TimeZoneData( const QString & loc ) : mId( loc ), mDstRule( false )
{
    qint32 numUtcIndicators, numStandardIndicators, numLeapSecs,
        numTransitionTimes, numTimeTypes, tzAbbreviationChars;

    QString fname = TimeZonePrivate::zonePath() + mId;
    fname = QDir::convertSeparators( fname );
    QFile f( fname );
    if ( !f.open( QIODevice::ReadOnly ) ) {
	qWarning("Unable to open '%s'", fname.toLatin1().constData() );
        return;
    }

    QByteArray data = f.readAll();
    f.close();
    if ( !data.size() ) {
        qWarning("invalid data size = %d", data.size());
        return;
    }

    QDataStream ds( data );

    char rawMagic[5];
    uint nbytes=4;
    rawMagic[4] = 0;
    ds.readRawData( rawMagic, nbytes );
    QString magic( rawMagic );
    if ( magic != "TZif"
#ifdef Q_OS_MAC
        && ( magic[0] != 0 || magic[1] != 0 || magic[2] != 0 || magic[3] != 0 )
#endif
    ) {
        // magic match failed
        return;
    }

    char reserved[16];
    nbytes = 16;
    ds.readRawData(reserved, nbytes);
    if ( nbytes != 16 ) {
        qWarning("reserved size failed");
        return;
    }

    ds >> numUtcIndicators >> numStandardIndicators >> numLeapSecs
       >> numTransitionTimes >> numTimeTypes >> tzAbbreviationChars;

    transitionTimes.resize(numTransitionTimes);
    transitionInfo transInfo;
    int i =0;
    for ( i = 0; i < numTransitionTimes; ++i ) {
        quint32 secs;
        ds >> secs;

        transInfo.time = TimeZonePrivate::setUtcTime( (time_t)secs );
        if (transInfo.time.isNull()){
            qWarning("Invalid transistion time for %s", id().toLatin1().constData());
        }
        transInfo.timeTypeIndex = 0;
        transitionTimes[i] = transInfo;
    }

    unsigned char byte = 0;
    for ( i = 0; i < numTransitionTimes; ++i ) {
        ds >> byte;
        transInfo = transitionTimes[i];
        transInfo.timeTypeIndex = (int) byte;
        transitionTimes[i] = transInfo;
    }

    timeTypes.resize(numTimeTypes);
    unsigned int maxAbbrevIndex = 0;
    ttinfo offsetInfo;
    for ( i =0; i < numTimeTypes; ++i ) {
        ds >> offsetInfo.utcOffset;
        ds >> byte;
        offsetInfo.isDst = (bool) byte;
        if ( offsetInfo.isDst )
            mDstRule = true;
        ds >> byte;
        offsetInfo.abbreviationIndex = (unsigned int)byte;
        timeTypes[i] = offsetInfo;

        if (  offsetInfo.abbreviationIndex > maxAbbrevIndex )
            maxAbbrevIndex = offsetInfo.abbreviationIndex;
    }

    char abbrev[5];
    abbrev[4] = 0;
    nbytes = 4;
    for ( uint index = 0; index <= maxAbbrevIndex; index += 4 ) {
        ds.readRawData( abbrev, nbytes );
        abbreviations.insert( index, abbrev );
    }

    // leap seconds aren't set for any file, so not exactly sure
    // what they are used for
    for ( i = 0; i < numLeapSecs; ++i ) {
        qint32 leapSecOccurs, applyLeapSecs;
        ds >> leapSecOccurs >> applyLeapSecs;
    }

    for ( i = 0; i < numStandardIndicators; ++i ) {
        ds >> byte;
        offsetInfo = timeTypes[i];
        offsetInfo.isWallTime = !(bool) byte;
        timeTypes[i] = offsetInfo;
    }

    for ( i = 0; i < numUtcIndicators; ++i ) {
        ds >> byte;
        offsetInfo = timeTypes[i];
        offsetInfo.isTransitionLocal = !(bool) byte;
        timeTypes[i] = offsetInfo;
    }
}

static QString toHMS(qint32 seconds)
{
    QString hms;

    if (seconds > 0)
        hms += '-';
    else
        seconds *= -1;

    qint32 hours = seconds / (60*60);
    hms += QString::number(hours);

    seconds = seconds % (60*60);
    qint32 minutes = seconds / 60;
    if (minutes > 0) {
        hms += ':';
        if (minutes < 10)
            hms += '0';

        QString::number(minutes);
    }

    seconds = seconds % 60;
    if (seconds > 0) {
        hms += ':';
        if (minutes < 10)
            hms += '0';

        hms += QString::number(seconds);
    }

    return hms;
}

QString TimeZoneData::tzSetFormat(int stdIndex, int dstStartIndex, int dstEndIndex) const
{
    QString tz;

    if (stdIndex == -1 || stdIndex > transitionTimes.count())
        return tz;

    // standard time
    const ttinfo &stdTime = timeTypes[transitionTimes[stdIndex].timeTypeIndex];
    if (stdTime.isDst) {
        qWarning() << "stdIndex points to DST time.";
        return tz;
    }

    if (abbreviations[stdTime.abbreviationIndex].isEmpty())
        tz += "STD";
    else
        tz += abbreviations[stdTime.abbreviationIndex];
    tz += toHMS(stdTime.utcOffset);

    // daylight savings time, if applicable
    if (dstStartIndex == -1 || dstStartIndex > transitionTimes.count())
        return tz;

    if (dstEndIndex == -1 || dstEndIndex > transitionTimes.count())
        return tz;

    const ttinfo &dstTimeStart = timeTypes[transitionTimes[dstStartIndex].timeTypeIndex];
    if (!dstTimeStart.isDst) {
        qWarning() << "dstStartIndex points to STD time.";
        return tz;
    }

    const ttinfo &dstTimeEnd = timeTypes[transitionTimes[dstEndIndex].timeTypeIndex];
    if (dstTimeEnd.isDst) {
        qWarning() << "dstEndIndex points to DST time.";
        return tz;
    }

    if (abbreviations[dstTimeStart.abbreviationIndex].isEmpty())
        tz += "DST";
    else
        tz += abbreviations[dstTimeStart.abbreviationIndex];

    tz += toHMS(dstTimeStart.utcOffset);
    tz += ',';

    QDateTime dstStart = transitionTimes[dstStartIndex].time.addSecs(stdTime.utcOffset);
    QDateTime dstEnd = transitionTimes[dstEndIndex].time.addSecs(dstTimeStart.utcOffset);

    tz += QString::number(dstStart.date().dayOfYear() - 1);
    tz += dstStart.time().toString("/hh:mm:ss");
    tz += ',';
    tz += QString::number(dstEnd.date().dayOfYear() - 1);
    tz += dstEnd.time().toString("/hh:mm:ss");

    return tz;
}

QString TimeZoneData::tzSetFormat(const QDateTime &dt) const
{
    int transIndex = findTranstionTimeIndex(dt, true);

    if (timeTypes[transitionTimes[transIndex].timeTypeIndex].isDst) {
        // currently in DST use transIndex+1 as STD
        return tzSetFormat(transIndex+1, transIndex, transIndex+1);
    } else {
        // currently in STD use transIndex+1 and transIndex+2 to calculate DST
        return tzSetFormat(transIndex, transIndex+1, transIndex+2);
    }
}

/*******************************************************************
 *
 * TimeZoneLocation
 *
 *******************************************************************/

class TimeZoneLocation
{
public:
    TimeZoneLocation( const char *line );

    bool isValid() const { return !mId.isEmpty(); }

// in seconds
    int latitude() const;
// in seconds
    int longitude() const;

    QString description() const;
    QString area() const;
    QString city() const;
    QString countryCode() const;
    QByteArray id() const;
    int distance( const TimeZoneLocation &e ) const
        { return qAbs(latitude() - e.latitude()) + qAbs(longitude() - e.longitude()); }

    static void load( QHash<QByteArray,TimeZoneLocation*> &store );
    static QStringList languageList();

private:
    int calcLat(const QConstCString &) const;
    int calcLon(const QConstCString &) const;

private:
    QByteArray mId;
    QByteArray mDescription;
    QByteArray mCountryCode;
    int mLat;
    int mLon;
};

TimeZoneLocation::TimeZoneLocation( const char *line )
    : mLat(0), mLon(0)
{
    QConstCString mLine(line);

    int pos = 0;
    int endPos = mLine.length();

    int tokenBegin = pos;
    int tokenNum = 0;

    const char *pLine = line + pos;
    while (pos < endPos) {
        char ch = *pLine;
        if ( ch == '\t' || ch == '\n' || ch == '\r' || pos+1 == endPos ) {
            QConstCString token = mLine.mid(tokenBegin, pos-tokenBegin);
            switch ( tokenNum ) {
            case 0:
                mCountryCode = token.toByteArray();
                break;
            case 1: {
                QConstCString latStr, lonStr;
                if ( token.length() == 15 ) {
                    latStr = token.mid(0, 7);
                    lonStr = token.mid(7);
                } else if ( token.length() == 11 ) {
                    latStr = token.mid(0, 5);
                    lonStr = token.mid(5);
                }
                else {
                    qWarning() << "can't parse lat lon str" << token.toByteArray();
                    return;
                }

                // sanity check
                if ( latStr[0] != '+' && latStr[0] != '-') {
                    qWarning("lat/lon is invalid");
                    return;
                }
                mLat = calcLat(latStr);
                mLon = calcLon(lonStr);
            }
                break;
            case 2: {
                mId = token.toByteArray();
            } break;
            case 3:
                mDescription = token.toByteArray();
                break;
            }

            tokenNum++;
            tokenBegin = pos+1;
        }

        pos++;
        pLine++;
    }
}

int TimeZoneLocation::calcLat(const QConstCString &latStr) const
{
    int deg, min, sec;
    int sign = 1;
    if ( latStr[0] == '-' )
        sign = -1;
    deg = (latStr[1] - '0') * 10 + (latStr[2] - '0');
    min = (latStr[3] - '0') * 10 + (latStr[4] - '0');

    //deg = latStr.mid(1, 2).toInt();
    //min = latStr.mid(3, 2).toInt();
    sec = 0;
    if ( latStr.length() == 7 )
        sec = (latStr[5] - '0') * 10 + (latStr[6] - '0');
//      sec = latStr.mid(5, 2).toInt();
    return sign*deg*3600 + sign*min*60 + sign*sec;
}

int TimeZoneLocation::calcLon(const QConstCString &lonStr) const
{
    int deg, min, sec;

    int sign = 1;
    if ( lonStr[0] == '-' )
        sign = -1;

    deg = (lonStr[1] - '0') * 100 + (lonStr[2] - '0') * 10 + (lonStr[3] - '0');
    min = (lonStr[4] - '0') * 10 + (lonStr[5] - '0');
    // deg = lonStr.mid(1, 3).toInt();
    //min = lonStr.mid( 4, 2 ).toInt();
    sec = 0;
    if ( lonStr.length() == 8 )
        sec = (lonStr[6] - '0') * 10 + (lonStr[7] - '0');
//      sec = lonStr.mid( 6, 2 ).toInt();

    return sign*deg*3600 + sign*min*60 + sign*sec;
}

int TimeZoneLocation::latitude() const
{
    return mLat;
}

int TimeZoneLocation::longitude() const
{
    return mLon;
}

QString TimeZoneLocation::description() const
{
    return qApp->translate( "QTimeZone", mDescription.constData() );
}

QString TimeZoneLocation::area() const
{
    QString displayArea = mId;
    int sp = displayArea.lastIndexOf('/');
    if (sp >= 0)
        displayArea.truncate(sp);
    displayArea.replace( '_', ' ' );
    return qApp->translate( "QTimeZone", displayArea.toAscii().constData() );
}

QString TimeZoneLocation::city() const
{
    QString displayCity;
    int sp = mId.lastIndexOf('/');
    if (sp >= 0)
        displayCity = mId.mid(sp+1);
    displayCity.replace( '_', ' ' );

    return qApp->translate( "QTimeZone", displayCity.toAscii().constData() );
}

QString TimeZoneLocation::countryCode() const
{
    return mCountryCode;
}

QByteArray TimeZoneLocation::id() const
{
    return mId;
}

QStringList TimeZoneLocation::languageList()
{
    return Qtopia::languageList();
}

void TimeZoneLocation::load( QHash<QByteArray,TimeZoneLocation*> &store )
{

    QStringList langs = languageList();
    for (QStringList::ConstIterator lit = langs.begin(); lit!=langs.end(); ++lit) {
        QString lang = *lit;
        QTranslator * trans = new QTranslator(qApp);
        QString tfn = Qtopia::qtopiaDir()+"i18n/"+lang+"/timezone.qm";
        if ( trans->load( tfn ))
            qApp->installTranslator( trans );
        else
            delete trans;
    }
    QFile file( TimeZonePrivate::zoneFile() );
    if ( !file.open( QIODevice::ReadOnly ) ) {
        qWarning( "Unable to open %s", file.fileName().toLatin1().constData() );
        qFatal( "Fatal: Timezone data must be installed at %s, exiting", TimeZonePrivate::zonePath().toLatin1().constData() );
        return;
    }

    char line[2048];
    TimeZoneLocation *tz = NULL;
    while ( !file.atEnd() ) {

        file.readLine( line, 2048 );
        if ( line[0] == '#' )
            continue;

        tz = new TimeZoneLocation( line );

        if ( !tz->isValid() ) {
            qWarning("TimeZoneLocation::load Unable to parse line %s", (const char *)line );
            delete tz; tz = NULL;
            continue;
        }
        store.insert( tz->id(), tz );
    }

    file.close();
}

/*******************************************************************
 *
 * TzCache
 *
 *******************************************************************/

class TzCache : public QObject
{
public:
    static TzCache &instance();
    TimeZoneData *data( const QString &id );
    TimeZoneLocation *location( const QString &id );
    QStringList ids();
    TimeZoneData *findFromMinutesEast(QDateTime t, int mineast, bool isdst);


private:
    TzCache();
    QMap<QString,TimeZoneData*> mDataDict;
    QHash<QByteArray,TimeZoneLocation*> mLocationDict;
    static QPointer<TzCache> sInstance;
};

QPointer<TzCache> TzCache::sInstance = 0;

TzCache &TzCache::instance()
{
    if ( !sInstance ) {
        if (!qApp) qWarning() << "Timezone translations cannot be loaded.";
        sInstance = new TzCache();
    }
    return *sInstance;
}

TzCache::TzCache()
    : QObject( qApp )
{
    // load the zone.tab file
    TimeZoneLocation::load( mLocationDict );
}

TimeZoneData *TzCache::data( const QString &id )
{
    if ( id.isEmpty() )
        return TimeZoneData::null;

    QMap<QString,TimeZoneData*>::const_iterator it = mDataDict.find(id);
    if (it != mDataDict.end())
        return *it;

    TimeZoneData *d = new TimeZoneData( id );
    if ( !d->isValid() ) {
        qWarning("QTimeZone::data Can't create a valid data object for '%s'", id.toLatin1().constData() );
        delete d;
        return TimeZoneData::null;
    }
    mDataDict.insert( id, d );

    return d;
}

TimeZoneData *TzCache::findFromMinutesEast(QDateTime t, int mineast, bool isdst)
{
    QMap<QString,TimeZoneData*>::const_iterator it = mDataDict.begin();
    while ( it != mDataDict.end()) {
        if ( (*it)->matchTime(t,mineast*60,isdst) )
            return *it;
        ++it;
    }
    return 0;
}

TimeZoneLocation *TzCache::location( const QString &id )
{
    if ( id.isEmpty() )
        return 0;
    TimeZoneLocation * l = mLocationDict[ id.toAscii() ];
    return l;
}

QStringList TzCache::ids()
{
    QStringList rv;
    foreach(QByteArray key, mLocationDict.keys())
        rv << key;
    return rv;
}

TimeZoneLocation *TimeZonePrivate::location() const
{
    if ( !loc )
        loc = TzCache::instance().location( ident );
    return loc;
}

TimeZoneData *TimeZonePrivate::data() const
{
    if ( !dat )
        dat = TzCache::instance().data( ident );
    return dat;
}

/*******************************************************************
 *
 * QTimeZone
 *
 *******************************************************************/

/*!
  \class QTimeZone
    \inpublicgroup QtBaseModule

  \brief The QTimeZone class provides access to time zone data.

  QTimeZone provides access to timezone data and conversion between
  dates and times in different time zones.

  A time zone is a region of the world that has adopted the same standard time,
  including conventions on Daylight Saving or Summer time adjustments.

  Each time zone is specified by a unique identifier. A set of such identifiers is known
  to the system, and can be found using ids().

  Conversions between time zones can either be done via UTC, using fromUtc() and toUtc(),
  or directly with convert(), fromCurrent(), or toCurrent().

  Each time zone is in a greater area() and has an associated city() that observes
  the standard. This city is not necessarily the capital, just a well-known city
  in the area. More than one city in an area may observe identical standards, yet
  they will appear as distinct time zones. The countyCode() of the country containing
  the time zone is also available.

  A QTimeZone either isValid() or is not valid. The default constructor creates an
  invalid QTimeZone.

  Note that some features of QTimeZone may not be available on non-POSIX platforms.

  \ingroup time

  \sa QtopiaApplication::timeChanged()
*/

/*!
  Construct an invalid time zone.
*/
QTimeZone::QTimeZone() : d( new TimeZonePrivate( 0 ) )
{
}

/*!
  Construct a QTimeZone for location \a locId. The time zone isValid() if
  \a locId is a valid time zone identifier in the list of ids().
*/
QTimeZone::QTimeZone( const char * locId ) : d( new TimeZonePrivate( locId ) )
{
}

/*!
  Constructs a copy of the \a other QTimeZone.
*/
QTimeZone::QTimeZone( const QTimeZone & other ) :
    d( new TimeZonePrivate( other.d->id() ) )
{
}

/*!
  Destruct QTimeZone.
*/
QTimeZone::~QTimeZone()
{
    delete d; d = 0;
}

/*!
  Sets the time zone id of this QTimeZone to \a id.
*/
void QTimeZone::setId( const char *id )
{
    d->setId(id);
}

/*!
  Assign time zone \a from to this time zone.
*/
QTimeZone &QTimeZone::operator=( const QTimeZone &from)
{
    d->setId(from.d->id());
    return *this;
}

/*!
  Returns true if \a c is equal to this, otherwise false.
*/
bool QTimeZone::operator==( const QTimeZone &c) const
{
    return (d->id() == c.d->id());
}

/*!
  Returns true if \a c is not equal to this, otherwise false.
*/
bool QTimeZone::operator!=( const QTimeZone &c) const
{
    return (d->id() != c.d->id());
}

/*!
  Returns a time zone that, at time \a t, is \a mineast minutes ahead of GMT,
  and is observing Daylight Time according to \a isdst.

  Usually, the returned timezone will be a UNIX GMT time zone offset
  (eg. "GMT-10" for 10 hours East of GMT), however, if no such timezone
  exists (eg. for half-hour time zones), a city time zone will be returned.
*/
QTimeZone QTimeZone::findFromMinutesEast(const QDateTime& t, int mineast, bool isdst)
{
    if ( mineast % 60 == 0 ) {
        QString s;
        s.sprintf("Etc/GMT%+d",-mineast/60);
        return QTimeZone(s.toLatin1().data());
    }

    // Fallback: find non-standard city
    TimeZoneData *data = TzCache::instance().findFromMinutesEast(t,mineast,isdst);
    return QTimeZone(data ? data->id().toLatin1().data() : 0);
}

/*!
  Return a time zone located at the UTC reference.
*/
QTimeZone QTimeZone::utc()
{
    //return QTimeZone("Europe/London");
    return QTimeZone("UTC");
}

/*!
  Return the current system UTC date and time.
*/
QDateTime QTimeZone::utcDateTime()
{
    return TimeZonePrivate::setUtcTime( (time_t)time(0) );
}

/*!
  Returns the date and time in this time zone from the number of seconds
  since 1 January 1970.
*/
QDateTime QTimeZone::fromTime_t( time_t secs ) const
{
    QDateTime utc = TimeZonePrivate::setUtcTime( secs );
    return fromUtc( utc );
}

/*!
  Returns the date and time \a dt in this time zone,
  as the number of seconds since 1 January 1970.
*/
uint QTimeZone::toTime_t( const QDateTime &dt ) const
{
    return toUtc( dt ).toTime_t();
}

/*!
  Returns the date and time \a dt in this time zone as
  a date and time in UTC.
*/
QDateTime QTimeZone::toUtc( const QDateTime &dt ) const
{
    TimeZoneData *data = d->data();
    return data->toUtc( dt );
}

/*!
  Returns the UTC date and time \a utc as a date and time in this time zone.
*/
QDateTime QTimeZone::fromUtc( const QDateTime &utc ) const
{
    TimeZoneData *data = d->data();
    return data->fromUtc( utc );
}

/*!
  Returns the date and time \a dt in this time zone as the date and time
  in the current system time zone.

  \sa current()
*/
QDateTime QTimeZone::toCurrent( const QDateTime &dt ) const
{
    QTimeZone curTz = current();
    return curTz.convert( dt, *this );
}

/*!
  Returns the date and time \a curT in the current time zone as the
  date and time in this time zone.

  \sa current()
*/
QDateTime QTimeZone::fromCurrent( const QDateTime &curT ) const
{
    QTimeZone curTz = current();
    return convert( curT, curTz );
}

/*!
  Return the date and time \a dt in time zone \a dtTz as the date and time
  in this time zone.
*/
QDateTime QTimeZone::convert( const QDateTime &dt, const QTimeZone &dtTz ) const
{
    QDateTime utc = dtTz.toUtc( dt );
    return fromUtc( utc );
}

QString lastZoneRead;
QString lastLocRead;

/*!
  Returns the current system time zone.
*/
QTimeZone QTimeZone::current()
{
    QString cZone;
#ifndef __UCLIBC__
    cZone = getenv("TZ");
#endif
    if (cZone.isEmpty()) {
        QFile file("/etc/timezone");
        if (file.open(QFile::ReadOnly))
            cZone = file.readAll().trimmed();
    }

#ifdef Q_WS_MAC
    QTimeZone env(cZone.toLocal8Bit());
    if ( env.isValid() )
        return env;

    time_t now = time(0);
    cZone = localtime(&now)->tm_zone;
    QTimeZone lt(cZone.toLocal8Bit());
    if ( lt.isValid() )
        return lt;

    QFileInfo el("/etc/localtime");
    QString zone = el.readLink();
    int z = zone.indexOf("/zoneinfo/");
    if ( z >= 0 ) {
        QTimeZone zi(zone.mid(z+10).toLocal8Bit());
        if ( zi.isValid() )
            return zi;
    }
#else
    QString currentLoc;
    if (lastLocRead.isEmpty() || lastZoneRead != cZone) {
        if (cZone.isEmpty()) {
            QSettings lconfig("Trolltech","locale");
            lconfig.beginGroup( "Location" );
            currentLoc = lconfig.value("Timezone", "America/New_York").toString();
        } else {
            currentLoc = cZone;
        }
        lastZoneRead = cZone;
        lastLocRead = currentLoc;
    } else {
        currentLoc = lastLocRead;
    }
    if ( !currentLoc.isEmpty() )
        return QTimeZone( currentLoc.toAscii().constData() );

    qWarning("QTimeZone::current Location information is not set in the QSettings file locale!");
    // this is mainly for windows side code, in the initial case
    tzset();
    QString standardAbbrev, daylightAbbrev;

    standardAbbrev = tzname[0];
    daylightAbbrev = tzname[1];

    QDateTime today = QDateTime::currentDateTime();
    QStringList allIds = TzCache::instance().ids();
    foreach (QString id, allIds) {
        if (id.isEmpty())
            continue;
        TimeZoneData *data = TzCache::instance().data( id );
        if ( data->matchAbbrev( today, standardAbbrev, daylightAbbrev ) ) {
            currentLoc = id;
            break;
        }
    }

    if ( !currentLoc.isEmpty() ) {
        QSettings lconfig("Trolltech","locale");
        lconfig.beginGroup( "Location" );
        lconfig.setValue( "Timezone", currentLoc );
        return QTimeZone (currentLoc.toAscii().constData());
    }
#endif

    return QTimeZone();
}

/*!
  Return the time zone identifier, for example, Europe/London
*/
QString QTimeZone::id() const
{
    return d->id();
}

/*!
  Returns true if this is a valid time zone, otherwise false.
*/
bool QTimeZone::isValid() const
{
    TimeZoneData *data = d->data();
    TimeZoneLocation *loc = d->location();

    if (d->id() == "UTC" && data->isValid())
        return true;
    return data->isValid() && loc && loc->isValid();
}

/*!
  Returns the Daylight Savings Time (DST) time zone abbreviation.
*/
QString QTimeZone::dstAbbreviation() const
{
    TimeZoneData *data = d->data();
    return data->dstAbbreviation();
}

/*!
  Returns the time zone abbreviation, e.g. EST
*/
QString QTimeZone::standardAbbreviation() const
{
    TimeZoneData *data = d->data();
    return data->standardAbbreviation();
}

/*!
  Returns a list of all time zone ids known to the system.
*/
QStringList QTimeZone::ids()
{
    return TzCache::instance().ids();
}

/*!
  Returns the latitude of the city() of this timezone, in seconds of a degree.
*/
int QTimeZone::latitude() const
{
    TimeZoneLocation *loc = d->location();
    if ( loc )
        return loc->latitude();
    return 0;
}


/*!
  Returns the longitude of the city() of this timezone, in seconds of a degree.
*/
int QTimeZone::longitude() const
{
    TimeZoneLocation *loc = d->location();
    if ( loc )
        return loc->longitude();
    return 0;
}

/*!
  Returns the translated description of this time zone.
*/
QString QTimeZone::description() const
{
    TimeZoneLocation *loc = d->location();
    if ( loc )
        return loc->description();
    return QString();
}

/*!
  Returns the translated greater area of this time zone, e.g. Europe.
*/
QString QTimeZone::area() const
{
    TimeZoneLocation *loc = d->location();
    if ( loc )
        return loc->area();
    return QString();
}

/*!
  Returns either the city() for this time zone, or some other
  string meaningful to the user (eg. "GMT+10" meaning 10 hours
  East of GMT).
*/
QString QTimeZone::name() const
{
    if (d->id().left(7) == "Etc/GMT") {
        // UNIX uses hours West (i.e. towards Berkeley), i.e. "behind", not "ahead".
        // Humans use hours "ahead" (i.e. "+") of GMT/UTC.
        bool ok;
        int west = d->id().mid(7).toInt(&ok);
        if (ok) {
            QString r;
            r.sprintf("GMT %+d",-west);
            return r;
        }
    }
    TimeZoneLocation *loc = d->location();
    if ( loc )
        return loc->city();
    return d->id(); // Better than nothing.
}

/*!
  Returns the translated city of this time zone, e.g. Oslo,
  or a null string if no specific city is identified.
*/
QString QTimeZone::city() const
{
    TimeZoneLocation *loc = d->location();
    if ( loc )
        return loc->city();
    return QString();
}

/*!
  Returns the ISO 3166 2-character country code.
*/
QString QTimeZone::countryCode()
{
    TimeZoneLocation *loc = d->location();
    if ( loc )
        return loc->countryCode();
    return QString();
}

/*!
  \internal
  Returns the sum of the differences in longitude and latitude between
  points in this and another timezone.
*/
int QTimeZone::distance( const QTimeZone &e ) const
{
    TimeZoneLocation *loc = d->location();
    TimeZoneLocation *comp = e.d->location();
    if ( loc && comp )
        return loc->distance( *comp );
    return 0;
}

/*!
  \internal
  \fn void QTimeZone::serialize(Stream &stream) const
*/
template <typename Stream> void QTimeZone::serialize(Stream &stream) const
{
    stream << id();
}

/*!
  \internal
  \fn void QTimeZone::deserialize(Stream &stream)
*/
template <typename Stream> void QTimeZone::deserialize(Stream &stream)
{
    QString id;
    stream >> id;
    setId( id.toAscii() );
}

/*!
    Sets the system time zone to \a newtz.
*/
void QTimeZone::setSystemTimeZone(const QString &newtz)
{
    QString tzEnv;

#ifdef __UCLIBC__
    // uClibc does not support zoneinfo
    QTimeZone tz(newtz.toLatin1().constData());
    tzEnv = tz.d->data()->tzSetFormat(utcDateTime());

    QFile localTime("/etc/TZ");
    if (localTime.open(QIODevice::WriteOnly | QIODevice::Text)) {
        localTime.write(tzEnv.toLatin1().constData());
        localTime.write(QByteArray(1, '\n'));
        localTime.close();
    } else {
        qWarning() << "Cannot open" << localTime.fileName() << "for writing.";
    }
#else
    // for systems that support zoneinfo
    tzEnv = newtz;

#if defined(QTOPIA_ZONEINFO_PATH)
    QString filename = QTOPIA_ZONEINFO_PATH + newtz;
#else
    QString filename = "/usr/share/zoneinfo/" + newtz;
#endif
    QFile localTime("/etc/localtime");
    if (localTime.exists())
        if (!localTime.remove())
            qWarning() << "localtime could not be removed";

    if (!QFile::copy(filename, "/etc/localtime"))
        qWarning() << "Could not copy" << newtz<< "to localtime";
#endif

    setenv("TZ", tzEnv.toLatin1().constData(), 1);
    tzset();

    QSettings lconfig("Trolltech","locale");
    lconfig.beginGroup( "Location" );
    lconfig.setValue( "Timezone", newtz.toLatin1().constData() );

    QFile timeZone("/etc/timezone");
    if (timeZone.open(QIODevice::WriteOnly | QIODevice::Text)) {
        timeZone.write(newtz.toLatin1().constData());
        localTime.close();
    } else {
        qWarning() << "Cannot open" << timeZone.fileName() << "for writing.";
    }
}

/*!
    Sets the application time zone to \a newtz.
*/
void QTimeZone::setApplicationTimeZone(const QString &newtz)
{
    QString tzEnv;

#ifdef __UCLIBC__
    QTimeZone tz(newtz.toLatin1().constData());
    tzEnv = tz.d->data()->tzSetFormat(utcDateTime());
#else
    tzEnv = newtz;
#endif

    setenv("TZ", tzEnv.toLatin1(), 1);
    tzset();
}

Q_IMPLEMENT_USER_METATYPE(QTimeZone)
