/*
 * This file was generated by qfsodbusxml2cpp version 0.7
 * Command line was: qfsodbusxml2cpp -p qfsopimmessage -c QFsoPIMMessage specs/xml/org.freesmartphone.PIM.Message.xml
 *
 * qfsodbusxml2cpp is Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#include "qfsopimmessage.h"

/*
 * Implementation of interface class QFsoPIMMessage
 */

QFsoPIMMessage::QFsoPIMMessage(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent)
    : QDBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
{
}

QFsoPIMMessage::~QFsoPIMMessage()
{
}
