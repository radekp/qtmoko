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

// Needed to give friend access to the function defined by Q_GLOBAL_STATIC
#define QMAILSTOREINSTANCE_DEFINED_HERE
#include "qmailstore.h"
#include "qmailstore_p.h"
#include "qmailfolder.h"
#include "qmailmessage.h"
#include "qmailmessagekey.h"
#include "qmailmessagesortkey.h"
#include "qmailfolderkey.h"
#include "qmailfoldersortkey.h"
#include "qmailaccountkey.h"
#include "qmailaccountsortkey.h"
#include "qmailid.h"
#include "qmailtimestamp.h"
#include "qmailaccount.h"
#include "qmailmessageremovalrecord.h"
#include <private/accountconfiguration_p.h>

#include <qtopiasql.h>
#include <qtopianamespace.h>
#include <QDebug>
#include <QContent>
#include <qtopialog.h>
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlRecord>
#include <QSet>
#include <QSettings>

#include <algorithm>
#include <unistd.h>

// When using GCC 4.1.1 on ARM, TR1 functional cannot be included when RTTI
// is disabled, since it automatically instantiates some code using typeid().
//#include <tr1/functional>
//using std::tr1::bind;
//using std::tr1::cref;

// Provide the small parts of functional we use - binding only to member functions, 
// with up to 4 function parameters, and with crefs only to value types.
#include "bind_p.h"

using nonstd::tr1::bind;
using nonstd::tr1::cref;


// Note on retry logic - it appears that SQLite3 will return a SQLITE_BUSY error (5)
// whenever there is contention on file locks or mutexes, and that these occurrences
// are not handled by the handler installed by either sqlite3_busy_timeout or 
// sqlite3_busy_handler.  Furthermore, the comments for sqlite3_step state that if
// the SQLITE_BUSY error is returned whilst in a transaction, the transaction should
// be rolled back.  Therefore, it appears that we must handle this error by retrying
// at the QMailStore level, since this is the level where we perform transactions. 

const int Sqlite3BusyErrorNumber = 5;


/*!
    \class QMailStore
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule

    \preliminary
    \brief The QMailStore class represents the main interface for storage and retrieval
    of messages and folders on the message store.
     
    \ingroup messaginglibrary

    The QMailStore class is accessed through a singleton interface and provides functions 
    for adding, updating and deleting of QMailAccounts, QMailFolders and QMailMessages on the message store.
    
    QMailStore also provides functions for querying and counting of QMailFolders, QMailAccounts and QMailMessages
    when used in conjunction with QMailMessageKey, QMailFolderKey and QMailAccountKey classes.

    \sa QMailMessage, QMailFolder, QMailMessageKey, QMailFolderKey, QMailAccountKey
*/

/*!
    \enum QMailStore::MessageRemovalOption

    Defines whether or not a QMailMessageRemovalRecord is created upon message removal. 

    \value NoRemovalRecord Do not create a QMailMessageRemovalRecord upon message removal. 
    \value CreateRemovalRecord Create a QMailMessageRemovalRecord upon message removal. 
*/

/*!
    Constructs a new QMailStore object and opens the message store database.
*/
QMailStore::QMailStore()
{
    //create the db and sql interfaces
    d = new QMailStorePrivate(this);
}

/*!
    Destroys this QMailStore object.
*/
QMailStore::~QMailStore()
{
    delete d; d = 0;
}

/*!
    Returns true if the QMailStore object was correctly initialized.
*/

bool QMailStore::initialized() const
{
    return QMailStore::storeInitialized();
}

/*!
    Returns true if the QMailStore was correctly initialized.
*/

bool QMailStore::storeInitialized()
{
    return QMailStorePrivate::initialized();
}

/*!
    Adds a new QMailAccount object \a account into the messsage store, with the
    configuration details optionally specified by \a config.
    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::addAccount(QMailAccount* account, AccountConfiguration* config)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptAddAccount, this, account, config), "addAccount");
}

/*!
    Adds a new QMailFolder object \a folder into the message store, performing
    respective integrity checks. Returns \c true if the operation
    completed successfully, \c false otherwise. 
*/
bool QMailStore::addFolder(QMailFolder* folder)
{   
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptAddFolder, this, folder), "addFolder");
}

/*!
    Adds a new QMailMessage object \a msg into the message store, performing
    respective integrity checks. Returns \c true if the operation
    completed successfully, \c false otherwise. 
*/
bool QMailStore::addMessage(QMailMessage* msg)
{
    if (!msg->parentFolderId().isValid()) {
        qLog(Messaging) << "Unable to add message. Invalid parent folder id";
        return false;
    }

    QString mailfile(d->generateUniqueFileName());
    if (!d->saveMessageBody(*msg,mailfile)) {
        qLog(Messaging) << "Could not store message body to file" << mailfile;
        return false;
    }

    if (!addMessage(msg, mailfile)) {
        if (!d->removeMessageBody(mailfile))
            qLog(Messaging) << "Could not remove temp mail body" << mailfile;
        return false;
    }

    return true;
}

/*!
    Adds a new QMailMessageMetaData object \a metaData into the message store, performing
    respective integrity checks. Returns \c true if the operation completed 
    successfully, \c false otherwise. 
*/
bool QMailStore::addMessage(QMailMessageMetaData* metaData)
{
    return addMessage(metaData, QString());
}

/*! \internal */
bool QMailStore::addMessage(QMailMessageMetaData* metaData, const QString& mailfile)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptAddMessage, this, metaData, cref(mailfile)), "addMessage");
}

/*!
    Removes a QMailAccount with QMailAccountId \a id from the store.  Also removes any 
    folder, message and message removal records associated with the removed account.
    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::removeAccount(const QMailAccountId& id) 
{
    return removeAccounts(QMailAccountKey(QMailAccountKey::Id, id));
}

/*!
    Removes all QMailAccounts identified by the key \a key from the store. Also removes 
    any folder, message and message removal records associated with the removed account.
    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::removeAccounts(const QMailAccountKey& key)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptRemoveAccounts, this, cref(key)), "removeAccounts");
}

/*!
    Removes a QMailFolder with QMailFolderId \a id from the message store. Also removes any 
    sub-folders of the identified folder, and any messages contained in any of the 
    folders removed.  If \a option is QMailStore::CreateRemovalRecord then removal 
    records will be created for each removed message.
    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::removeFolder(const QMailFolderId& id, QMailStore::MessageRemovalOption option)
{
    // Remove the identified folder and any sub-folders
    QMailFolderKey idKey(QMailFolderKey::Id, id);
    QMailFolderKey subKey(QMailFolderKey::AncestorFolderIds, id, QMailDataComparator::Includes);

    return removeFolders(idKey | subKey, option);
}

/*!
    Removes all QMailFolders identified by the key \a key from the message store. Also
    removes any sub-folders of the removed folders, and any messages contained in any of
    the folders removed.  If \a option is QMailStore::CreateRemovalRecord then removal 
    records will be created for each removed message.
    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::removeFolders(const QMailFolderKey& key, QMailStore::MessageRemovalOption option)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptRemoveFolders, this, cref(key), option), "removeFolders");
}

/*!
    Removes a QMailMessage with QMailMessageId \a id from the message store. If \a option is 
    QMailStore::CreateRemovalRecord then a removal record will be created for the
    removed message.
    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::removeMessage(const QMailMessageId& id, QMailStore::MessageRemovalOption option)
{
    return removeMessages(QMailMessageKey(QMailMessageKey::Id, id), option);
}

/*!
    Removes all QMailMessages identified by the key \a key from the message store.
    If \a option is QMailStore::CreateRemovalRecord then removal records will be 
    created for each removed message.
    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::removeMessages(const QMailMessageKey& key, QMailStore::MessageRemovalOption option)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptRemoveMessages, this, cref(key), option), "removeMessages");
}

/*!
    Updates the existing QMailAccount \a account on the store, with the 
    configuration details optionally specified by \a config.
    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::updateAccount(QMailAccount* account, AccountConfiguration *config)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptUpdateAccount, this, account, config), "updateAccount");
}

/*!
    Updates the existing QMailFolder \a folder on the message store.
    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::updateFolder(QMailFolder* folder)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptUpdateFolder, this, folder), "updateFolder");
}

/*!
    Updates the existing QMailMessage \a msg on the message store.
    Returns \c true if the operation completed successfully, or \c false otherwise. 
*/
bool QMailStore::updateMessage(QMailMessage* msg)
{
    return updateMessage(msg, msg);
}

/*!
    Updates the meta data of the existing message on the message store, to match \a metaData.
    Returns \c true if the operation completed successfully, or \c false otherwise. 
*/
bool QMailStore::updateMessage(QMailMessageMetaData* metaData)
{
    return updateMessage(metaData, NULL);
}

/*! \internal */
bool QMailStore::updateMessage(QMailMessageMetaData* metaData, QMailMessage* mail)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptUpdateMessage, this, metaData, mail), "updateMessage");
}

/*!
    Updates the message properties defined in \a properties to match the respective element
    contained in the \a data, for all messages which match the criteria defined by \a key.

    Returns \c true if the operation completed successfully, or \c false otherwise. 
*/
bool QMailStore::updateMessagesMetaData(const QMailMessageKey& key,
                                        const QMailMessageKey::Properties& properties,
                                        const QMailMessageMetaData& data) 
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptUpdateMessagesMetaData, this, cref(key), cref(properties), cref(data)), "updateMessagesMetaData");
}

/*!
    Updates message status flags set in \a status according to \a set,
    for messages which match the criteria defined by \a key.

    Returns \c true if the operation completed successfully, or \c false otherwise. 
*/
bool QMailStore::updateMessagesMetaData(const QMailMessageKey& key, quint64 status, bool set)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptUpdateMessagesStatus, this, cref(key), status, set), "updateMessagesMetaData");
}

/*!
    Returns the count of the number of accounts which pass the 
    filtering criteria defined in QMailAccountKey \a key. If 
    key is empty a count of all accounts is returned.
*/
int QMailStore::countAccounts(const QMailAccountKey& key) const
{
    int result(0);
    repeatedly<ReadAccess>(bind(&QMailStore::attemptCountAccounts, this, cref(key), &result), "countAccounts");
    return result;
}

/*!
    Returns the count of the number of folders which pass the 
    filtering criteria defined in QMailFolderKey \a key. If 
    key is empty a count of all folders is returned.
*/
int QMailStore::countFolders(const QMailFolderKey& key) const
{
    int result(0);
    repeatedly<ReadAccess>(bind(&QMailStore::attemptCountFolders, this, cref(key), &result), "countFolders");
    return result;
}

/*!
    Returns the count of the number of messages which pass the 
    filtering criteria defined in QMailMessageKey \a key. If 
    key is empty a count of all messages is returned.
*/
int QMailStore::countMessages(const QMailMessageKey& key) const
{
    int result(0);
    repeatedly<ReadAccess>(bind(&QMailStore::attemptCountMessages, this, cref(key), &result), "countMessages");
    return result;
}

/*!
    Returns the total size of the messages which pass the 
    filtering criteria defined in QMailMessageKey \a key. If 
    key is empty the total size of all messages is returned.
*/
int QMailStore::sizeOfMessages(const QMailMessageKey& key) const
{
    int result(0);
    repeatedly<ReadAccess>(bind(&QMailStore::attemptSizeOfMessages, this, cref(key), &result), "sizeOfMessages");
    return result;
}

/*!
    Returns the \l{QMailAccountId}s of accounts in the store. If \a key is not empty 
    only accounts matching the parameters set by \a key will be returned, otherwise 
    all accounts identifiers will be returned.
    If \a sortKey is not empty, the identifiers will be sorted by the parameters set 
    by \a sortKey.
*/
const QMailAccountIdList QMailStore::queryAccounts(const QMailAccountKey& key,
                                                   const QMailAccountSortKey& sortKey) const
{
    QMailAccountIdList ids;
    repeatedly<ReadAccess>(bind(&QMailStore::attemptQueryAccounts, this, cref(key), cref(sortKey), &ids), "queryAccounts");
    return ids;
}

/*!
    Returns the \l{QMailFolderId}s of folders in the message store. If \a key is not empty 
    only folders matching the parameters set by \a key will be returned, otherwise 
    all folder identifiers will be returned.
    If \a sortKey is not empty, the identifiers will be sorted by the parameters set 
    by \a sortKey.
*/
const QMailFolderIdList QMailStore::queryFolders(const QMailFolderKey& key,
                                                 const QMailFolderSortKey& sortKey) const
{
    QMailFolderIdList ids;
    repeatedly<ReadAccess>(bind(&QMailStore::attemptQueryFolders, this, cref(key), cref(sortKey), &ids), "queryFolders");
    return ids;
}

/*!
    Returns the \l{QMailMessageId}s of messages in the message store. If \a key is not empty 
    only messages matching the parameters set by \a key will be returned, otherwise 
    all message identifiers will be returned.
    If \a sortKey is not empty, the identifiers will be sorted by the parameters set 
    by \a sortKey.
*/
const QMailMessageIdList QMailStore::queryMessages(const QMailMessageKey& key, 
                                                   const QMailMessageSortKey& sortKey) const
{
    QMailMessageIdList ids;
    repeatedly<ReadAccess>(bind(&QMailStore::attemptQueryMessages, this, cref(key), cref(sortKey), &ids), "queryMessages");
    return ids;
}

/*!
   Returns the QMailAcount defined by a QMailAccountId \a id from
   the store.
 */
QMailAccount QMailStore::account(const QMailAccountId& id) const
{
    if (d->accountCache.contains(id))
        return d->accountCache.lookup(id);

    QMailAccount account;
    account.setId(id);
    if (d->loadAccountSettings(&account)) {
        //update account cache
        if (account.id().isValid())
            d->accountCache.insert(account);
    }

    return account;
}

/*!
   Returns the QMailFolder defined by a QMailFolderId \a id from 
   the message store.
*/
QMailFolder QMailStore::folder(const QMailFolderId& id) const
{
    if (d->folderCache.contains(id))
        return d->folderCache.lookup(id);

    QMailFolder folder;
    repeatedly<ReadAccess>(bind(&QMailStore::attemptFolder, this, cref(id), &folder), "folder");
    return folder;
}

/*!
   Returns the QMailMessage defined by a QMailMessageId \a id from 
   the message store.
*/
QMailMessage QMailStore::message(const QMailMessageId& id) const
{
    // Resolve from overloaded member functions:
    AttemptResult (QMailStore::*func)(const QMailMessageId&, QMailMessage*, MailStoreReadLock&) const = &QMailStore::attemptMessage;

    QMailMessage msg;
    repeatedly<ReadAccess>(bind(func, this, cref(id), &msg), "message(id)");
    return msg;
}

/*!
   Returns the QMailMessage defined by the unique identifier \a uid from the account with id \a accountId.
*/
QMailMessage QMailStore::message(const QString& uid, const QMailAccountId& accountId) const
{
    // Resolve from overloaded member functions:
    AttemptResult (QMailStore::*func)(const QString&, const QMailAccountId&, QMailMessage*, MailStoreReadLock&) const = &QMailStore::attemptMessage;

    QMailMessage msg;
    repeatedly<ReadAccess>(bind(func, this, cref(uid), cref(accountId), &msg), "message(uid, accountId)");
    return msg;
}

/*!
   Returns the meta data for the message identified by the QMailMessageId \a id from the message store.
*/
QMailMessageMetaData QMailStore::messageMetaData(const QMailMessageId& id) const
{
    if (d->headerCache.contains(id))
        return d->headerCache.lookup(id);

    //if not in the cache, then preload the cache with the id and its most likely requested siblings
    preloadHeaderCache(id);

    return d->headerCache.lookup(id);
}

/*!
   Returns the meta data for the message identified by the unique identifier \a uid from the account with id \a accountId.
*/
QMailMessageMetaData QMailStore::messageMetaData(const QString& uid, const QMailAccountId& accountId) const
{
    QMailMessageKey uidKey(QMailMessageKey::ServerUid,uid);
    QMailMessageKey accountKey(QMailMessageKey::ParentAccountId,accountId);

    QMailMessageMetaDataList results = messagesMetaData(uidKey & accountKey, QMailStorePrivate::allMessageProperties());
    if(!results.isEmpty()) {
        if (results.count() > 1)
            qLog(Messaging) << "Warning, messageMetaData by uid returned more than 1 result";
        
        d->headerCache.insert(results.first());
        return results.first();
    }

    return QMailMessageMetaData();
}

/*!
    \enum QMailStore::ReturnOption
    This enum defines the meta data list return option for QMailStore::messagesMetaData()

    \value ReturnAll        Return all meta data objects that match the selection criteria, including duplicates.
    \value ReturnDistinct   Return distinct meta data objects that match the selection criteria, excluding duplicates.
*/

/*!
    Retrieves a list of QMailMessageMetaData objects containing meta data elements specified by 
    \a properties, for messages which match the criteria defined by \a key. If \a option is 
    \c ReturnAll then duplicate objects are included in the list; otherwise
    duplicate objects are excluded from the returned list.

    Returns a list of QMailMessageMetaData objects if successfully completed, or an empty list for 
    an error or no data.
*/
const QMailMessageMetaDataList QMailStore::messagesMetaData(const QMailMessageKey& key,
                                                      const QMailMessageKey::Properties& properties,
                                                      ReturnOption option) const
{
    QMailMessageMetaDataList metaData;
    repeatedly<ReadAccess>(bind(&QMailStore::attemptMessagesMetaData, this, cref(key), cref(properties), option, &metaData), "messagesMetaData");
    return metaData;
}

/*!
    Retrieves a list of QMailMessageRemovalRecord objects containing information about messages
    that have been removed from local storage. Records are retrived for messages whose account Id's 
    match \a accountId and whose optional mailbox matches \a fromMailbox.
    This information is primarily for synchronization of local changes to remote message storage 
    services such as IMAP servers.

    Returns a list of QMailMessageRemovalRecord objects if successfully completed, or an empty list for 
    an error or no data.
*/
const QMailMessageRemovalRecordList QMailStore::messageRemovalRecords(const QMailAccountId& accountId,
                                                                      const QString& fromMailbox) const
{
    QMailMessageRemovalRecordList removalRecords;
    repeatedly<ReadAccess>(bind(&QMailStore::attemptMessageRemovalRecords, this, cref(accountId), cref(fromMailbox), &removalRecords), "messageRemovalRecords");
    return removalRecords;
}

/*!
    Erases message deletion records from the account with id \a accountId and 
    server uid listed in \a serverUids.  If serverUids is empty, all message deletion
    records for the specified account are deleted.

    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::purgeMessageRemovalRecords(const QMailAccountId& accountId, const QStringList& serverUids)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptPurgeMessageRemovalRecords, this, cref(accountId), cref(serverUids)), "purgeMessageRemovalRecords");
}

/*!
    Updates the QMailMessage with QMailMessageId \a id to move the message back to the
    previous folder it was contained by.

    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::restoreToPreviousFolder(const QMailMessageId& id)
{
    return restoreToPreviousFolder(QMailMessageKey(QMailMessageKey::Id, id));
}

/*!
    Updates all QMailMessages identified by the key \a key to move the messages back to the
    previous folder they were contained by.

    Returns \c true if the operation completed successfully, \c false otherwise. 
*/
bool QMailStore::restoreToPreviousFolder(const QMailMessageKey& key)
{
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptRestoreToPreviousFolder, this, cref(key)), "restoreToPreviousFolder");
}

/*!
    Registers a status flag for QMailFolder objects, with the identifier \a name.
    Returns true if the flag is already registered, or if it is successfully registered; otherwise returns false.

    \sa folderStatusMask()
*/
bool QMailStore::registerFolderStatusFlag(const QString& name)
{
    if (folderStatusMask(name) != 0)
        return true;

    static const QString context("folderstatus");
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptRegisterStatusBit, this, cref(name), cref(context), 64), "registerFolderStatusBit");
}

/*!
    Returns the status bitmask needed to test the result of QMailFolder::status() 
    against the QMailFolder status flag registered with the identifier \a name.

    \sa registerFolderStatusFlag(), QMailFolder::statusMask()
*/
quint64 QMailStore::folderStatusMask(const QString& name) const
{
    static QMap<QString, quint64> statusMap;
    static const QString context("folderstatus");

    return queryStatusMap(name, context, statusMap);
}

/*!
    Registers a status flag for QMailMessage objects, with the identifier \a name.
    Returns true if the flag is already registered, or if it is successfully registered; otherwise returns false.

    \sa messageStatusMask()
*/
bool QMailStore::registerMessageStatusFlag(const QString& name)
{
    if (messageStatusMask(name) != 0)
        return true;

    static const QString context("messagestatus");
    return repeatedly<WriteAccess>(bind(&QMailStore::attemptRegisterStatusBit, this, cref(name), cref(context), 64), "registerMessageStatusBit");
}

/*!
    Returns the status bitmask needed to test the result of QMailMessage::status() 
    against the QMailMessage status flag registered with the identifier \a name.

    \sa registerMessageStatusFlag(), QMailMessage::statusMask()
*/
quint64 QMailStore::messageStatusMask(const QString& name) const
{
    static QMap<QString, quint64> statusMap;
    static const QString context("messagestatus");

    return queryStatusMap(name, context, statusMap);
}

/*!
    Forces any queued event notifications to immediately be synchronously emitted, and processed
    synchronously by recipient processes.

    Any events occurring before flushIpcNotifications() is invoked will be processed by 
    recipient processes before any IPC packets generated after the invocation.
*/
void QMailStore::flushIpcNotifications()
{
    d->flushIpcNotifications();
}

/*! \internal */
quint64 QMailStore::queryStatusMap(const QString &name, const QString &context, QMap<QString, quint64> &map) const
{
    QMap<QString, quint64>::const_iterator it = map.find(name);
    if (it != map.end())
        return it.value();

    int result(0);
    repeatedly<ReadAccess>(bind(&QMailStore::attemptStatusBit, this, cref(name), cref(context), &result), "folderStatusMask");
    if (result == 0) 
        return 0;

    quint64 maskValue = (1 << (result - 1));
    map[name] = maskValue;
    return maskValue;
}

/*! \internal */
QMailAccountIdList QMailStore::messageAccountIds(const QMailMessageKey& key, bool inTransaction, AttemptResult *result) const
{
    QMailAccountIdList accountIds;

    if (inTransaction) {
        // We can't retry this query after a busy error if we're in a transaction
        MailStoreReadLock l(d);
        *result = attemptMessageAccountIds(key, &accountIds, l);
    } else {
        bool ok = repeatedly<ReadAccess>(bind(&QMailStore::attemptMessageAccountIds, this, cref(key), &accountIds), "messageAccountIds");
        if (result)
            *result = ok ? Success : Failure;
    }

    return accountIds;
}

/*! \internal */
QMailFolderIdList QMailStore::messageFolderIds(const QMailMessageKey& key, bool inTransaction, AttemptResult *result) const
{
    QMailFolderIdList folderIds;

    if (inTransaction) {
        // We can't retry this query after a busy error if we're in a transaction
        MailStoreReadLock l(d);
        *result = attemptMessageFolderIds(key, &folderIds, l);
    } else {
        bool ok = repeatedly<ReadAccess>(bind(&QMailStore::attemptMessageFolderIds, this, cref(key), &folderIds), "messageFolderIds");
        if (result)
            *result = ok ? Success : Failure;
    }

    return folderIds;
}

/*! \internal */
QMailAccountIdList QMailStore::folderAccountIds(const QMailFolderKey& key, bool inTransaction, AttemptResult *result) const
{
    QMailAccountIdList accountIds;

    if (inTransaction) {
        // We can't retry this query after a busy error if we're in a transaction
        MailStoreReadLock l(d);
        *result = attemptFolderAccountIds(key, &accountIds, l);
    } else {
        bool ok = repeatedly<ReadAccess>(bind(&QMailStore::attemptFolderAccountIds, this, cref(key), &accountIds), "folderAccountIds");
        if (result)
            *result = ok ? Success : Failure;
    }

    return accountIds;
}

/*! \internal */
QMailFolderIdList QMailStore::folderAncestorIds(const QMailFolderIdList& ids, bool inTransaction, AttemptResult *result) const
{
    QMailFolderIdList ancestorIds;

    if (inTransaction) {
        // We can't retry this query after a busy error if we're in a transaction
        MailStoreReadLock l(d);
        *result = attemptFolderAncestorIds(ids, &ancestorIds, l);
    } else {
        bool ok = repeatedly<ReadAccess>(bind(&QMailStore::attemptFolderAncestorIds, this, cref(ids), &ancestorIds), "folderAncestorIds");
        if (result)
            *result = ok ? Success : Failure;
    }

    return ancestorIds;
}

/*! \internal */
void QMailStore::removeExpiredData(const QMailMessageIdList& messageIds, const QStringList& mailfiles, const QMailFolderIdList& folderIds, const QMailAccountIdList& accountIds)
{
    foreach (const QMailMessageId& id, messageIds) {
        d->headerCache.remove(id);
    }

    foreach (const QString& mailfile, mailfiles) {
        if (!mailfile.isEmpty())
            if (!d->removeMessageBody(mailfile))
                qLog(Messaging) << "Could not remove expired message content file" << mailfile;
    }

    foreach (const QMailFolderId& id, folderIds) {
        d->folderCache.remove(id);
    }

    foreach (const QMailAccountId& id, accountIds) {
        d->accountCache.remove(id);

        //delete the accounts from the settings 
        if (!d->removeAccountSettings(id))
            qLog(Messaging) << "Could not remove the account from settings file" << d->accountSettingsFileName();
    }
}

/*!
    Returns true if the running process is in the act of emitting an asynchronous QMailStore 
    signal caused by another process.  This can only be true when called from a slot
    invoked by a QMailStore signal.
*/
bool QMailStore::asynchronousEmission() const
{
    return d->asynchronousEmission();
}


template<typename FunctionType>
QMailStore::AttemptResult evaluate(QMailStore::WriteAccess, FunctionType func, const QString& description, QMailStorePrivate* d)
{
    MailStoreTransaction t(d);

    QMailStore::AttemptResult result = func(t);

    // Ensure that the transaction was committed
    if ((result == QMailStore::Success) && !t.committed()) {
        qLog(Messaging) << ::getpid() << "Failed to commit successful" << qPrintable(description) << "!";
    }

    return result;
}

template<typename FunctionType>
QMailStore::AttemptResult evaluate(QMailStore::ReadAccess, FunctionType func, const QString&, QMailStorePrivate* d)
{
    MailStoreReadLock l(d);

    return func(l);
}

/*! 
    \fn QMailStore::repeatedly(FunctionType, const QString &) const
    \internal 
*/
template<typename AccessType, typename FunctionType>
bool QMailStore::repeatedly(FunctionType func, const QString &description) const
{
    static const unsigned int MinRetryDelay = 64;
    static const unsigned int MaxRetryDelay = 2048;
    static const unsigned int MaxAttempts = 10;

    // This function calls the supplied function repeatedly, retrying whenever it
    // returns the DatabaseFailure result and the database's last error is SQLITE_BUSY.
    // It sleeps between repeated attempts, for increasing amounts of time.
    // The argument should be an object allowing nullary invocation returning an
    // AttemptResult value, created with tr1::bind if necessary.

    unsigned int attemptCount = 0;
    unsigned int delay = MinRetryDelay;

     while (true) {
        AttemptResult result = evaluate(AccessType(), func, description, d);

        if (result == Success) {
            if (attemptCount > 0) {
                qLog(Messaging) << ::getpid() << "Able to" << qPrintable(description) << "after" << attemptCount << "failed attempts";
            }
            return true;
        } else if (result == Failure) {
            qLog(Messaging) << ::getpid() << "Unable to" << qPrintable(description);
            break;
        } else { 
            // result == DatabaseFailure
            if (d->lastErrorNumber() == Sqlite3BusyErrorNumber) {
                if (attemptCount < MaxAttempts) {
                    qLog(Messaging) << ::getpid() << "Failed to" << qPrintable(description) << "- busy, pausing to retry";

                    // Pause before we retry
                    Qtopia::usleep(delay * 1000);
                    if (delay < MaxRetryDelay)
                        delay *= 2;

                    ++attemptCount;
                } else {
                    qLog(Messaging) << ::getpid() << "Retry count exceeded - failed to" << qPrintable(description);
                    break;
                }
            } else {
                qLog(Messaging) << ::getpid() << "Unable to" << qPrintable(description) << "- code:" << d->lastErrorNumber();
                break;
            }
        }
    }

    return false;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptAddAccount(QMailAccount *account, AccountConfiguration* config, MailStoreTransaction& t)
{
    if (account->id().isValid() && d->idExists(account->id())) {
        qLog(Messaging) << "Account already exists in database, use update instead";
        return Failure;
    }

    QMailAccountId insertId;

    {
        QString properties("type,name");
        QString values("?,?");
        QVariantList propertyValues;
        propertyValues << static_cast<int>(account->accountType()) << account->accountName();

        if (config) {
            // EmailAddress is currently in the account table, although it is part of the configuration...
            properties.append(",emailaddress");
            values.append(",?");
            propertyValues << config->emailAddress();
        }

        QSqlQuery query(d->simpleQuery(QString("INSERT INTO mailaccounts (%1) VALUES (%2)").arg(properties).arg(values),
                                       propertyValues,
                                       "addAccount mailaccounts query"));
        if (query.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;

        //Extract the insert id
        insertId = QMailAccountId(d->extractValue<quint64>(query.lastInsertId()));
    }

    account->setId(insertId);
    if (config)
        config->setId(insertId);

    //add the account information into the settings file
    if (!d->saveAccountSettings(*account)) {
        qLog(Messaging) << "Could not save account settings into settings file" << d->accountSettingsFileName();

        account->setId(QMailAccountId()); //revert the id
        return Failure;
    }

    // Write the additional config if provided
    if (config && !d->saveAccountSettings(*config)) {
        qLog(Messaging) << "Could not save account configuration settings into settings file" << d->accountSettingsFileName();

        account->setId(QMailAccountId()); //revert the id
        return Failure;
    }

    if (!t.commit()) {
        qLog(Messaging) << "Could not commit account changes to database";
        if (!d->removeAccountSettings(account->id())) {
            qLog(Messaging) << "Could not remove account settings from file" << d->accountSettingsFileName();
        }

        account->setId(QMailAccountId()); //revert the id
        return DatabaseFailure;
    }

    //synchronize
    QMailAccountIdList ids;
    ids.append(insertId);

    d->notifyAccountsChange(QMailStorePrivate::Added,ids);
    emit accountsAdded(ids);

    return Success;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptAddFolder(QMailFolder *folder, MailStoreTransaction& t)
{   
    //check that the parent folder actually exists
    if (!d->checkPreconditions(*folder))
        return Failure;

    QMailFolderId insertId;

    {
        //insert the qmailfolder
        QSqlQuery query(d->simpleQuery("INSERT INTO mailfolders (name,parentid,parentaccountid,displayname,status) VALUES (?,?,?,?,?)",
                                       QVariantList() << folder->name() 
                                                      << folder->parentId().toULongLong()
                                                      << folder->parentAccountId().toULongLong()
                                                      << folder->displayName()
                                                      << folder->status(),
                                       "addFolder mailfolders query"));
        if (query.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;

        //set the insert id
        insertId = QMailFolderId(d->extractValue<quint64>(query.lastInsertId()));
    }

    folder->setId(insertId);

    //create links to ancestor folders
    if (folder->parentId().isValid()) {
        {
            //add records for each ancestor folder
            QSqlQuery ancestorQuery(d->simpleQuery("INSERT INTO mailfolderlinks "
                                                   "SELECT DISTINCT id, ? "
                                                   "FROM mailfolderlinks WHERE descendantid = ?",
                                                   QVariantList() << folder->id().toULongLong() 
                                                                  << folder->parentId().toULongLong(),
                                                   "mailfolderlinks insert ancestors"));
            if (ancestorQuery.lastError().type() != QSqlError::NoError)
                return DatabaseFailure;
        }

        {
            // Our direct parent is also an ancestor
            QSqlQuery parentQuery(d->simpleQuery("INSERT INTO mailfolderlinks VALUES (?, ?)",
                                                 QVariantList() << folder->parentId().toULongLong() 
                                                                << folder->id().toULongLong(),
                                                 "mailfolderlinks insert parent"));
            if (parentQuery.lastError().type() != QSqlError::NoError)
                return DatabaseFailure;
        }
    }

    if (t.commit()) {
        //synchronize

        QMailFolderIdList ids;
        QMailAccountIdList accountIds;
        ids.append(insertId);
        if (folder->parentAccountId().isValid())
            accountIds.append(folder->parentAccountId());

        d->notifyFoldersChange(QMailStorePrivate::Added,ids);
        if (!accountIds.isEmpty())
            d->notifyAccountsChange(QMailStorePrivate::ContentsModified,accountIds);

        emit foldersAdded(ids);
        if (!accountIds.isEmpty())
            emit accountContentsModified(accountIds);
        
        return Success;
    }
   
    folder->setId(QMailFolderId()); //revert the id
    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptAddMessage(QMailMessageMetaData *metaData, const QString &mailfile, MailStoreTransaction& t)
{
    if (!metaData->parentFolderId().isValid()) {
        qLog(Messaging) << "Unable to add folder. Invalid parent folder id";
        return Failure;
    }

    if (metaData->id().isValid() && d->idExists(metaData->id())) {
        qLog(Messaging) << "Message ID" << metaData->id() << "already exists in database, use update instead";
        return Failure;
    }

    // Ensure that any phone numbers are added in minimal form
    QMailAddress from(metaData->from());
    QString fromText(from.isPhoneNumber() ? from.minimalPhoneNumber() : from.toString());

    QStringList recipients;
    foreach (const QMailAddress& address, metaData->to())
        recipients.append(address.isPhoneNumber() ? address.minimalPhoneNumber() : address.toString());

    QMailMessageId insertId;

    {
        // Add the record to the mailmessages table
        QSqlQuery query(d->simpleQuery("INSERT INTO mailmessages (type,"
                                                                 "parentfolderid,"
                                                                 "sender,"
                                                                 "recipients,"
                                                                 "subject,"
                                                                 "stamp,"
                                                                 "status,"
                                                                 "parentaccountid,"
                                                                 "frommailbox,"
                                                                 "mailfile,"
                                                                 "serveruid,"
                                                                 "size,"
                                                                 "contenttype"
                                                                 ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                                       QVariantList() << static_cast<int>(metaData->messageType())
                                                      << metaData->parentFolderId().toULongLong()
                                                      << fromText
                                                      << recipients.join(",")
                                                      << metaData->subject()
                                                      << QMailTimeStamp(metaData->date()).toLocalTime()
                                                      << static_cast<int>(metaData->status())
                                                      << metaData->parentAccountId().toULongLong()
                                                      << metaData->fromMailbox()
                                                      << mailfile
                                                      << metaData->serverUid()
                                                      << metaData->size()
                                                      << static_cast<int>(metaData->content()),
                                       "addMessage mailmessages query"));
        if (query.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;

        //retrieve the insert id
        insertId = QMailMessageId(d->extractValue<quint64>(query.lastInsertId()));
    }

    QMailMessageIdList ids;
    QMailFolderIdList folderIds;
    QMailAccountIdList accountIds;

    ids.append(insertId);
    folderIds.append(metaData->parentFolderId());
    if (metaData->parentAccountId().isValid())
        accountIds.append(metaData->parentAccountId());

    AttemptResult result;

    //find the set of modified folders, including ancestor folders
    folderIds += folderAncestorIds(folderIds, true, &result);
    if (result != Success)
        return result;

    if (t.commit()) {
        metaData->setId(insertId);

        //synchronize
        d->notifyMessagesChange(QMailStorePrivate::Added,ids);
        d->notifyFoldersChange(QMailStorePrivate::ContentsModified,folderIds);
        if (!accountIds.isEmpty())
            d->notifyAccountsChange(QMailStorePrivate::ContentsModified,accountIds);

        emit messagesAdded(ids);
        emit folderContentsModified(folderIds);
        if (!accountIds.isEmpty())
            emit accountContentsModified(accountIds);

        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptRemoveAccounts(const QMailAccountKey &key, MailStoreTransaction& t)
{
    QMailAccountIdList deletedAccounts;
    QMailFolderIdList deletedFolders;
    QMailMessageIdList deletedMessages;
    QStringList expiredMailfiles;

    if (d->deleteAccounts(key, deletedAccounts, deletedFolders, deletedMessages, expiredMailfiles)) {
        if (t.commit()) {
            //remove deleted objects from caches
            removeExpiredData(deletedMessages, expiredMailfiles, deletedFolders, deletedAccounts);

            //synchronize
            d->notifyMessageRemovalRecordsChange(QMailStorePrivate::Removed, deletedAccounts);
            d->notifyMessagesChange(QMailStorePrivate::Removed, deletedMessages);
            d->notifyFoldersChange(QMailStorePrivate::Removed, deletedFolders);
            d->notifyAccountsChange(QMailStorePrivate::Removed, deletedAccounts);
            emit messageRemovalRecordsRemoved(deletedAccounts);
            emit messagesRemoved(deletedMessages);
            emit foldersRemoved(deletedFolders);
            emit accountsRemoved(deletedAccounts);

            return Success;
        }
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptRemoveFolders(const QMailFolderKey &key, QMailStore::MessageRemovalOption option, MailStoreTransaction& t)
{
    QMailFolderIdList deletedFolders;
    QMailMessageIdList deletedMessages;
    QStringList expiredMailfiles;
    QMailAccountIdList modifiedAccounts;

    if (d->deleteFolders(key, option, deletedFolders, deletedMessages, expiredMailfiles, modifiedAccounts)) {
        if (t.commit()) {
            //remove deleted objects from caches
            removeExpiredData(deletedMessages, expiredMailfiles, deletedFolders);

            //synchronize

            d->notifyMessagesChange(QMailStorePrivate::Removed, deletedMessages);
            d->notifyFoldersChange(QMailStorePrivate::Removed, deletedFolders);
            if (!modifiedAccounts.isEmpty()) {
                d->notifyMessageRemovalRecordsChange(QMailStorePrivate::Added, modifiedAccounts);
                d->notifyAccountsChange(QMailStorePrivate::ContentsModified, modifiedAccounts);
            }

            emit messagesRemoved(deletedMessages);
            emit foldersRemoved(deletedFolders);
            if (!modifiedAccounts.isEmpty()) {
                emit messageRemovalRecordsAdded(modifiedAccounts);
                emit accountContentsModified(modifiedAccounts);
            }

            return Success;
        }
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptRemoveMessages(const QMailMessageKey &key, QMailStore::MessageRemovalOption option, MailStoreTransaction& t)
{
    QMailMessageIdList deletedMessages;
    QStringList expiredMailfiles;
    QMailAccountIdList modifiedAccounts;
    QMailFolderIdList modifiedFolders;

    if (d->deleteMessages(key, option, deletedMessages, expiredMailfiles, modifiedAccounts, modifiedFolders)) {
        if (t.commit()) {
            //remove deleted objects from caches
            removeExpiredData(deletedMessages, expiredMailfiles);

            //synchronize
            d->notifyMessagesChange(QMailStorePrivate::Removed, deletedMessages);
            d->notifyFoldersChange(QMailStorePrivate::ContentsModified, modifiedFolders);
            if (!modifiedAccounts.isEmpty()) {
                d->notifyMessageRemovalRecordsChange(QMailStorePrivate::Added, modifiedAccounts);
                d->notifyAccountsChange(QMailStorePrivate::ContentsModified, modifiedAccounts);
            }

            emit messagesRemoved(deletedMessages);
            emit folderContentsModified(modifiedFolders);
            if (!modifiedAccounts.isEmpty()) {
                emit messageRemovalRecordsAdded(modifiedAccounts);
                emit accountContentsModified(modifiedAccounts);
            }

            return Success;
        }
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptUpdateAccount(QMailAccount *account, AccountConfiguration *config, MailStoreTransaction& t)
{
    if (!account->id().isValid())
        return Failure;

    QString properties("type=?, name=?");
    QVariantList propertyValues;
    propertyValues << static_cast<int>(account->accountType()) << account->accountName();

    if (config) {
        // EmailAddress is currently in the account table, although it is part of the configuration...
        properties.append(", emailaddress=?");
        propertyValues << config->emailAddress();
    }

    QSqlQuery query(d->simpleQuery(QString("UPDATE mailaccounts SET %1 WHERE id=?").arg(properties),
                                   propertyValues << account->id().toULongLong(),
                                   "updateAccount mailaccounts query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (!d->updateAccountSettings(*account, true)) {
        qLog(Messaging) << "Could not update account settings" << d->accountSettingsFileName();
        return Failure;
    }

    if (config && !d->updateAccountSettings(*config, false)) {
        qLog(Messaging) << "Could not update account configuration settings" << d->accountSettingsFileName();
        return Failure;
    }

    if (t.commit()) {
        //update the account cache
        if (d->accountCache.contains(account->id()))
            d->accountCache.insert(*account);

        //synchronize
        
        QMailAccountIdList ids;
        ids.append(account->id());
        d->notifyAccountsChange(QMailStorePrivate::Updated,ids);
        emit accountsUpdated(ids);

        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptUpdateFolder(QMailFolder *folder, MailStoreTransaction& t)
{
    //check that the parent folder actually exists
    if(!d->checkPreconditions(*folder, true))
        return Failure;

    QMailFolderId parentId;
    QMailAccountId parentAccountId;

    {
        //find the current parent folder
        QSqlQuery parentQuery(d->simpleQuery("SELECT parentid, parentaccountid FROM mailfolders WHERE id=?",
                                             QVariantList() << folder->id().toULongLong(),
                                             "mailfolder parent query"));
        if (parentQuery.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;

        if (parentQuery.first()) {
            parentId = QMailFolderId(d->extractValue<quint64>(parentQuery.value(0)));
            parentAccountId = QMailAccountId(d->extractValue<quint64>(parentQuery.value(1)));
        }
    }

    {
        QSqlQuery query(d->simpleQuery("UPDATE mailfolders SET name=?,parentid=?,parentaccountid=?,displayname=?,status=? WHERE id=?",
                                       QVariantList() << folder->name()
                                                      << folder->parentId().toULongLong()
                                                      << folder->parentAccountId().toULongLong()
                                                      << folder->displayName()
                                                      << folder->status()
                                                      << folder->id().toULongLong(),
                                       "updateFolder mailfolders query"));
        if (query.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;
    }
    
    QMailAccountIdList affectedAccounts;

    if (parentId != folder->parentId()) {
        // QMailAccount contains a copy of the folder data; we need to tell it to reload
        if (parentId.isValid())
            affectedAccounts.append(parentAccountId);
        if (folder->parentId().isValid() && !affectedAccounts.contains(folder->parentAccountId()))
            affectedAccounts.append(folder->parentAccountId());

        {
            //remove existing links to this folder
            QSqlQuery deleteQuery(d->simpleQuery("DELETE FROM mailfolderlinks WHERE descendantid = ?",
                                                 QVariantList() << folder->id().toULongLong(),
                                                 "mailfolderlinks delete in update"));
            if (deleteQuery.lastError().type() != QSqlError::NoError)
                return DatabaseFailure;
        }

        {
            //add links to the new parent
            QSqlQuery ancestorQuery(d->simpleQuery("INSERT INTO mailfolderlinks "
                                                   "SELECT DISTINCT id, ? "
                                                   "FROM mailfolderlinks WHERE descendantid = ?",
                                                   QVariantList() << folder->id().toULongLong() 
                                                                  << folder->parentId().toULongLong(),
                                                   "mailfolderlinks insert ancestors"));
            if (ancestorQuery.lastError().type() != QSqlError::NoError)
                return DatabaseFailure;
        }

        {
            QSqlQuery parentQuery(d->simpleQuery("INSERT INTO mailfolderlinks VALUES (?, ?)",
                                                 QVariantList() << folder->parentId().toULongLong()
                                                                << folder->id().toULongLong(),
                                                 "mailfolderlinks insert parent"));
            if (parentQuery.lastError().type() != QSqlError::NoError)
                return DatabaseFailure;
        }
    }
        
    if (t.commit()) {
        //update the folder cache
        if (d->folderCache.contains(folder->id()))
            d->folderCache.insert(*folder);

        //synchronize
        QMailFolderIdList ids;
        ids.append(folder->id());

        d->notifyFoldersChange(QMailStorePrivate::Updated,ids);
        if (!affectedAccounts.isEmpty())
            d->notifyAccountsChange(QMailStorePrivate::ContentsModified,affectedAccounts);
        
        emit foldersUpdated(ids);
        if (!affectedAccounts.isEmpty())
            emit accountContentsModified(affectedAccounts);

        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptUpdateMessage(QMailMessageMetaData *metaData, QMailMessage *mail, MailStoreTransaction& t)
{
    if (!metaData->id().isValid())
        return Failure;

    if (!metaData->dataModified() && (!mail || !mail->contentModified()))
        return Failure;

    QMailAccountId parentAccountId;
    QMailFolderId parentFolderId;
    QString mailfile;
    QMailFolderIdList affectedFolderIds;

    // Find the existing properties 
    {
        QSqlQuery query(d->simpleQuery("SELECT parentaccountId,parentfolderId,mailfile FROM mailmessages WHERE id = ?",
                                       QVariantList() << metaData->id().toULongLong(),
                                       "updateMessage existing properties query"));
        if (query.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;

        if (query.first()) {
            parentAccountId = QMailAccountId(d->extractValue<quint64>(query.value(0)));
            parentFolderId = QMailFolderId(d->extractValue<quint64>(query.value(1)));
            mailfile = d->extractValue<QString>(query.value(2));

            // Find any folders affected by this update
            affectedFolderIds.append(metaData->parentFolderId());
            if (parentFolderId != metaData->parentFolderId()) {
                // The previous location folder has also changed
                affectedFolderIds.append(parentFolderId);
                metaData->setPreviousParentFolderId(parentFolderId);
            }

            // Ancestor folders are also considered to be affected
            AttemptResult result;
            affectedFolderIds += folderAncestorIds(affectedFolderIds, true, &result);
            if (result != Success)
                return result;
        } else {
            qLog(Messaging) << "Could not query parent account, folder and mailfile path";
            return Failure;
        }
    }

    bool updateMailfile(false);
    bool updateContent(mail && mail->contentModified());

    if (updateContent) {
        if (mailfile.isEmpty()) {
            // We need to create a new mailfile
            updateMailfile = true;
            mailfile = d->generateUniqueFileName();
        }
    }

    QMailMessageKey::Properties updateProperties(QMailStorePrivate::updatableMessageProperties());

    // Don't update the previous parent folder if it isn't set
    if (!metaData->previousParentFolderId().isValid())
        updateProperties &= ~QMailMessageKey::PreviousParentFolderId;

    QString properties(d->expandProperties(updateProperties, true));
    QVariantList messageValues(d->updateValues(updateProperties, *metaData));

    if (updateMailfile) {
        properties += ",mailfile=?";
        messageValues.append(mailfile);
    }

    {
        QMailMessageKey idKey(QMailMessageKey::Id, metaData->id());
        QSqlQuery query(d->simpleQuery(QString("UPDATE mailmessages SET %1 WHERE").arg(properties),
                                       messageValues,                              
                                       idKey,
                                       "updateMessage mailmessages update"));
        if (query.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;
    }

    if (updateContent) {
        //update the mail body
        if(!d->updateMessageBody(mailfile, mail)) {
            qLog(Messaging) << "Could not update mail body " << mailfile;
            return Failure;
        }
    }

    if (t.commit()) {
        // The message is now up-to-date with data store
        metaData->committed();

        //update the header cache
        if(d->headerCache.contains(metaData->id())) {
            if (updateMailfile)
                messageValues.takeLast();

            QMailMessageMetaData cachedMetaData = d->headerCache.lookup(metaData->id());
            d->updateMessageValues(updateProperties, messageValues, cachedMetaData);
            cachedMetaData.committed();
            d->headerCache.insert(cachedMetaData);
        }

        //synchronize
        QMailMessageIdList ids;
        QMailAccountIdList accountIds;
        ids.append(metaData->id());

        if (metaData->parentAccountId().isValid())
            accountIds.append(metaData->parentAccountId());
        if (parentAccountId != metaData->parentAccountId()) {
            if (parentAccountId.isValid())
                accountIds.append(parentAccountId);
        }

        d->notifyMessagesChange(QMailStorePrivate::Updated,ids);
        d->notifyFoldersChange(QMailStorePrivate::ContentsModified,affectedFolderIds);
        if (!accountIds.isEmpty())
            d->notifyAccountsChange(QMailStorePrivate::ContentsModified,accountIds);

        emit messagesUpdated(ids);
        emit folderContentsModified(affectedFolderIds);
        if (!accountIds.isEmpty())
            emit accountContentsModified(accountIds);

        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::affectedByMessageIds(const QMailMessageIdList &messages, QMailFolderIdList *folderIds, QMailAccountIdList *accountIds) const
{
    AttemptResult result;

    // Find the set of folders whose contents are modified by this update
    QMailFolderIdList messageFolderIds;

    {
        MailStoreReadLock l(d);
        result = attemptMessageFolderIds(QMailMessageKey(messages), &messageFolderIds, l);
    }

    if (result != Success)
        return result;

    return affectedByFolderIds(messageFolderIds, folderIds, accountIds);
}

/*! \internal */
QMailStore::AttemptResult QMailStore::affectedByFolderIds(const QMailFolderIdList &folders, QMailFolderIdList *folderIds, QMailAccountIdList *accountIds) const
{
    AttemptResult result;

    // Any ancestor folders are also modified
    QMailFolderIdList ancestorIds;

    {
        MailStoreReadLock l(d);
        result = attemptFolderAncestorIds(folders, &ancestorIds, l);
    }

    if (result != Success)
        return result;

    *folderIds = folders + ancestorIds;

    // Find the set of accounts whose contents are modified by this update
    MailStoreReadLock l(d);
    result = attemptFolderAccountIds(QMailFolderKey(*folderIds), accountIds, l);
    return result;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptUpdateMessagesMetaData(const QMailMessageKey &key, 
                                                                    const QMailMessageKey::Properties &properties, 
                                                                    const QMailMessageMetaData &data, 
                                                                    MailStoreTransaction& t) 
{
    //do some checks first
    if (properties & QMailMessageKey::Id) {
        qLog(Messaging) << "Updating of messages IDs is not supported";
        return Failure;
    }
    
    d->checkComparitors(key);

    if (properties & QMailMessageKey::ParentFolderId) {
        if (!d->idExists(data.parentFolderId())) {
            qLog(Messaging) << "Update of messages failed. Parent folder does not exist";
            return Failure;
        }
    }

    QMailFolderIdList modifiedFolderIds;
    QMailAccountIdList modifiedAccountIds;
    QVariantList messageValues;

    //get the valid ids
    QMailMessageIdList modifiedMessageIds = queryMessages(key);
    if (!modifiedMessageIds.isEmpty()) {
        // Find the set of folders and accounts whose contents are modified by this update
        QMailMessageKey modifiedMessageKey(modifiedMessageIds);
        AttemptResult result = affectedByMessageIds(modifiedMessageIds, &modifiedFolderIds, &modifiedAccountIds);
        if (result != Success)
            return result;

        // If we're setting parentFolderId, that folder is modified also
        if (properties & QMailMessageKey::ParentFolderId) {
            if (!modifiedFolderIds.contains(data.parentFolderId()))
                modifiedFolderIds.append(data.parentFolderId());

            // All these messages need to have previousparentfolderid updated, where it will change
            QSqlQuery query(d->simpleQuery(QString("UPDATE mailmessages SET previousparentfolder=parentfolderid "
                                                   "WHERE parentfolderid <> %1 AND"),
                                           QVariantList() << data.parentFolderId().toULongLong(),
                                           modifiedMessageKey,
                                           "updateMessagesMetaData mailmessages previousparentfolderid update query"));
            if (query.lastError().type() != QSqlError::NoError)
                return DatabaseFailure;
        }

        messageValues = d->updateValues(properties, data);

        {
            QSqlQuery query(d->simpleQuery(QString("UPDATE mailmessages SET %1 WHERE").arg(d->expandProperties(properties, true)),
                                           messageValues,
                                           modifiedMessageKey,
                                           "updateMessagesMetaData mailmessages query"));
            if (query.lastError().type() != QSqlError::NoError)
                return DatabaseFailure;
        }
    }

    if (t.commit()) {
        if (!modifiedMessageIds.isEmpty()) {
            //update the header cache
            foreach(const QMailMessageId& id,modifiedMessageIds) {
                if (d->headerCache.contains(id)) {
                    QMailMessageMetaData cachedMetaData = d->headerCache.lookup(id);
                    d->updateMessageValues(properties, messageValues, cachedMetaData);
                    cachedMetaData.committed();
                    d->headerCache.insert(cachedMetaData);
                }
            }

            //synchronize
            d->notifyMessagesChange(QMailStorePrivate::Updated,modifiedMessageIds);
            d->notifyFoldersChange(QMailStorePrivate::ContentsModified,modifiedFolderIds);
            if (!modifiedAccountIds.isEmpty())
                d->notifyAccountsChange(QMailStorePrivate::ContentsModified,modifiedAccountIds);

            emit messagesUpdated(modifiedMessageIds);
            emit folderContentsModified(modifiedFolderIds);
            if (!modifiedAccountIds.isEmpty())
                emit accountContentsModified(modifiedAccountIds);
        }

        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptUpdateMessagesStatus(const QMailMessageKey &key, quint64 status, bool set, MailStoreTransaction& t)
{
    d->checkComparitors(key);

    QMailFolderIdList modifiedFolderIds;
    QMailAccountIdList modifiedAccountIds;

    //get the valid ids
    QMailMessageIdList modifiedMessageIds = queryMessages(key);
    if (!modifiedMessageIds.isEmpty()) {
        QMailMessageKey modifiedMessageKey(modifiedMessageIds);

        // Find the set of folders and accounts whose contents are modified by this update
        AttemptResult result = affectedByMessageIds(modifiedMessageIds, &modifiedFolderIds, &modifiedAccountIds);
        if (result != Success)
            return result;

        {
            QString sql = "UPDATE mailmessages SET status = status %1 ? WHERE";
            QSqlQuery query(d->simpleQuery(sql.arg(set ? "|" : "&"),
					   QVariantList() <<
					   (set
					    ? status
					    : (QMailMessageMetaData::All & ~status)),
					   modifiedMessageKey,
					   "updateMessagesMetaData status query"));
            if (query.lastError().type() != QSqlError::NoError)
                return DatabaseFailure;
        }
    }

    if (t.commit()) {
        if (!modifiedMessageIds.isEmpty()) {
            //update the header cache
            foreach (const QMailMessageId& id, modifiedMessageIds) {
                if (d->headerCache.contains(id)) {
                    QMailMessageMetaData cachedMetaData = d->headerCache.lookup(id);
                    quint64 newStatus = cachedMetaData.status();
                    newStatus = set ? (newStatus | status) : (newStatus & ~status);
                    cachedMetaData.setStatus(newStatus);
                    cachedMetaData.committed();
                    d->headerCache.insert(cachedMetaData);
                }
            }

            //synchronize
            d->notifyMessagesChange(QMailStorePrivate::Updated,modifiedMessageIds);
            d->notifyFoldersChange(QMailStorePrivate::ContentsModified,modifiedFolderIds);
            if (!modifiedAccountIds.isEmpty())
                d->notifyAccountsChange(QMailStorePrivate::ContentsModified,modifiedAccountIds);

            emit messagesUpdated(modifiedMessageIds);
            emit folderContentsModified(modifiedFolderIds);
            if (!modifiedAccountIds.isEmpty())
                emit accountContentsModified(modifiedAccountIds);
        }

        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptCountAccounts(const QMailAccountKey &key, int *result, MailStoreReadLock&) const
{
    d->checkComparitors(key);

    QString sql = "SELECT COUNT(*) FROM mailaccounts";
    if (!key.isEmpty())
        sql += " WHERE " + d->buildWhereClause(key);    

    QSqlQuery query = d->prepare(sql);
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (!key.isEmpty())
        d->bindWhereData(key,query);

    if (d->execute(query) && query.next()) {
        *result = d->extractValue<int>(query.value(0));
        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptCountFolders(const QMailFolderKey &key, int *result, MailStoreReadLock&) const
{
    d->checkComparitors(key);

    QString sql = "SELECT COUNT(*) FROM mailfolders ";
    if (!key.isEmpty())
        sql += "WHERE " + d->buildWhereClause(key); 

    QSqlQuery query = d->prepare(sql);
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (!key.isEmpty())
        d->bindWhereData(key,query);

    if (d->execute(query) && query.next()) {
        *result = d->extractValue<int>(query.value(0));
        return Success;
    }
    
    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptCountMessages(const QMailMessageKey &key, int *result, MailStoreReadLock&) const
{
    d->checkComparitors(key);

    QString sql = "SELECT COUNT(*) FROM mailmessages ";
    if (!key.isEmpty())
        sql += "WHERE " + d->buildWhereClause(key);    

    QSqlQuery query = d->prepare(sql);
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (!key.isEmpty())
        d->bindWhereData(key,query);

    if (d->execute(query) && query.next()) {
        *result = d->extractValue<int>(query.value(0));
        return Success;
    }
    
    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptSizeOfMessages(const QMailMessageKey &key, int *result, MailStoreReadLock&) const
{
    QString sql = "SELECT SUM(size) FROM mailmessages ";
    if (!key.isEmpty())
        sql += "WHERE " + d->buildWhereClause(key);    

    QSqlQuery query = d->prepare(sql);
    if(query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (!key.isEmpty())
        d->bindWhereData(key,query);

    if (d->execute(query) && query.next()) {
        *result = d->extractValue<int>(query.value(0));
        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptQueryAccounts(const QMailAccountKey &key, 
                                                           const QMailAccountSortKey &sortKey, 
                                                           QMailAccountIdList *ids, 
                                                           MailStoreReadLock&) const
{
    d->checkComparitors(key);

    QString sql = "SELECT id FROM mailaccounts";
    if (!key.isEmpty())
        sql += " WHERE " + d->buildWhereClause(key); 
    if (!sortKey.isEmpty())
        sql += " " + d->buildOrderClause(sortKey);

    QSqlQuery query = d->prepare(sql);
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (!key.isEmpty())
        d->bindWhereData(key,query);

    if (d->execute(query)) {
        while (query.next())
            ids->append(QMailAccountId(d->extractValue<quint64>(query.value(0))));

        return Success;
    }

    return DatabaseFailure; 
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptQueryFolders(const QMailFolderKey &key,
                                                          const QMailFolderSortKey &sortKey,
                                                          QMailFolderIdList *ids, 
                                                          MailStoreReadLock&) const
{
    d->checkComparitors(key);

    QString sql = "SELECT id FROM mailfolders";
    if (!key.isEmpty())
        sql += " WHERE " + d->buildWhereClause(key); 
    if (!sortKey.isEmpty())
        sql += " " + d->buildOrderClause(sortKey);

    QSqlQuery query = d->prepare(sql);
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (!key.isEmpty())
        d->bindWhereData(key,query);

    if (d->execute(query)) {
        while (query.next())
            ids->append(QMailFolderId(d->extractValue<quint64>(query.value(0))));

        return Success; 
    }

    return DatabaseFailure; 
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptQueryMessages(const QMailMessageKey &key, 
                                                           const QMailMessageSortKey &sortKey,
                                                           QMailMessageIdList *ids, 
                                                           MailStoreReadLock&) const
{
    d->checkComparitors(key);

    QString sql = "SELECT id FROM mailmessages";
    if (!key.isEmpty())
        sql += " WHERE " + d->buildWhereClause(key);
    if (!sortKey.isEmpty())
        sql += " " + d->buildOrderClause(sortKey);

    QSqlQuery query = d->prepare(sql);
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (!key.isEmpty())
        d->bindWhereData(key,query);

    if (d->execute(query)) {
        while (query.next())
            ids->append(QMailMessageId(d->extractValue<quint64>(query.value(0))));

        //store the results of this call for cache preloading
        d->lastQueryMessageResult = *ids;

        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptFolder(const QMailFolderId &id, QMailFolder *result, MailStoreReadLock&) const
{
    QSqlQuery query(d->simpleQuery("SELECT * FROM mailfolders WHERE id=?",
                                   QVariantList() << id.toULongLong(),
                                   "folder mailfolders query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (query.first()) {
        //update cache 
        *result = d->buildQMailFolder(query.record());
        d->folderCache.insert(*result);
        return Success;
    }

    return Failure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptMessage(const QMailMessageId &id, QMailMessage *result, MailStoreReadLock&) const
{
    //mailfile is not an exposed property, so add separately
    QString sql("SELECT %1,mailfile FROM mailmessages WHERE id = ?");

    QSqlQuery query(d->simpleQuery(sql.arg(d->expandProperties(QMailStorePrivate::allMessageProperties())),
                                   QVariantList() << id.toULongLong(),
                                   "message mailmessages id query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (query.first()) {
        *result = d->buildQMailMessage(query.record());
        return Success;
    }
        
    return Failure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptMessage(const QString &uid, const QMailAccountId &accountId, QMailMessage *result, MailStoreReadLock&) const
{
    //mailfile is not an exposed property, so add separately
    QString sql("SELECT %1,mailfile FROM mailmessages WHERE serveruid = ? AND parentaccountid = ?");

    QSqlQuery query(d->simpleQuery(sql.arg(d->expandProperties(QMailStorePrivate::allMessageProperties())),
                                   QVariantList() << uid << accountId.toULongLong(),
                                   "message mailmessages uid/parentaccountid query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    if (query.first()) {
        *result = d->buildQMailMessage(query.record());
        return Success;
    }
        
    return Failure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptMessagesMetaData(const QMailMessageKey& key,
                                                              const QMailMessageKey::Properties &properties,
                                                              ReturnOption option, 
                                                              QMailMessageMetaDataList *result, 
                                                              MailStoreReadLock&) const
{
    d->checkComparitors(key);

    QString sql("SELECT %1 %2 FROM mailmessages WHERE");
    sql = sql.arg(option == ReturnDistinct ? "DISTINCT " : "");

    QSqlQuery query(d->simpleQuery(sql.arg(d->expandProperties(properties, false)),
                                   key,
                                   "messagesMetaData mailmessages query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    while (query.next())
        result->append(d->messageMetaData(query.record(), properties, properties));

    return Success;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptMessageRemovalRecords(const QMailAccountId &accountId, 
                                                                   const QString &fromMailbox, 
                                                                   QMailMessageRemovalRecordList *result, 
                                                                   MailStoreReadLock&) const
{
    QString sql("SELECT * FROM deletedmessages WHERE parentaccountid = ?");
    if (!fromMailbox.isEmpty())
        sql += " AND frommailbox = ?";

    QSqlQuery query = d->prepare(sql);
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    query.addBindValue(accountId.toULongLong());
    if (!fromMailbox.isEmpty())
        query.addBindValue(fromMailbox);

    if (d->execute(query)) {
        while (query.next())
            result->append(d->buildQMailMessageRemovalRecord(query.record()));

        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptPurgeMessageRemovalRecords(const QMailAccountId &accountId, const QStringList &serverUids, MailStoreTransaction& t)
{
    QMailMessageIdList removalIds;

    {
        QString sql("SELECT id FROM deletedmessages WHERE parentaccountid=?");

        QVariantList bindValues;
        bindValues << accountId.toULongLong();

        if (!serverUids.isEmpty()) {
            QVariantList uidValues;
            foreach (const QString& uid, serverUids)
                uidValues.append(uid);

            sql = "SELECT id FROM deletedmessages WHERE parentaccountid=? AND serveruid IN %1";
            sql = sql.arg(d->expandValueList(uidValues));

            bindValues << uidValues;
        }

        QSqlQuery query(d->simpleQuery(sql, 
                                       bindValues,
                                       "purgeMessageRemovalRecord info query"));
        if (query.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;

        while (query.next())
            removalIds.append(QMailMessageId(d->extractValue<quint64>(query.value(0))));
    }

    // anything to remove?
    if (!removalIds.isEmpty()) {
        QSqlQuery query(d->simpleQuery("DELETE FROM deletedmessages WHERE",
                                       QMailMessageKey(removalIds),
                                       "purgeMessageRemovalRecord delete query"));
        if (query.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;
    }

    if (t.commit()) {
        if (!removalIds.isEmpty()) {
            QMailAccountIdList accountIds;
            accountIds.append(accountId);

            d->notifyMessageRemovalRecordsChange(QMailStorePrivate::Removed, accountIds);
            emit messageRemovalRecordsRemoved(accountIds);
        }

        return Success;
    }

    return DatabaseFailure;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptMessageAccountIds(const QMailMessageKey &key, QMailAccountIdList *result, MailStoreReadLock&) const
{
    QSqlQuery query(d->simpleQuery("SELECT DISTINCT parentaccountid FROM mailmessages WHERE",
                                   key,
                                   "messageFolderIds folder select query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    while (query.next())
        result->append(QMailAccountId(d->extractValue<quint64>(query.value(0))));

    return Success;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptMessageFolderIds(const QMailMessageKey &key, QMailFolderIdList *result, MailStoreReadLock&) const
{
    QSqlQuery query(d->simpleQuery("SELECT DISTINCT parentfolderid FROM mailmessages WHERE",
                                   key,
                                   "messageFolderIds folder select query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    while (query.next())
        result->append(QMailFolderId(d->extractValue<quint64>(query.value(0))));

    return Success;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptFolderAccountIds(const QMailFolderKey &key, QMailAccountIdList *result, MailStoreReadLock&) const
{
    QSqlQuery query(d->simpleQuery("SELECT DISTINCT parentaccountid FROM mailfolders WHERE",
                                   key,
                                   "folderAccountIds account select query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    while (query.next())
        result->append(QMailAccountId(d->extractValue<quint64>(query.value(0))));

    return Success;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptFolderAncestorIds(const QMailFolderIdList &ids, QMailFolderIdList *result, MailStoreReadLock&) const
{
    QVariantList idValues;
    foreach (const QMailFolderId& id, ids)
        idValues.append(id.toULongLong());

    QString sql("SELECT DISTINCT id FROM mailfolderlinks WHERE descendantid IN %1");
    QSqlQuery query(d->simpleQuery(sql.arg(d->expandValueList(idValues)),
                                   idValues,
                                   "folderAncestorIds id select query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    while (query.next())
        result->append(QMailFolderId(d->extractValue<quint64>(query.value(0))));

    return Success;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptRestoreToPreviousFolder(const QMailMessageKey &key, MailStoreTransaction& t)
{
    QMailMessageIdList modifiedMessageIds;
    QMailFolderIdList modifiedFolderIds;
    QMailAccountIdList modifiedAccountIds;

    // Find the message and folders that are affected by this update
    QSqlQuery query(d->simpleQuery("SELECT id, parentfolderid, previousparentfolderid FROM mailmessages WHERE",
                                    key,
                                    "restoreToPreviousFolder info query"));
    if (query.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    QSet<quint64> folderIdSet;
    while (query.next()) {
        modifiedMessageIds.append(QMailMessageId(d->extractValue<quint64>(query.value(0))));

        folderIdSet.insert(d->extractValue<quint64>(query.value(1)));
        folderIdSet.insert(d->extractValue<quint64>(query.value(2)));
    }

    if (!folderIdSet.isEmpty()) {
        QMailFolderIdList folderIds;
        foreach (quint64 id, folderIdSet)
            folderIds.append(QMailFolderId(id));

        // Find the set of folders and accounts whose contents are modified by this update
        AttemptResult result = affectedByFolderIds(folderIds, &modifiedFolderIds, &modifiedAccountIds);
        if (result != Success)
            return result;

        // Update the message records
        QSqlQuery query(d->simpleQuery("UPDATE mailmessages "
                                       "SET parentfolderid = previousparentfolderid, previousparentfolderid = NULL WHERE",
                                       key,
                                       "restoreToPreviousFolder update query"));
        if (query.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;
    }

    if (t.commit()) {
        if (!modifiedMessageIds.isEmpty()) {
            //update the header cache
            foreach (const QMailMessageId &id, modifiedMessageIds) {
                if (d->headerCache.contains(id)) {
                    QMailMessageMetaData cachedMetaData = d->headerCache.lookup(id);
                    cachedMetaData.setParentFolderId(cachedMetaData.previousParentFolderId());
                    cachedMetaData.setPreviousParentFolderId(QMailFolderId());
                    cachedMetaData.committed();
                    d->headerCache.insert(cachedMetaData);
                }
            }

            //synchronize
            d->notifyMessagesChange(QMailStorePrivate::Updated, modifiedMessageIds);
            d->notifyFoldersChange(QMailStorePrivate::ContentsModified, modifiedFolderIds);
            if (!modifiedAccountIds.isEmpty())
                d->notifyAccountsChange(QMailStorePrivate::ContentsModified, modifiedAccountIds);

            emit messagesUpdated(modifiedMessageIds);
            emit folderContentsModified(modifiedFolderIds);
            if (!modifiedAccountIds.isEmpty())
                emit accountContentsModified(modifiedAccountIds);
        }

        return Success;
    }

    return DatabaseFailure;
}

void QMailStore::preloadHeaderCache(const QMailMessageId& id) const
{
    QMailMessageIdList idBatch;
    idBatch.append(id);

    int index = d->lastQueryMessageResult.indexOf(id);
    if (index != -1) {
        // Preload based on result of last call to queryMessages
        int count = 1;

        QMailMessageIdList::const_iterator begin = d->lastQueryMessageResult.begin();
        QMailMessageIdList::const_iterator end = d->lastQueryMessageResult.end();
        QMailMessageIdList::const_iterator lowIt = begin + index;
        QMailMessageIdList::const_iterator highIt = lowIt;

        bool ascend(true);
        bool descend(lowIt != begin);

        while ((count < (QMailStorePrivate::lookAhead * 2)) && (ascend || descend)) {
            if (ascend) {
                ++highIt;
                if (highIt == end) {
                    ascend = false;
                } else  {
                    if (!d->headerCache.contains(*highIt)) {
                        idBatch.append(*highIt);
                        ++count;
                    } else {
                        // Most likely, a sequence in the other direction will be more useful
                        ascend = false;
                    }
                }
            }

            if (descend) {
                --lowIt;
                if (!d->headerCache.contains(*lowIt)) {
                    idBatch.prepend(*lowIt);
                    ++count;

                    if (lowIt == begin) {
                        descend = false;
                    }
                } else {
                    // Most likely, a sequence in the other direction will be more useful
                    descend = false;
                }
            }
        }
    } else {
        // Don't bother preloading - if there is a query result, we have now searched outside it;
        // we should consider it to have outlived its usefulness
        if (!d->lastQueryMessageResult.isEmpty())
            d->lastQueryMessageResult = QMailMessageIdList();
    }

    QMailMessageMetaData result;
    QMailMessageKey key(idBatch);
    foreach (const QMailMessageMetaData& metaData, messagesMetaData(key, QMailStorePrivate::allMessageProperties())) {
        if (metaData.id().isValid()) {
            d->headerCache.insert(metaData);
            if (metaData.id() == id)
                result = metaData;
        }
    }
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptStatusBit(const QString &name, const QString &context, int *result, MailStoreReadLock&) const
{
    QSqlQuery selectQuery(d->simpleQuery("SELECT COALESCE(statusbit, 0) FROM mailstatusflags WHERE name=? AND context=?",
                                         QVariantList() << name
                                                        << context,
                                         "mailstatusflags select"));
    if (selectQuery.lastError().type() != QSqlError::NoError)
        return DatabaseFailure;

    *result = 0;
    if (selectQuery.next())
        *result = d->extractValue<int>(selectQuery.value(0));

    return Success;
}

/*! \internal */
QMailStore::AttemptResult QMailStore::attemptRegisterStatusBit(const QString &name, const QString &context, int maximum, MailStoreTransaction& t)
{
    int highest = 0;

    {
        // Find the highest 
        QSqlQuery selectQuery(d->simpleQuery("SELECT MAX(statusbit) FROM mailstatusflags WHERE context=?",
                                             QVariantList() << context,
                                             "mailstatusflags register select"));
        if (selectQuery.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;

        if (selectQuery.next())
            highest = d->extractValue<int>(selectQuery.value(0));
    }

    if (highest == maximum) {
        return Failure;
    } else {
        QSqlQuery insertQuery(d->simpleQuery("INSERT INTO mailstatusflags (name,context,statusbit) VALUES (?,?,?)",
                                             QVariantList() << name 
                                                            << context
                                                            << (highest + 1),
                                             "mailstatusflags register insert"));
        if (insertQuery.lastError().type() != QSqlError::NoError)
            return DatabaseFailure;
    }

    if (!t.commit()) {
        qLog(Messaging) << "Could not commit statusflag changes to database";
        return DatabaseFailure;
    }

    return Success;
}

Q_GLOBAL_STATIC(QMailStore,QMailStoreInstance);

/*!
    Returns an instance of the QMailStore object.
*/

QMailStore* QMailStore::instance()
{
    static bool init = false;
    if (!init) {
        init = true;
        QMailStoreInstance()->d->initStore();
    }
    return QMailStoreInstance();
}

/*!
    \fn void QMailStore::accountsAdded(const QMailAccountIdList& ids)

    Signal that is emitted when the accounts in the list \a ids are
    added to the store.

    \sa accountsRemoved(), accountsUpdated()
*/

/*!
    \fn void QMailStore::accountsRemoved(const QMailAccountIdList& ids)

    Signal that is emitted when the accounts in the list \a ids are
    removed from the store.

    \sa accountsAdded(), accountsUpdated()
*/

/*!
    \fn void QMailStore::accountsUpdated(const QMailAccountIdList& ids)

    Signal that is emitted when the accounts in the list \a ids are
    updated within the store.

    \sa accountsAdded(), accountsRemoved()
*/

/*!
    \fn void QMailStore::accountContentsModified(const QMailAccountIdList& ids)

    Signal that is emitted when changes to messages and folders in the mail store
    affect the content of the accounts in the list \a ids.

    \sa messagesAdded(), messagesUpdated(), messagesRemoved(), foldersAdded(), foldersUpdated(), foldersRemoved()
*/

/*!
    \fn void QMailStore::messagesAdded(const QMailMessageIdList& ids)

    Signal that is emitted when the messages in the list \a ids are
    added to the mail store.

    \sa messagesRemoved(), messagesUpdated()
*/

/*!
    \fn void QMailStore::messagesRemoved(const QMailMessageIdList& ids)

    Signal that is emitted when the messages in the list \a ids are
    removed from the mail store.

    \sa messagesAdded(), messagesUpdated()
*/

/*!
    \fn void QMailStore::messagesUpdated(const QMailMessageIdList& ids)

    Signal that is emitted when the messages in the list \a ids are
    updated within the mail store.

    \sa messagesAdded(), messagesRemoved()
*/

/*!
    \fn void QMailStore::foldersAdded(const QMailFolderIdList& ids)

    Signal that is emitted when the folders in the list \a ids are
    added to the mail store.

    \sa foldersRemoved(), foldersUpdated()
*/

/*!
    \fn void QMailStore::foldersRemoved(const QMailFolderIdList& ids)

    Signal that is emitted when the folders in the list \a ids are
    removed from the mail store.

    \sa foldersAdded(), foldersUpdated()
*/

/*!
    \fn void QMailStore::foldersUpdated(const QMailFolderIdList& ids)

    Signal that is emitted when the folders in the list \a ids are
    updated within the mail store.

    \sa foldersAdded(), foldersRemoved()
*/

/*!
    \fn void QMailStore::messageRemovalRecordsAdded(const QMailAccountIdList& ids)

    Signal that is emitted when QMailMessageRemovalRecords are added to the store, 
    affecting the accounts listed in \a ids.

    \sa messageRemovalRecordsRemoved()
*/

/*!
    \fn void QMailStore::messageRemovalRecordsRemoved(const QMailAccountIdList& ids)

    Signal that is emitted when QMailMessageRemovalRecords are removed from the store, 
    affecting the accounts listed in \a ids.

    \sa messageRemovalRecordsAdded()
*/

/*!
    \fn void QMailStore::folderContentsModified(const QMailFolderIdList& ids)

    Signal that is emitted when changes to messages in the mail store
    affect the content of the folders in the list \a ids.

    \sa messagesAdded(), messagesUpdated(), messagesRemoved()
*/
