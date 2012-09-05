/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2012 Radek Polak.
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

#ifndef QLED_H
#define QLED_H

#include <qtopiaglobal.h>

QTOPIABASE_EXPORT int setLed(const char *device, const char *color,
                             const char *function, const char *attr,
                             const char *value);

QTOPIABASE_EXPORT const char *qLedDevice();
QTOPIABASE_EXPORT const char *qLedFunctionPower();
QTOPIABASE_EXPORT const char *qLedFunctionCalls();
QTOPIABASE_EXPORT const char *qLedColorPower();
QTOPIABASE_EXPORT const char *qLedColorCalls();
QTOPIABASE_EXPORT const char *qLedAttrBrightness();
QTOPIABASE_EXPORT const char *qLedAttrTrigger();
QTOPIABASE_EXPORT const char *qLedAttrDelayOn();
QTOPIABASE_EXPORT const char *qLedAttrDelayOff();
QTOPIABASE_EXPORT const char *qLedMaxBrightness();

QTOPIABASE_EXPORT bool qLedSetSms(const char *attr, const char *value);
QTOPIABASE_EXPORT bool qLedSetCall(const char *attr, const char *value);
QTOPIABASE_EXPORT bool qLedSetPower(const char *attr, const char *value);

QTOPIABASE_EXPORT void qLedIndicatorNewSmsOn();
QTOPIABASE_EXPORT void qLedIndicatorNewSmsOff();

QTOPIABASE_EXPORT void qLedIndicatorMissedCallsOn();
QTOPIABASE_EXPORT void qLedIndicatorMissedCallsOff();

QTOPIABASE_EXPORT void qLedIndicatorLowPowerOn();
QTOPIABASE_EXPORT void qLedIndicatorLowPowerOff();

#endif
