/******************************************************+**********************
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

#include <qmodemphonebook.h>
#include <qmodemservice.h>
#include <qatutils.h>
#include <qatresultparser.h>
#include <QTimer>
#include <QTextCodec>

/*!
    \class QModemPhoneBook
    \inpublicgroup QtCellModule

    \brief The QModemPhoneBook class implements SIM phone book operations for AT-based modems.

    This class uses the commands \c{AT+CPBS}, \c{AT+CPBR}, \c{AT+CPBW}, \c{AT+CSCS},
    and \c{AT+CLCK} from 3GPP TS 27.007.

    QModemPhoneBook implements the QPhoneBook telephony interface.  Client
    applications should use QPhoneBook instead of this class to
    access the modem's SIM phone books.

    \sa QPhoneBook
    \ingroup telephony::modem
*/

// Fetch 10 entries at a time every half a second when in slow update mode.
#define SLOW_UPDATE_TIMEOUT     500

class QModemPhoneBookPrivate
{
public:
    QModemPhoneBookPrivate()
    {
        caches = 0;
        charsetRetry = 5;
        storageRetry = 5;
    }
    ~QModemPhoneBookPrivate();

    QModemService *service;
    QModemPhoneBookCache *caches;
    QString prevStorageName;
    QString pendingCommand;
    QString pendingStore;
    QTimer *slowTimer;
    QTextCodec *stringCodec;
    int charsetRetry;
    int storageRetry;
};

class QModemPhoneBookOperation
{
public:
    enum Operation { Add, Remove, Update };

    QModemPhoneBookOperation( Operation oper, const QPhoneBookEntry& entry, QModemPhoneBookOperation *next )
    {
        this->oper = oper;
        this->entry = entry;
        this->next = next;
    }
    ~QModemPhoneBookOperation();

public:
    Operation oper;
    QPhoneBookEntry entry;
    QModemPhoneBookOperation *next;
};

QModemPhoneBookOperation::~QModemPhoneBookOperation()
{
    if ( next )
        delete next;
}

class QModemPhoneBookCache
{
public:
    QModemPhoneBookCache( const QString& store );
    ~QModemPhoneBookCache();

public:
    QString store;
    QModemPhoneBookCache *next;
    QList<QPhoneBookEntry> entries;
    uint first, last, current;
    bool fullyLoaded;
    bool needEmit;
    bool needLimitEmit;
    bool fast;
    bool redoQuery;
    bool selectOk;
    QModemPhoneBookOperation *opers;
    QString passwd;
    QPhoneBookLimits limits;
};

QModemPhoneBookCache::QModemPhoneBookCache( const QString& store )
{
    this->store = store;
    next = 0;
    first = 0;
    last = 0;
    current = 0;
    fullyLoaded = false;
    needEmit = false;
    needLimitEmit = false;
    fast = false;
    redoQuery = false;
    selectOk = true;
    opers = 0;
    passwd = QString();
}

QModemPhoneBookCache::~QModemPhoneBookCache()
{
    if ( next ) {
        delete next;
    }
    if ( opers ) {
        delete opers;
    }
}

QModemPhoneBookPrivate::~QModemPhoneBookPrivate()
{
    if ( caches )
        delete caches;
}

/*!
    Construct an AT-based SIM phone book handler for \a service.
*/
QModemPhoneBook::QModemPhoneBook( QModemService *service )
    : QPhoneBook( service->service(), service, QCommInterface::Server )
{
    d = new QModemPhoneBookPrivate();
    d->service = service;

    d->slowTimer = new QTimer( this );
    d->slowTimer->setSingleShot( true );
    QObject::connect( d->slowTimer, SIGNAL(timeout()),
                      this, SLOT(slowTimeout()) );
}

/*!
    Destroy this AT-based SIM phone book handler.
*/
QModemPhoneBook::~QModemPhoneBook()
{
    delete d;
}

/*!
    Returns the codec being used by the phone book subsystem to
    decode strings from the modem.  This may be useful in other
    interfaces that need to decode strings.  The value is based
    on the response to the \c{AT+CSCS?} command.

    \sa updateCodec()
*/
QTextCodec *QModemPhoneBook::stringCodec() const
{
    return d->stringCodec;
}

/*!
    \reimp
*/
void QModemPhoneBook::getEntries( const QString& store )
{
    QModemPhoneBookCache *cache;

    // Find the cache entry associated with this store.
    cache = findCache( store );

    // If the cache is fully loaded, then emit the result now.
    // If there is a password, then force a re-get of the phone book
    // because the new password may not be the same as the original.
    if ( cache->fullyLoaded && cache->passwd.isEmpty() ) {
        emit entries( store, cache->entries );
        return;
    }

    // We need to requery the extents if the phone book was previously loaded.
    if ( cache->fullyLoaded ) {
        cache->fullyLoaded = false;
        cache->entries.clear();
        cache->limits = QPhoneBookLimits();
        sendQuery( cache );
    }

    // We'll need a signal to be emitted once it is fully loaded.
    cache->needEmit = true;
}

// Strip non-numeric digits and then convert commas into the
// extension "digit" used by the modem.
static QString fixPhoneBookNumber( const QString& number )
{
    QString temp = QAtUtils::stripNumber( number );
    return temp.replace( QChar( ',' ), QChar( 'p' ) );
}

/*!
    \reimp
*/
void QModemPhoneBook::add
        ( const QPhoneBookEntry& entry, const QString& store, bool flush )
{
    QModemPhoneBookCache *cache = findCache( store );
    QPhoneBookEntry newEntry( entry );
    newEntry.setNumber( fixPhoneBookNumber( entry.number() ) );
    newEntry.setIndex( 0 );
    cache->opers = new QModemPhoneBookOperation
        ( QModemPhoneBookOperation::Add, newEntry, cache->opers );
    if ( cache->fullyLoaded && flush ) {
        flushOperations( cache );
    }
}

/*!
    \reimp
*/
void QModemPhoneBook::remove( uint index, const QString& store, bool flush )
{
    QModemPhoneBookCache *cache = findCache( store );
    QPhoneBookEntry entry;
    entry.setIndex( index );
    cache->opers = new QModemPhoneBookOperation
        ( QModemPhoneBookOperation::Remove, entry, cache->opers );
    if ( cache->fullyLoaded && flush ) {
        flushOperations( cache );
    }
}

/*!
    \reimp
*/
void QModemPhoneBook::update
        ( const QPhoneBookEntry& entry, const QString& store, bool flush )
{
    QModemPhoneBookCache *cache = findCache( store );
    QPhoneBookEntry newEntry( entry );
    newEntry.setNumber( fixPhoneBookNumber( entry.number() ) );
    cache->opers = new QModemPhoneBookOperation
        ( QModemPhoneBookOperation::Update, newEntry, cache->opers );
    if ( cache->fullyLoaded && flush ) {
        flushOperations( cache );
    }
}

/*!
    \reimp
*/
void QModemPhoneBook::flush( const QString& store )
{
    QModemPhoneBookCache *cache = findCache( store );
    if ( cache->fullyLoaded ) {
        flushOperations( cache );
    }
}

/*!
    \reimp
*/
void QModemPhoneBook::setPassword
        ( const QString& store, const QString& password )
{
    QModemPhoneBookCache *cache = findCache( store, true, password );
    cache->passwd = password;

    // Force the storage to be reselected upon the next operation,
    // to force the password to be sent.
    forceStorageUpdate();
}

/*!
    \reimp
*/
void QModemPhoneBook::clearPassword( const QString& store )
{
    QModemPhoneBookCache *cache = findCache( store );
    cache->passwd = QString();
}

/*!
    \reimp
*/
void QModemPhoneBook::requestLimits( const QString& store )
{
    QModemPhoneBookCache *cache;

    // Find the cache entry associated with this store.
    cache = findCache( store );

    // If the cache is fully loaded, then emit the result now.
    // If there is a password, then force a re-get of the phone book
    // because the new password may not be the same as the original.
    if ( cache->fullyLoaded && cache->passwd.isEmpty() ) {
        emit limits( store, cache->limits );
        return;
    }

    // We need to requery the extents if the phone book was previously loaded.
    if ( cache->fullyLoaded ) {
        cache->fullyLoaded = false;
        cache->entries.clear();
        cache->limits = QPhoneBookLimits();
        sendQuery( cache );
    }

    // We'll need a signal to be emitted once it is fully loaded.
    cache->needLimitEmit = true;
}

/*!
    \reimp
*/
void QModemPhoneBook::requestFixedDialingState()
{
    d->service->secondaryAtChat()->chat
        ( "AT+CLCK=\"FD\",2", this, SLOT(fdQueryDone(bool,QAtResult)) );
}

/*!
    \reimp
*/
void QModemPhoneBook::setFixedDialingState( bool enabled, const QString& pin2 )
{
    QString cmd = "AT+CLCK=\"FD\",";
    if ( enabled )
        cmd += "1,\"";
    else
        cmd += "0,\"";
    cmd += QAtUtils::quote( pin2 );
    cmd += "\"";
    d->service->secondaryAtChat()->chat
        ( cmd, this, SLOT(fdModifyDone(bool,QAtResult)) );
}

/*!
    Attempt to preload the contents of \a store, in anticipation that
    it will be needed sometime in the future.  This is typically used
    for the "SM" store so that the contacts list appears to load
    quickly for the user.
*/
void QModemPhoneBook::preload( const QString& store )
{
    // Create the cache in "slow loading" mode.
    if ( ! hasModemPhoneBookCache() ) {
        findCache( store, false );
    }
}

/*!
    Flush all cached phone book information from the system because the
    SIM has been removed from the modem or because the modem has indicated
    that the SIM phone book has changed.
*/
void QModemPhoneBook::flushCaches()
{
    removeAllCaches();
    d->pendingCommand = QString();
    d->pendingStore = QString();
}

/*!
    Indicate that the modem is now ready to accept phone book
    related commands.  See hasModemPhoneBookCache() for more information.

    \sa hasModemPhoneBookCache()
*/
void QModemPhoneBook::phoneBooksReady()
{
    // Caching modem has finished preloading the SIM phone book into
    // its own internal memory.  Now we copy it into ours and then
    // force it to be emitted to the application layer to refresh
    // contact lists.
    requestStorages();
    QModemPhoneBookCache *cache = findCache( "SM" );
    cache->needEmit = true;

    // Do the same with the emergency numbers list.
    cache = findCache( "EN" );
    cache->needEmit = true;
}

/*!
    Returns true if the modem has an internal phone book cache; false otherwise.
    If this function returns true, then the phone library will not
    attempt to access phone books until phoneBooksReady()
    is called.  The default implementation returns false.

    \sa phoneBooksReady()
*/
bool QModemPhoneBook::hasModemPhoneBookCache() const
{
    return false;
}

/*!
    Returns true if the \c index option to the \c{AT+CPBW} command should
    be empty to add an element rather than to update an existing element.
    Returns false (the default) if the \c index option should be zero
    for this case.
*/
bool QModemPhoneBook::hasEmptyPhoneBookIndex() const
{
    return false;
}

void QModemPhoneBook::forceStorageUpdate()
{
    // Force "updateStorageName()" to send the name update the next operation.
    d->prevStorageName = QString();
}

class QStoreUserData : public QAtResult::UserData
{
public:
    QStoreUserData( const QString& store ) { this->store = store; }

    QString store;
};

void QModemPhoneBook::updateStorageName
        ( const QString& storage, QObject *target, const char *slot )
{
    if ( d->prevStorageName != storage ) {
        d->prevStorageName = storage;
        QModemPhoneBookCache *cache = findCache( storage );
        if ( !cache->passwd.isEmpty() ) {
            d->service->secondaryAtChat()->chat
                ( "AT+CPBS=\"" + storage + "\",\"" +
                  QAtUtils::quote( cache->passwd ) + "\"", target, slot,
                  new QStoreUserData( storage ) );
        } else {
            d->service->secondaryAtChat()->chat
                ( "AT+CPBS=\"" + storage + "\"", target, slot,
                  new QStoreUserData( storage ) );
        }
    }
}

void QModemPhoneBook::sendQuery( QModemPhoneBookCache *cache )
{
    // Update the current storage name in the phone device.
    cache->selectOk = false;
    updateStorageName( cache->store, this, SLOT(selectDone(bool,QAtResult)) );
    
    // Query the extents of the phone book, so we know what range to use.
    d->service->secondaryAtChat()->chat
        ( "AT+CPBR=?", this, SLOT(queryDone(bool,QAtResult)),
          new QStoreUserData( cache->store ) );
}

void QModemPhoneBook::selectDone( bool ok, const QAtResult& result )
{
    QString store = ((QStoreUserData *)result.userData())->store;
    QModemPhoneBookCache *cache = findCache( store, false );
    cache->selectOk = ok;
}

QModemPhoneBookCache *QModemPhoneBook::findCache
    ( const QString& store, bool fast, const QString& initialPassword )
{
    QModemPhoneBookCache *cache;

    // Search for an existing cache with this name.
    cache = d->caches;
    while ( cache != 0 ) {
        if ( cache->store == store ) {
            if ( fast ) {
                // We need the values soon, so speed up the fetch cycle.
                cache->fast = true;
            }
            if ( cache->redoQuery ) {
                // The last query attempt failed, so send another.
                cache->redoQuery = false;
                forceStorageUpdate();
                sendQuery( cache );
            }
            return cache;
        }
        cache = cache->next;
    }

    // Create a new cache.
    cache = new QModemPhoneBookCache( store );
    cache->next = d->caches;
    cache->fast = fast;
    cache->passwd = initialPassword;
    d->caches = cache;

    // Send the query command.
    sendQuery( cache );

    // Return the cache to the caller.
    return cache;
}

void QModemPhoneBook::flushOperations( QModemPhoneBookCache *cache )
{
    QModemPhoneBookOperation *oper;
    QModemPhoneBookOperation *prev;
    bool changes = false;
    int used = (int)(cache->limits.used());

    // Process the operations, from the end of the queue back.
    while ( cache->opers != 0 ) {

        // Remove the last item on the queue.
        oper = cache->opers;
        prev = 0;
        while ( oper->next != 0 ) {
            prev = oper;
            oper = oper->next;
        }
        if ( prev ) {
            prev->next = oper->next;
        } else {
            cache->opers = oper->next;
        }

        // Process the operation.
        if ( oper->oper == QModemPhoneBookOperation::Add ) {
            flushAdd( oper->entry, cache );
            ++used;
        } else if ( oper->oper == QModemPhoneBookOperation::Remove ) {
            flushRemove( oper->entry.index(), cache );
            --used;
        } else if ( oper->oper == QModemPhoneBookOperation::Update ) {
            flushUpdate( oper->entry, cache );
        }
        changes = true;

        // Delete the operation, which we no longer require.
        delete oper;
    }

    // Emit the entry list if changes were made.
    if ( changes ) {
        if ( used < 0 )
            used = 0;       // Just in case.
        cache->limits.setUsed( (uint)used );
        emit entries( cache->store, cache->entries );
    }
}

void QModemPhoneBook::flushAdd( const QPhoneBookEntry& entry, QModemPhoneBookCache *cache )
{
    uint index, adjindex;

    // Find an unused index in the cache.
    QList<QPhoneBookEntry>::ConstIterator iter;
    unsigned char *flags = new unsigned char
        [((cache->last - cache->first + 1) + 7) / 8];
    ::memset( flags, 0, ((cache->last - cache->first + 1) + 7) / 8 );
    for ( iter = cache->entries.begin();
          iter != cache->entries.end(); ++iter ) {
        index = (*iter).index();
        if ( index >= cache->first && index <= cache->last ) {
            adjindex = index - cache->first;
            flags[adjindex / 8] |= (1 << (adjindex % 8));
        }
    }
    for ( index = cache->first; index <= cache->last; ++index ) {
        adjindex = index - cache->first;
        if ( (flags[adjindex / 8] & (1 << (adjindex % 8))) == 0 ) {
            break;
        }
    }
    delete flags;
    if ( index > cache->last ) {
        // The phone book is full, so we cannot add this entry.
        return;
    }

    // Update the phone device.
    QPhoneBookEntry newEntry( entry );
    newEntry.setIndex( index );
    flushUpdate( newEntry, cache );
}

void QModemPhoneBook::flushRemove( uint index, QModemPhoneBookCache *cache )
{
    // Find the entry in the cache and remove it.
    QList<QPhoneBookEntry>::Iterator iter;
    for ( iter = cache->entries.begin();
          iter != cache->entries.end(); ++iter ) {
        if ( (*iter).index() == index ) {
            cache->entries.erase( iter );
            break;
        }
    }

    // Send the update command to the phone device.
    updateStorageName( cache->store );
    d->service->secondaryAtChat()->chat
        ( "AT+CPBW=" + QString::number( index ) );
}

void QModemPhoneBook::flushUpdate( const QPhoneBookEntry& entry, QModemPhoneBookCache *cache )
{
    // Find the entry in the cache and update it.
    uint index = entry.index();
    QList<QPhoneBookEntry>::Iterator iter;
    bool found = false;
    for ( iter = cache->entries.begin();
          iter != cache->entries.end(); ++iter ) {
        if ( (*iter).index() == index ) {
            (*iter) = entry;
            found = true;
            break;
        }
    }
    if ( !found ) {
        // This is an "add" operation, so add a new entry.
        QPhoneBookEntry newEntry( entry );
        newEntry.setIndex( index );
        cache->entries.append( newEntry );
        if ( hasEmptyPhoneBookIndex() )
            index = 0;
    }

    // Send the update command to the phone device.
    updateStorageName( cache->store );
    QString indexString;
    if ( index == 0 && hasEmptyPhoneBookIndex() )
        indexString = "";
    else
        indexString = QString::number( index );
    d->service->secondaryAtChat()->chat
            ( "AT+CPBW=" + indexString + "," +
              QAtUtils::encodeNumber( entry.number() ) + ",\"" +
              QAtUtils::quote( entry.text(), d->stringCodec ) + "\"" );
}

void QModemPhoneBook::removeAllCaches()
{
    if ( d->caches != 0 ) {
        QModemPhoneBookCache *cache = d->caches;
        while ( cache != 0 ) {
            cache->entries.clear();
            emit entries( cache->store, cache->entries );
            cache = cache->next;
        }
        delete d->caches;
        d->caches = 0;
    }
}

void QModemPhoneBook::cscsDone( bool ok, const QAtResult& result )
{
    if ( ok ) {
        QAtResultParser cmd( result );
        cmd.next( "+CSCS:" );
        QString name = cmd.readString();
        if ( !name.isEmpty() ) {
            updateCodec( name );
        }
        QTimer::singleShot( 8000, this, SLOT(requestStorages()) );
    } else if ( d->charsetRetry-- > 0 ) {
        QTimer::singleShot( 3000, this, SLOT(requestCharset()) );
    } else {
        QTimer::singleShot( 8000, this, SLOT(requestStorages()) );
    }
}

void QModemPhoneBook::cscsSetDone( bool ok, const QAtResult& result )
{
    if ( ok ) {
        QTimer::singleShot( 8000, this, SLOT(requestStorages()) );
    } else if ( d->charsetRetry-- > 0 ) {
        QTimer::singleShot( 3000, this, SLOT(setCharset()) );
    } else {
        QTimer::singleShot( 8000, this, SLOT(requestStorages()) );
    }
}

void QModemPhoneBook::cpbsDone( bool ok, const QAtResult& result )
{
    if ( ok ) {
        QAtResultParser cmd( result );
        if ( cmd.next( "+CPBS:" ) )
        {
            QList<QAtResultParser::Node> nodes = cmd.readList();
            QStringList storages;
            for ( int i = 0; i < nodes.count(); i++ )
                storages.append( nodes.at( i ).asString() );

            setValue( QLatin1String("Storages"), storages );
        }
        emit ready();
    } else if ( d->storageRetry-- > 0 ) {
        QTimer::singleShot( 5000, this, SLOT(requestStorages()) );
    }
}

/*!
    Inform the modem SIM phone book handler that the \c{AT+CSCS} character
    set has been changed to \a gsmCharset.

    \sa stringCodec()
*/
void QModemPhoneBook::updateCodec( const QString& gsmCharset )
{
    d->stringCodec = QAtUtils::codec( gsmCharset );
}

static uint nextNumber( const QString& line, uint& posn )
{
    uint value = 0;
    while ( posn < (uint)(line.length()) && ( line[posn] < '0' || line[posn] > '9' ) ) {
        ++posn;
    }
    while ( posn < (uint)(line.length()) && line[posn] >= '0' && line[posn] <= '9' ) {
        value = value * 10 + (uint)(line[posn].unicode() - '0');
        ++posn;
    }
    return value;
}


void QModemPhoneBook::readDone( bool ok, const QAtResult& result )
{
    QAtResultParser cmd( result );

    // Get the storage area that this response applies to.
    QString store = ((QStoreUserData *)result.userData())->store;
    QModemPhoneBookCache *cache = findCache( store, false );

    // Extract the entries from the response.
    QString line, number, text;
    uint posn, index, type;
    QPhoneBookEntry entry;
    while ( cmd.next( "+CPBR:" ) ) {

        // Parse the contents of the line.
        line = cmd.line();
        posn = 0;
        index = nextNumber( line, posn );
        number = QAtUtils::nextString( line, posn );
        type = nextNumber( line, posn );
        text = QAtUtils::nextString( line, posn );
        if ( type == 145 && ( number.length() == 0 || number[0] != '+' ) ) {
            number = "+" + number;
        }

        // Add the entry to the list.
        entry.setIndex( index );
        entry.setNumber( number );
        entry.setText( QAtUtils::decode( text, d->stringCodec ) );
        cache->entries.append( entry );

    }

    // Determine if the list has finished loading.
    if ( ok && cache->current <= cache->last ) {

        // Fetch the next 10 elements in "slow" mode, or everything if "fast".
        uint first = cache->current;
        uint last = cache->last;
        if ( !(cache->fast) ) {
            if ( ( last - first + 1 ) > 10 ) {
                last = first + 9;
            }
        }
        cache->current = last + 1;

        // Update the current storage name in the phone device.
        updateStorageName( store );

        // Query the extents of the phone book, so we know what range to use.
        QString command;
        command = "AT+CPBR=" + QString::number( first ) +
                  "," + QString::number(last);
        d->pendingCommand = QString();
        d->pendingStore = QString();
        if ( cache->fast ) {
            d->service->secondaryAtChat()->chat
                ( command, this, SLOT(readDone(bool,QAtResult)),
                  new QStoreUserData( store ) );
        } else {
            // Don't send it immediately: wait a bit to give other
            // parts of the system access to the AT handlers too.
            setGsmCharset();
            d->pendingCommand = command;
            d->pendingStore = store;
            d->slowTimer->start( SLOW_UPDATE_TIMEOUT );
        }

    } else {

        // The fetch has completed.
        setGsmCharset();
        readFinished( cache );
    }
}

void QModemPhoneBook::readFinished( QModemPhoneBookCache *cache )
{
    cache->fullyLoaded = true;
    cache->limits.setUsed( (uint)( cache->entries.size() ) );

    // Emit the list of entries if we have a pending "getEntries".
    if ( cache->needEmit ) {
        cache->needEmit = false;
        emit entries( cache->store, cache->entries );
    }

    // Emit the limits if we have a pending "requestLimits()".
    if ( cache->needLimitEmit ) {
        cache->needLimitEmit = false;
        emit limits( cache->store, cache->limits );
    }

    // Execute pending add/remove/update commands.
    flushOperations( cache );
}

void QModemPhoneBook::queryDone( bool ok, const QAtResult& result )
{
    QAtResultParser cmd( result );

    // Get the storage area that this response applies to.
    QString store = ((QStoreUserData *)result.userData())->store;
    QModemPhoneBookCache *cache = findCache( store, false );

    // If the query failed on a modem that caches phone books internally,
    // we need to queue it again for later.
    if ( !ok && hasModemPhoneBookCache() ) {
        cache->redoQuery = true;
        return;
    }

    // If the select failed, then the phone book does not exist and
    // we may have accidentally picked up a different phone book.
    if ( !cache->selectOk ) {
        readFinished( cache );
        return;
    }

    // Extract the extent information for the phone book.
    // We assume that it has the format "(N-M)".
    uint first = 0;
    uint last = 0;
    uint nlength = 0;
    uint tlength = 0;
    if ( cmd.next( "+CPBR:" ) ) {

        QString line = cmd.line();
        int posn = 0;
        while ( posn < line.length() && line[posn] != '(' ) {
            ++posn;
        }
        if ( posn < line.length() ) {
            ++posn;
            while ( posn < line.length() && line[posn] >= '0' &&
                    line[posn] <= '9' ) {
                first = first * 10 + (uint)(line[posn].unicode() - '0');
                ++posn;
            }
        }
        while ( posn < line.length() && line[posn] != ')' ) {
            ++posn;
        }
        uint end = (uint)(posn + 1);
        if ( posn < line.length () ) {
            uint mult = 1;
            while ( posn > 0 && line[posn - 1] >= '0' &&
                    line[posn - 1] <= '9' ) {
                --posn;
                last += mult * (uint)(line[posn].unicode() - '0');
                mult *= 10;
            }
        }
        nlength = QAtUtils::parseNumber( line, end );
        tlength = QAtUtils::parseNumber( line, end );

    }

    // Record the storage limits in the cache.
    cache->first = first;
    cache->last = last;
    cache->limits.setFirstIndex( first );
    cache->limits.setLastIndex( last );
    cache->limits.setNumberLength( nlength );
    cache->limits.setTextLength( tlength );

    // Determine how many entries to fetch.  In "slow loading"
    // mode, we don't load more than 10 entries at a time, so
    // that other things in the system have a chance to work.
    if ( !(cache->fast) ) {
        if ( ( last - first + 1 ) > 10 ) {
            last = first + 9;
        }
    }
    cache->current = last + 1;

    // Update the storage name in the device again, just in case a
    // request for another phone book arrived in the meantime.
    updateStorageName( store );

    // Send the read command to get the entries.
    QString command;
    command = "AT+CPBR=" + QString::number( first ) +
              "," + QString::number(last);
    setUcs2Charset();
    d->service->secondaryAtChat()->chat
        ( command, this, SLOT(readDone(bool,QAtResult)),
          new QStoreUserData( store ) );
}

void QModemPhoneBook::slowTimeout()
{
    if ( !d->pendingCommand.isEmpty() ) {
        setUcs2Charset();
        d->service->secondaryAtChat()->chat
            ( d->pendingCommand, this, SLOT(readDone(bool,QAtResult)),
              new QStoreUserData( d->pendingStore ) );
        d->pendingCommand = QString();
        d->pendingStore = QString();
    }
}

void QModemPhoneBook::fdQueryDone( bool, const QAtResult& result )
{
    bool enabled = false;
    QAtResultParser parser( result );
    if ( parser.next( "+CLCK:" ) ) {
        enabled = ( parser.readNumeric() != 0 );
    }
    emit fixedDialingState( enabled );
}

void QModemPhoneBook::fdModifyDone( bool, const QAtResult& result )
{
    emit setFixedDialingStateResult( (QTelephony::Result)result.resultCode() );
}

void QModemPhoneBook::requestCharset()
{
    d->service->secondaryAtChat()->chat
        ( "AT+CSCS?", this, SLOT(cscsDone(bool,QAtResult)) );
}

void QModemPhoneBook::setUcs2Charset()
{
    d->stringCodec = QAtUtils::codec( "UCS2" );    
    d->service->secondaryAtChat()->chat( "AT+CSCS=\"UCS2\"" );
}

void QModemPhoneBook::setGsmCharset()
{
    d->stringCodec = QAtUtils::codec( "GSM" );
    d->service->secondaryAtChat()->chat( "AT+CSCS=\"GSM\"" );
}

void QModemPhoneBook::requestStorages()
{
    d->service->secondaryAtChat()->chat
        ( "AT+CPBS=?", this, SLOT(cpbsDone(bool,QAtResult)) );
}
