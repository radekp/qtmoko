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
#ifndef QMODEMPHONEBOOK_H
#define QMODEMPHONEBOOK_H

#include <qphonebook.h>

class QModemService;
class QModemPhoneBookPrivate;
class QModemPhoneBookOperation;
class QModemPhoneBookCache;
class QAtResult;
class QTextCodec;

class QTOPIAPHONEMODEM_EXPORT QModemPhoneBook : public QPhoneBook
{
    Q_OBJECT
public:
    explicit QModemPhoneBook( QModemService *service );
    ~QModemPhoneBook();

    QTextCodec *stringCodec() const;

public slots:
    void getEntries( const QString& store );
    void add( const QPhoneBookEntry& entry, const QString& store, bool flush );
    void remove( uint index, const QString& store, bool flush );
    void update( const QPhoneBookEntry& entry, const QString& store, bool flush );
    void flush( const QString& store );
    void setPassword( const QString& store, const QString& password );
    void clearPassword( const QString& store );
    void requestLimits( const QString& store );
    void requestFixedDialingState();
    void setFixedDialingState( bool enabled, const QString& pin2 );
    void preload( const QString& store );
    void flushCaches();
    void phoneBooksReady();
    void updateCodec( const QString& gsmCharset );

protected:
    virtual bool hasModemPhoneBookCache() const;
    virtual bool hasEmptyPhoneBookIndex() const;

private:
    void forceStorageUpdate();
    void updateStorageName( const QString& storage, QObject *target=0,
                            const char *slot=0 );
    void sendQuery( QModemPhoneBookCache *cache );
    QModemPhoneBookCache *findCache( const QString& store, bool fast=true,
                                     const QString& initialPassword = QString() );
    void flushOperations( QModemPhoneBookCache *cache );
    virtual void flushAdd( const QPhoneBookEntry& entry, QModemPhoneBookCache *cache );
    void flushRemove( uint index, QModemPhoneBookCache *cache );
    void flushUpdate( const QPhoneBookEntry& entry, QModemPhoneBookCache *cache );
    void removeAllCaches();

private slots:
    void cscsDone( bool ok, const QAtResult& result );
    void cscsSetDone( bool ok, const QAtResult& result );
    void readDone( bool ok, const QAtResult& result );
    void readFinished( QModemPhoneBookCache *cache );
    void queryDone( bool ok, const QAtResult& result );
    void slowTimeout();
    void fdQueryDone( bool ok, const QAtResult& result );
    void fdModifyDone( bool ok, const QAtResult& result );
    void selectDone( bool ok, const QAtResult& result );
    void requestCharset();
    void setGsmCharset();
    void setUcs2Charset();
    void cpbsDone( bool ok, const QAtResult& result );
    void requestStorages();

private:
    QModemPhoneBookPrivate *d;
};

#endif
