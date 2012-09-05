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

#include <QFile>
#include <QDebug>
#include <qtopialog.h>
#include <qled.h>

/*!
  Set led attribute. Function parameters reflect linux led interface.
  The implementation writes \a value to led attribute file. The file name
  is determined by \a device \a color \a function and \a attr arguments.
  
  For creating portable code you should use qLedDevice() function for
  \a device, qLedColorXXX() for \a color, qLedFunctionXXX() for \a function,
  qLedAttrXXX() for \a attr argument values.
  
  Some devices support only certain combination of color/function/attr
  combinations:
  
  GTA04: color can be "green" or "red" type can be "power" or "aux"
  GTA02: supports "blue power" "orange power" and "red aux"

  The function returns 1 if the value was succesfully written to led sysfs
  file, -1 of some of the arguments is NULL, 0 if writing to the sysfs file
  failed.
  */
int setLed(const char *device, const char *color,
           const char *function, const char *attr, const char *value)
{
    if (device == 0 || color == 0 || function == 0 || attr == 0)
        return -1;              // device does not have this attribute/led type

    char filename[255];
    sprintf(filename, "/sys/class/leds/%s:%s:%s/%s", device, color, function, attr);    // e.g. /sys/class/leds/gta04:green:power/brightness
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "setLed failed" << filename << f.errorString();
        return 0;
    }
    f.write(value);
    f.close();
    return 1;
}

#ifdef QT_QWS_GTA04

const char *qLedDevice()
{
    return "gta04";
}

const char *qLedFunctionPower()
{
    return "power";
}

const char *qLedFunctionCalls()
{
    return "aux";
}

const char *qLedFunctionSms()
{
    return "aux";
}

const char *qLedColorPower()
{
    return "red";
}

const char *qLedColorSms()
{
    return "green";
}

const char *qLedColorCalls()
{
    return "green";
}

const char *qLedAttrBrightness()
{
    return "brightness";
}

const char *qLedAttrTrigger()
{
    return "trigger";
}

const char *qLedAttrDelayOn()
{
    return "delay_on";
}

const char *qLedAttrDelayOff()
{
    return "delay_off";
}

const char *qLedMaxBrightness()
{
    return "255";
}

#elif QT_QWS_NEO

const char *qLedDevice()
{
    return "gta02";
}

const char *qLedFunctionPower()
{
    return "power";
}

const char *qLedFunctionSms()
{
    return "aux";
}

const char *qLedFunctionCalls()
{
    return "aux";
}

const char *qLedColorPower()
{
    return "orange";
}

const char *qLedColorSms()
{
    return "red";
}

const char *qLedColorCalls()
{
    return "red";
}

const char *qLedAttrBrightness()
{
    return "brightness";
}

const char *qLedAttrTrigger()
{
    return NULL;
}

const char *qLedAttrDelayOn()
{
    return NULL;
}

const char *qLedAttrDelayOff()
{
    return NULL;
}

const char *qLedMaxBrightness()
{
    return "255";
}

#else

const char *qLedDevice()
{
    return NULL;
}

const char *qLedFunctionPower()
{
    return NULL;
}

const char *qLedFunctionSms()
{
    return NULL;
}

const char *qLedFunctionCalls()
{
    return NULL;
}

const char *qLedColorPower()
{
    return NULL;
}

const char *qLedColorSms()
{
    return NULL;
}

const char *qLedColorCalls()
{
    return NULL;
}

const char *qLedAttrBrightness()
{
    return NULL;
}

const char *qLedAttrTrigger()
{
    return NULL;
}

const char *qLedAttrDelayOn()
{
    return NULL;
}

const char *qLedAttrDelayOff()
{
    return NULL;
}

const char *qLedMaxBrightness()
{
    return NULL;
}

#endif

/*!
  Set attribute on led that reports new SMS.
*/
bool qLedSetSms(const char *attr, const char *value)
{
    return setLed(qLedDevice(), qLedColorSms(), qLedFunctionSms(), attr,
                  value);
}

/*!
  Set attribute on led that reports missed calls.
*/
bool qLedSetCall(const char *attr, const char *value)
{
    return setLed(qLedDevice(), qLedColorCalls(), qLedFunctionCalls(), attr,
                  value);
}

/*!
  Set attribute on led that reports power state.
*/
bool qLedSetPower(const char *attr, const char *value)
{
    return setLed(qLedDevice(), qLedColorPower(), qLedFunctionPower(), attr,
                  value);
}

/*!
  Start indicating new SMS on SMS led.
*/
void qLedIndicatorNewSmsOn()
{
    qLedSetSms(qLedAttrBrightness(), qLedMaxBrightness());
    qLedSetSms(qLedAttrTrigger(), "timer");
    qLedSetSms(qLedAttrDelayOff(), "1024");
    qLedSetSms(qLedAttrDelayOn(), "1024");
}

/*!
  Stop indicating new SMS calls on SMS led.
*/
void qLedIndicatorNewSmsOff()
{
    qLedSetSms(qLedAttrBrightness(), "0");
    qLedSetSms(qLedAttrTrigger(), "none");
}

/*!
  Start indicating missed calls on call led.
*/
void qLedIndicatorMissedCallsOn()
{
    qLedSetCall(qLedAttrBrightness(), qLedMaxBrightness());
    qLedSetCall(qLedAttrTrigger(), "timer");
    qLedSetCall(qLedAttrDelayOff(), "1024");
    qLedSetCall(qLedAttrDelayOn(), "1024");
}

/*!
  Stop indicating missed calls on call led.
*/
void qLedIndicatorMissedCallsOff()
{
    qLedSetCall(qLedAttrBrightness(), "0");
    qLedSetCall(qLedAttrTrigger(), "none");
}

/*!
  Start indicating low power on power led.
*/
void qLedIndicatorLowPowerOn()
{
    qLedSetPower(qLedAttrBrightness(), qLedMaxBrightness());
    qLedSetPower(qLedAttrTrigger(), "timer");
    qLedSetPower(qLedAttrDelayOff(), "1024");
    qLedSetPower(qLedAttrDelayOn(), "1024");
}

/*!
  Stop indicating low power on power led.
*/
void qLedIndicatorLowPowerOff()
{
    qLedSetPower(qLedAttrBrightness(), "0");
    qLedSetPower(qLedAttrTrigger(), "none");
}
