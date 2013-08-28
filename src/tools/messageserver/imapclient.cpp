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

#include "imapclient.h"
#include <private/longstream_p.h>

#include <QMailAccount>
#include <QMailStore>
#include <QMailFolder>


namespace QtMail
{

    QString messageMailbox(const QString& uid)
    {
        int index = 0;
        if ((index = uid.lastIndexOf('|')) != -1)
            return uid.left(index);
        else 
            return QString();
    }

    ImapClient::FolderStatus folderStatusFlags(const QString &flags)
    {
        int status = 0;

        if (flags.indexOf("NoInferiors", 0, Qt::CaseInsensitive) != -1)
            status |= ImapClient::NoInferiors;
        if (flags.indexOf("NoSelect", 0, Qt::CaseInsensitive) != -1)
            status |= ImapClient::NoSelect;
        if (flags.indexOf("Marked", 0, Qt::CaseInsensitive) != -1)
            status |= ImapClient::Marked;
        if (flags.indexOf("Unmarked", 0, Qt::CaseInsensitive) != -1)
            status |= ImapClient::Unmarked;

        return static_cast<ImapClient::FolderStatus>(status);
    }

    QString decodeModBase64(QString in)
    {
        //remove  & -
        in.remove(0,1);
        in.remove(in.length()-1,1);

        if(in.isEmpty())
            return "&";

        QByteArray buf(in.length(),static_cast<char>(0));
        QByteArray out(in.length() * 3 / 4 + 2,static_cast<char>(0));

        //chars to numeric
        QByteArray latinChars = in.toLatin1();
        for (int x = 0; x < in.length(); x++) {
            int c = latinChars[x];
            if ( c >= 'A' && c <= 'Z')
                buf[x] = c - 'A';
            if ( c >= 'a' && c <= 'z')
                buf[x] = c - 'a' + 26;
            if ( c >= '0' && c <= '9')
                buf[x] = c - '0' + 52;
            if ( c == '+')
                buf[x] = 62;
            if ( c == ',')
                buf[x] = 63;
        }

        int i = 0; //in buffer index
        int j = i; //out buffer index

        unsigned char z;
        QString result;

        while(i+1 < buf.size())
        {
            out[j] = buf[i] & (0x3F); //mask out top 2 bits
            out[j] = out[j] << 2;
            z = buf[i+1] >> 4;
            out[j] = (out[j] | z);      //first byte retrieved

            i++;
            j++;

            if(i+1 >= buf.size())
                break;

            out[j] = buf[i] & (0x0F);   //mask out top 4 bits
            out[j] = out[j] << 4;
            z = buf[i+1] >> 2;
            z &= 0x0F;
            out[j] = (out[j] | z);      //second byte retrieved

            i++;
            j++;

            if(i+1 >= buf.size())
                break;

            out[j] = buf[i] & 0x03;   //mask out top 6 bits
            out[j] = out[j] <<  6;
            z = buf[i+1];
            out[j] = out[j] | z;  //third byte retrieved

            i+=2; //next byte
            j++;
        }

        //go through the buffer and extract 16 bit unicode network byte order
        for(int z = 0; z < out.count(); z+=2) {
            unsigned short outcode = 0x0000;
            outcode = out[z];
            outcode <<= 8;
            outcode &= 0xFF00;

            unsigned short b = 0x0000;
            b = out[z+1];
            b &= 0x00FF;
            outcode = outcode | b;
            if(outcode)
                result += QChar(outcode);
        }

        return result;
    }

    QString decodeModUTF7(QString in)
    {
        QRegExp reg("&[^&-]*-");

        int startIndex = 0;
        int endIndex = 0;

        startIndex = in.indexOf(reg,endIndex);
        while (startIndex != -1) {
            endIndex = startIndex;
            while(endIndex < in.length() && in[endIndex] != '-')
                endIndex++;
            endIndex++;

            //extract the base64 string from the input string
            QString mbase64 = in.mid(startIndex,(endIndex - startIndex));
            QString unicodeString = decodeModBase64(mbase64);

            //remove encoding
            in.remove(startIndex,(endIndex-startIndex));
            in.insert(startIndex,unicodeString);

            endIndex = startIndex + unicodeString.length();
            startIndex = in.indexOf(reg,endIndex);
        }

        return in;
    }

    QString decodeFolderName(const QString &name)
    {
        return decodeModUTF7(name);
    }

}

ImapClient::ImapClient(QObject* parent)
    : Client(parent),
      supportsIdle(false),
      idling(false),
      retryDelay(5), // seconds
      waitingForIdle(false),
      folders(false)
{
    connect(&client, SIGNAL(finished(ImapCommand&,OperationState&)),
            this, SLOT(operationDone(ImapCommand&,OperationState&)) );
    connect(&client, SIGNAL(mailboxListed(QString&,QString&,QString&)),
            this, SLOT(mailboxListed(QString&,QString&,QString&)) );
    connect(&client, SIGNAL(messageFetched(QMailMessage&)),
            this, SLOT(messageFetched(QMailMessage&)) );
    connect(&client, SIGNAL(nonexistentMessage(QString,Client::DefunctReason)),
            this, SIGNAL(nonexistentMessage(QString,Client::DefunctReason)) );
    connect(&client, SIGNAL(downloadSize(int)),
            this, SLOT(downloadSize(int)) );
    connect(&client, SIGNAL(updateStatus(QString)),
            this, SLOT(transportStatus(QString)) );
    connect(&client, SIGNAL(connectionError(int,QString)),
            this, SLOT(errorHandling(int,QString)) );

    connect(&idleConnection, SIGNAL(finished(ImapCommand&,OperationState&)),
            this, SLOT(idleOperationDone(ImapCommand&,OperationState&)) );
    connect(&idleConnection, SIGNAL(updateStatus(QString)),
            this, SLOT(transportStatus(QString)));
    connect(&idleConnection, SIGNAL(connectionError(int,QString)),
            this, SLOT(idleErrorHandling(int,QString)) );
    connect(&idleTimer, SIGNAL(timeout()),
            this, SLOT(idleTimeOut()));
}

ImapClient::~ImapClient()
{
}

void ImapClient::newConnection()
{
    if (client.inUse())
        return;

    // Reload the account configuration
    _config = AccountConfiguration(accountId);
    if ( _config.mailServer().isEmpty() ) {
        QString msg("Cannot send message without IMAP server configuration");
        errorHandling(QMailMessageServer::ErrConfiguration, msg);
        return;
    }

    status = Init;

    folderStatus.clear();
    selected = false;
    tlsEnabled = false;
    messageCount = 0;
    listSize = 0;

    currentMailbox = QMailFolder();
    mailboxList.clear();

    _retrieveUids.clear();

    client.open(_config);
}

void ImapClient::foldersOnly(bool set)
{
    folders = set;
}

void ImapClient::idleOperationDone(ImapCommand &command, OperationState &state)
{
    ImapCommand nextCommand = IMAP_Idle_Continuation;
    OperationState nextState = OpOk;
    const int idleTimeout = 28*60*1000;
    if ( state != OpOk ) {
        qLog(IMAP) << "IDLE: IMAP Idle connection failed";
        if (idleConnection.inUse())
            idleConnection.close();
        // Consider retrying, say 10 times?
        operationDone(nextCommand, nextState);
        return;
    }
    
    switch( command ) {
        case IMAP_Init:
        {
            idleConnection.login(_config.mailUserName(), _config.mailPassword());
            return;
        }
        case IMAP_Login:
        {
            idleConnection.select("INBOX");
            return;
        }
        case IMAP_Select:
        {
            idleConnection.idle();
            return;
        }
        case IMAP_Idle:
        {
            if (idling) {
                emit newMailDiscovered(accountId);
            } else {
                qLog(IMAP) << "IDLE: Idle connection established.";
                idling = true;
                idleTimer.start( idleTimeout );
                if (waitingForIdle)
                    operationDone(nextCommand, nextState);
            }
            return;
        }
        case IMAP_Idle_Done:
        {
            idleConnection.idle();
            return;
        }
        default:        //default = all critical messages
        {
            qLog(IMAP) << "IDLE: IMAP Idle unknown command response: " << command;
            return;
        }
    }
}

void ImapClient::idleTimeOut()
{
    idling = false;
    idleTimer.stop();
    idleConnection.idleDone();
}

void ImapClient::operationDone(ImapCommand &command, OperationState &state)
{
    if ( state != OpOk ) {
        switch ( command ) {
            case IMAP_UIDStore:
            {
                // Couldn't set a flag, ignore as we can stil continue
                qLog(IMAP) << "could not store message flag";
                break;
            }

            case IMAP_Login:
            {
                errorHandling(QMailMessageServer::ErrLoginFailed, client.lastError() );
                return;
            }

            case IMAP_Full:
            {
                errorHandling(QMailMessageServer::ErrFileSystemFull, client.lastError() );
                return;
            }

            default:        //default = all critical messages
            {
                errorHandling(QMailMessageServer::ErrUnknownResponse, client.lastError() );
                return;
            }
        }
    }

    switch( command ) {
        case IMAP_Init:
        {
            emit updateStatus( QMailMessageServer::None, tr("Checking capabilities" ) );
            client.capability();
            break;
        }
        case IMAP_Capability:
        {
#ifndef QT_NO_OPENSSL
            if (!tlsEnabled && (_config.mailEncryption() == AccountConfiguration::Encrypt_TLS)) {
                if (client.supportsCapability("STARTTLS")) {
                    emit updateStatus( QMailMessageServer::None, tr("Starting TLS" ) );
                    client.startTLS();
                    break;
                } else {
                    // TODO: request user direction
                    qWarning() << "No TLS support - continuing unencrypted";
                }
            }
#endif
            supportsIdle = client.supportsCapability("IDLE");
            if (!idleConnection.connected() 
                && supportsIdle 
                && _config.pushEnabled()) {
                waitingForIdle = true;
                emit updateStatus( QMailMessageServer::None, tr("Logging in idle connection" ) );
                idleConnection.open(_config);
            } else {
                if (!_config.pushEnabled() && idleConnection.connected())
                    idleConnection.close();
                emit updateStatus( QMailMessageServer::None, tr("Logging in" ) );
                client.login(_config.mailUserName(), _config.mailPassword());
            }
            break;
        }
        case IMAP_Idle_Continuation:
        {
            waitingForIdle = false;
            emit updateStatus( QMailMessageServer::None, tr("Logging in" ) );
            client.login(_config.mailUserName(), _config.mailPassword());
            break;
        }
        case IMAP_StartTLS:
        {
            // We are now in TLS mode
            tlsEnabled = true;

            // Check capabilities for encrypted mode
            client.capability();
            break;
        }
        case IMAP_Login:
        {
            emit updateStatus( QMailMessageServer::None, tr("Retrieving folders") );
            mailboxNames.clear();

            if ( selected ) {
                fetchNextMail();    //get selected messages only
            } else {
                status = List;
                client.list(_config.baseFolder(), "*");        //_config.baseFolder() == root folder
            }
            break;
        }
        case IMAP_List:
        {
            removeDeletedMailboxes();

            if (folders) {
                // We have retrieved all the folders
                retrieveOperationCompleted();
                closeConnection();
            } else if (!selectNextMailbox()) {
                // Could be no mailbox has been selected to be stored locally
                retrieveOperationCompleted();
            }
            break;
        }
        case IMAP_Select:
        {
            handleSelect();
            break;
        }
        case IMAP_UIDSearch:
        {
            handleSearch();
            break;
        }
        case IMAP_UIDFetch:
        {
            handleUidFetch();
            break;
        }

        case IMAP_UIDStore:
        {
            if (!setNextSeen())
                if (!setNextDeleted())
                    handleUid();
            break;
        }

        case IMAP_Expunge:
        {
            // Deleted messages, we can handle UID now
            handleUid();
            break;
        }

        case IMAP_Logout:
        {
            retrieveOperationCompleted();
            return;

            break;
        }

        case IMAP_Full:
        {
            qFatal( "Logic error, IMAP_Full" );
            break;
        }
        
        case IMAP_Idle:
        case IMAP_Idle_Done:
        {
            qLog(IMAP) << "Unexpected idle response"; // shouldn't happen ever
            break;
        }

    }
}

bool ImapClient::selectNextMailbox()
{
    _seenUids = QStringList();
    _unseenUids = QStringList();
    _newUids = QStringList();
    _readUids = QStringList();
    _removedUids = QStringList();
    _expungeRequired = false;
    _searchStatus = Seen;

    if (nextMailbox()) {
        if (status == List) {
            emit updateStatus( QMailMessageServer::Retrieve, tr("Checking", "Checking <mailbox name>") + QChar(' ') + currentMailbox.displayName() );
        }

        FolderStatus folderState = folderStatus[currentMailbox.id()];
        if (folderState & NoSelect) {
            // Bypass the actual select, and go directly to the search result handler
            searchCompleted();
        } else {
            client.select( currentMailbox.name() );
        }
        return true;
    } else {
        return false;
    }
}

void ImapClient::handleUidFetch()
{
    if (status == Fetch) {    //getting headers
        handleUid();
    } else if (status == Retrieve) {    //getting complete messages
        fetchNextMail();
    }
}

void ImapClient::handleSearch()
{
    switch(_searchStatus)
    {
    case Seen:
    {
        _seenUids = client.mailboxUidList();

        _searchStatus = Unseen;
        client.uidSearch(MFlag_Unseen);
        break;
    }
    case Unseen:
    {
        _unseenUids = client.mailboxUidList();

        if ((_unseenUids.count() + _seenUids.count()) == client.exists()) {
            // We have a consistent set of search results
            searchCompleted();
        } else {
            qLog(IMAP) << "Inconsistent UID SEARCH result using SEEN/UNSEEN; reverting to ALL";

            // Try doing a search for ALL messages
            _unseenUids.clear();
            _seenUids.clear();
            _searchStatus = All;
            client.uidSearch(MFlag_All);
        }
        break;
    }
    case All:
    {
        _unseenUids = client.mailboxUidList();
        if (_unseenUids.count() != client.exists()) {
            qLog(IMAP) << "Inconsistent UID SEARCH result";

            // No consistent search result, so don't delete anything
            _searchStatus = Inconclusive;
        }

        searchCompleted();
        break;
    }
    default:
        qLog(IMAP) << "Unknown search status";
    }
}

static QStringList inFirstAndSecond(const QStringList &first, const QStringList &second)
{
    QStringList result;

    foreach (const QString &value, first)
        if (second.contains(value))
            result.append(value);

    return result;
}

static QStringList inFirstButNotSecond(const QStringList &first, const QStringList &second)
{
    QStringList result;

    foreach (const QString &value, first)
        if (!second.contains(value))
            result.append(value);

    return result;
}

void ImapClient::searchCompleted()
{
    QMailFolderId boxId = currentMailbox.id();

    if ((currentMailbox.status() & QMailFolder::SynchronizationEnabled) &&
        !(currentMailbox.status() & QMailFolder::Synchronized)) {
        // We have just synchronized this folder
        QMailFolder folder(boxId);
        folder.setStatus(QMailFolder::Synchronized, true);
        bool ok = QMailStore::instance()->updateFolder(&folder);
        Q_ASSERT(ok);
        Q_UNUSED(ok);
    }

    // Compare the server message list with our message list
    QStringList reportedUids = _seenUids + _unseenUids;

    QMailAccount account(accountId);
    QStringList readElsewhereUids = account.serverUids(boxId, QMailMessage::ReadElsewhere);
    QStringList unreadElsewhereUids = account.serverUids(boxId, QMailMessage::ReadElsewhere, false);

    // "deleted" here means messages that have been deleted from the
    // phone (i.e. from the Qtopia database) but may still exist on
    // the IMAP server.  Code below will delete these from the IMAP
    // server too.
    QStringList deletedUids = account.deletedMessages(boxId);

    // The following generates a list of all the UIDs that have ever
    // previously been seen on the phone.
    QStringList storedUids = readElsewhereUids + unreadElsewhereUids + deletedUids;

    // New messages reported by the server that we don't yet have
    QStringList newUids = inFirstButNotSecond(reportedUids, storedUids);
    if (!newUids.isEmpty()) {
        // Add this folder to the list to retrieve from later
        _retrieveUids.append(qMakePair(boxId, newUids));
    }

    if (_searchStatus == Inconclusive) {
        // Don't mark or delete any messages without a correct server listing
        handleUid();
    } else {
        // Only delete messages the server still has
        _removedUids = inFirstAndSecond(deletedUids, reportedUids);
        _expungeRequired = !_removedUids.isEmpty();

        // Messages marked read locally that the server reports are unseen
        _readUids = inFirstAndSecond(account.serverUids(boxId, QMailMessage::Read), _unseenUids);

        // If there are messages that exist on the phone but have
        // since been deleted from the IMAP server, add the
        // QMailMessage::Removed flag to those messages.  Note that we
        // don't automatically delete those messages from the phone.
	//
	// Optimize this a bit by skipping phone messages that already
	// have the QMailMessage::Removed flag.  Otherwise, in a
	// scenario where a lot of messages are deleted from the IMAP
	// server - but still exist on the phone - all of those
	// messages are reprocessed every time we sync with the IMAP
	// server, and that can take significant time and CPU.
        foreach (const QString &uid,
		 inFirstButNotSecond(inFirstButNotSecond(storedUids, reportedUids),
				     account.serverUids(boxId, QMailMessage::Removed)))
            emit nonexistentMessage(uid, Client::Removed);

        // Update any messages that are reported read-elsewhere, that we didn't previously know about
        foreach (const QString &uid, inFirstAndSecond(_seenUids, unreadElsewhereUids)) {
            // We know of this message, but we have it marked as unread
            QMailMessageMetaData metaData(uid, accountId);
            metaData.setStatus(QMailMessage::ReadElsewhere, true);
            bool ok = QMailStore::instance()->updateMessage(&metaData);
            Q_ASSERT(ok);
            Q_UNUSED(ok);
        }

        // Mark any messages that we have read that the server thinks are unread
        if (!setNextSeen())
            if (!setNextDeleted())
                handleUid();
    }
}

bool ImapClient::setNextSeen()
{
    if (!_readUids.isEmpty()) {
        msgUidl = _readUids.first();
        _readUids.removeAll( msgUidl );

        emit updateStatus( QMailMessageServer::None, tr("Marking message %1 read").arg(msgUidl) );
        client.uidStore(MFlag_Seen, ImapProtocol::uid(msgUidl));
        return true;
    }

    return false;
}

bool ImapClient::setNextDeleted()
{
    if (_config.canDeleteMail()) {
        if (!_removedUids.isEmpty()) {
            msgUidl = _removedUids.first();
            _removedUids.removeAll( msgUidl );

            emit updateStatus( QMailMessageServer::None, tr("Deleting message %1").arg(msgUidl) );

            //remove records of deleted messages
            QMailStore::instance()->purgeMessageRemovalRecords(accountId, QStringList() << msgUidl);

            client.uidStore(MFlag_Deleted, ImapProtocol::uid(msgUidl));
            return true;
        } else if (_expungeRequired) {
            // All messages flagged as deleted, expunge them
            client.expunge();
            return true;
        }
    }

    return false;
}

void ImapClient::handleUid()
{
    if (_newUids.count() > 0) {
        // TODO: We should find a compromise between retrieving all messages and retrieving messages individually:
        //client.uidFetch(F_Uid | F_Rfc822_Size | F_Rfc822_Header, ImapProtocol::uidRange(_newUids.first(), _newUids.last()) );
        client.uidFetch(F_Uid | F_Rfc822_Size | F_Rfc822_Header, ImapProtocol::uid(_newUids.takeFirst()));
    } else if (!selectNextMailbox()) {
        if ((status == Fetch) || (_retrieveUids.isEmpty())) {
            previewCompleted();
        } else {
            // We now have a list of all messages to be retrieved for each mailbox
            uint totalMessages = 0;
            QList<QPair<QMailFolderId, QStringList> >::const_iterator it = _retrieveUids.begin(), end = _retrieveUids.end();
            for ( ; it != end; ++it)
                totalMessages += it->second.count();

            emit fetchTotal(totalMessages);
            emit updateStatus(QMailMessageServer::Retrieve, tr("Previewing", "Previewing <number of messages>") +QChar(' ') + QString::number(totalMessages));

            fetchedCount = 0;
            status = Fetch;
            selectNextMailbox();
        }
    }
}

void ImapClient::handleSelect()
{
    // We have selected the current mailbox
    if (status == Retrieve) {
        // We're completing a message
        emit updateStatus( QMailMessageServer::Retrieve, tr("Completing %1 / %2").arg(messageCount).arg(listSize) );
        client.uidFetch( F_Uid | F_Rfc822_Size | F_Rfc822, ImapProtocol::uid(msgUidl) );
        return;
    } else if (status == Fetch) {
        // We're retrieving message metadata
        handleUid();
    } else {
        // We're searching mailboxes
        if (client.exists() > 0) {
            // Start by looking for previously-seen messages
            client.uidSearch(MFlag_Seen);
        } else {
            // No messages, so no need to perform search
            searchCompleted();
        }
    }
}

bool ImapClient::nextMailbox()
{
    while (!mailboxList.isEmpty()) {
        currentMailbox = QMailFolder(mailboxList.takeFirst());
        if (currentMailbox.status() & QMailFolder::SynchronizationEnabled)
            return true;
    }

    // We have exhausted the list of mailboxes
    if ((status == Fetch) && !_retrieveUids.isEmpty()) {
        // In fetch mode, return the mailboxes where retrievable messages are located
        QPair<QMailFolderId, QStringList> next = _retrieveUids.takeFirst();
        currentMailbox = QMailFolder(next.first);
        _newUids = next.second;
        return true;
    }

    currentMailbox = QMailFolder();
    return false;
}

void ImapClient::fetchNextMail()
{
    QMailFolderId mailboxId;

    MessageMap::ConstIterator selectionEnd = folderItr.value().end();

    QString serverUid;
    if (selectionItr == selectionEnd) {
        ++folderItr;
        if (folderItr != selectionMap.end()) {
            selectionItr = folderItr.value().begin();
            selectionEnd = folderItr.value().end();
        }
    } 

    if (selectionItr != selectionEnd) {
        serverUid = selectionItr.key();
        mailboxId = folderItr.key();

        ++selectionItr;
        ++messageCount;
    }

    if (mailboxId.isValid()) {
        if (mailboxId != currentMailbox.id())
            currentMailbox = QMailFolder(mailboxId);

        status = Retrieve;
        retrieveUid = serverUid;
        if ((currentMailbox.name()) == client.selected()) {
            emit updateStatus( QMailMessageServer::Retrieve, tr("Completing %1 / %2").arg(messageCount).arg(listSize) );
            msgUidl = serverUid;
            client.uidFetch( F_Uid | F_Rfc822_Size | F_Rfc822, ImapProtocol::uid(msgUidl) );
            return;

        } else {
            msgUidl = serverUid;
            client.select( currentMailbox.name() );
            return;
        }
    } else {
        currentMailbox = QMailFolder();
        closeConnection();
    }
}

/*  Mailboxes retrived from the server goes here.  If the INBOX mailbox
    is new, it means it is the first time the account is used.
*/
void ImapClient::mailboxListed(QString &flags, QString &delimiter, QString &name)
{
    QMailFolderId parentId;
    QMailFolderId boxId;

    QMailAccount account(accountId);

    QString item;
    QStringList list = name.split(delimiter);
    for ( QStringList::Iterator it = list.begin(); it != list.end(); ++it ) {
        if (!item.isEmpty())
            item.append(delimiter);
        item.append(*it);

        boxId = account.getMailbox(item);
        if (boxId.isValid()) {
            // This element already exists
            parentId = boxId;
        } else {
            // This element needs to be created
            QMailFolder folder(item, parentId, accountId);
            folder.setDisplayName(QtMail::decodeFolderName(*it));
            folder.setStatus(QMailFolder::SynchronizationEnabled, true);

            bool ok = QMailStore::instance()->addFolder(&folder);
            Q_ASSERT(ok);
            Q_UNUSED(ok);
            boxId = folder.id();
        }
    }

    if (boxId.isValid()) {
        Q_ASSERT(item == name);
        folderStatus[boxId] = QtMail::folderStatusFlags(flags);
    }

    mailboxNames.append(name);
}

void ImapClient::messageFetched(QMailMessage& mail)
{     //set some other parameters

    mail.setParentAccountId(accountId);
    mail.setFromMailbox(currentMailbox.name());
    mail.setParentFolderId(currentMailbox.id());

    bool isPartial(status == Fetch);
    mail.setStatus(QMailMessage::Downloaded, !isPartial);

    if (selected && folderItr.value().contains(retrieveUid)) {
        mail.setId(folderItr.value().value(retrieveUid));
    }

    emit newMessage(mail, isPartial);

    if (!retrieveUid.isEmpty()) {
        emit messageProcessed(retrieveUid);
        retrieveUid = QString();
    }

    if (isPartial) {
        emit fetchProgress(fetchedCount++);
    }
}

void ImapClient::setAccount(const QMailAccountId &id)
{
    if (client.inUse() && (id != accountId)) {
        QString msg("Cannot send message; socket in use");
        errorHandling(QMailMessageServer::ErrConnectionInUse, msg);
        return;
    }

    accountId = id;
}

QMailAccountId ImapClient::account() const
{
    return accountId;
}

void ImapClient::errorHandling(int code, QString msg)
{
    if ( client.inUse() || (status == Init) ) {
        client.close();
    }

    emit updateStatus(QMailMessageServer::None, tr("Error occurred"));
    emit errorOccurred(code, msg);
}

void ImapClient::idleErrorHandling(int, QString)
{
    qLog(IMAP) << "IDLE: An IMAP IDLE related error occurred."
               << "An attempt to automatically recover is scheduled in"
               << retryDelay << "seconds.";
    if (idleConnection.inUse())
        idleConnection.close();

    emit updateStatus(QMailMessageServer::None, tr("Idle Error occurred"));
    QTimer::singleShot(retryDelay*1000, this, SLOT(idleErrorRecovery()));
}

void ImapClient::idleErrorRecovery()
{
    const int oneHour = 60*60;
    if (idleConnection.connected()) {
        qLog(IMAP) << "IDLE: IMAP IDLE error recovery was successful.";
        retryDelay = 5;
        return;
    }
    qLog(IMAP) << "IDLE: IMAP IDLE error recovery failed. Retrying in"
               << retryDelay << "seconds.";
    retryDelay = qMin( oneHour, retryDelay*2 );
    QTimer::singleShot(retryDelay*1000, this, SLOT(idleErrorRecovery()));
    
    // Force new mail check which will reestablish idle connection
    emit newMailDiscovered(accountId);
}

void ImapClient::closeConnection()
{
    if ( client.connected() ) {
        emit updateStatus( QMailMessageServer::None, tr("Logging out") );
        client.logout();
    } else if ( client.inUse() ) {
        client.close();
    }
}

void ImapClient::setSelectedMails(const SelectionMap& data) 
{
    selected = true;
    selectionMap = data;
    folderItr = selectionMap.begin();
    selectionItr = folderItr.value().begin();

    listSize = 0;
    SelectionMap::const_iterator it = data.begin(), end = data.end();
    for ( ; it != end; ++it)
        listSize += it.value().count();

    messageCount = 0;
    mailDropSize = 0;

    if ( client.connected() && (status == Fetch) ) {
        fetchNextMail();
    }
}

/*  removes any mailboxes form the client list which no longer is
    registered on the server.
*/
void ImapClient::removeDeletedMailboxes()
{
    // We now have the full list of account mailboxes
    QMailAccount account(accountId);
    mailboxList = account.mailboxes();

    QMailFolderIdList nonexistent;
    foreach (const QMailFolderId &boxId, mailboxList) {
        QMailFolder mailbox(boxId);
        bool exists = mailboxNames.contains(mailbox.name());
        if (!exists) {
            nonexistent.append(mailbox.id());
        }
    }

    foreach (const QMailFolderId &boxId, nonexistent) {
        // Any messages in this box should be removed also
        foreach (const QString& uid, account.serverUids(boxId))
            emit nonexistentMessage(uid, Client::FolderRemoved);

        bool result = QMailStore::instance()->removeFolder(boxId);
        Q_ASSERT(result);
        Q_UNUSED(result);

        mailboxList.removeAll(boxId);
    }
}

void ImapClient::checkForNewMessages()
{
    emit allMessagesReceived();
}

void ImapClient::transportStatus(const QString& status)
{
    emit updateStatus(QMailMessageServer::None, status);
}

void ImapClient::cancelTransfer()
{
    errorHandling(QMailMessageServer::ErrCancel, tr("Cancelled by user"));
}

void ImapClient::previewCompleted()
{
    emit partialRetrievalCompleted();
}

void ImapClient::downloadSize(int length)
{
    if (!retrieveUid.isEmpty())
        emit retrievalProgress(retrieveUid, length);
}

void ImapClient::retrieveOperationCompleted()
{
    // This retrieval may have been asynchronous
    emit allMessagesReceived();

    // Or it may have been requested by a waiting client
    emit retrievalCompleted();
}

