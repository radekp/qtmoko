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

#ifndef CALLSCREEN_H
#define CALLSCREEN_H

#include "qabstractcallscreen.h"

#include <themedview.h>

#include <QListWidget>
#include "serverthemeview.h"
#include <QPhoneCall>

class DialerControl;
class QMenu;
class CallItemEntry;
class QAction;
class QLineEdit;
class QVBoxLayout;
class CallItemModel;
class SecondaryCallScreen;
class ThemeListModel;
class CallAudioHandler;
class AbstractAudioHandler;
class QPhoneCall;
class QAbstractMessageBox;
class MouseControlDialog;
class TaskManagerEntry;

class ThemedCallScreenView;

class ThemedCallScreen : public QAbstractCallScreen
{
    Q_OBJECT
public:
    ThemedCallScreen(QWidget *parent = 0, Qt::WFlags f = 0);

public slots:
    virtual void stateChanged();

protected:
    void closeEvent(QCloseEvent *e);

private slots:
    void raiseCallScreen();

private:
    ThemedCallScreenView *view;
};


class ThemedCallScreenView : public PhoneThemedView
{
    friend class CallItemListView;
    friend class CallItemEntry;
    friend class CallData;

    Q_OBJECT
public:
    ThemedCallScreenView(DialerControl *ctrl, QWidget *parent);

    bool tryClose();

signals:
    void acceptIncoming();
    void listEmpty();
    void hangupCall();
    void raiseCallScreen();
    void hideCallScreen();

public slots:
    void stateChanged();

protected slots:
    void manualLayout();

protected:
    virtual void themeLoaded( const QString &theme );
    QWidget *newWidget(ThemeWidgetItem* input, const QString& name);
    void showEvent(QShowEvent *);
    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);
    bool eventFilter(QObject *, QEvent *);
    void hideEvent( QHideEvent * );
    void mousePressEvent(QMouseEvent *);

private slots:
    CallItemEntry *findCall(const QPhoneCall &call, CallItemModel *model);
    void requestFailed(const QPhoneCall &,QPhoneCall::Request);
    void updateAll();
    void splitCall();
    void callSelected(const QModelIndex&);
    void themeItemReleased(ThemeItem*);
    void actionGsmSelected();
    void updateLabels();
    void setVideoWidget();
    void deleteVideoWidget();
    void initializeAudioConf();
    void grabMouse();
    void releaseMouse();
    void muteRingSelected();
    void callConnected(const QPhoneCall &);
    void callDropped(const QPhoneCall &);
    void callIncoming(const QPhoneCall &);
    void callDialing(const QPhoneCall &);
    void showProgressDlg();
    void hideProgressDlg();
    void interactionDelayTimeout();
    void rejectModalDialog();
    void initializeMouseControlDialog();
    void endCall();
    void showEndCall();

private:
    int activeCallCount() const { return activeCount; }
    int heldCallCount() const { return holdCount; }
    bool incomingCall() const { return incoming; }
    bool inMultiCall() const { return activeCount > 1 || holdCount > 1; }

    QString ringTone();
    void clearDtmfDigits(bool clearOneChar = false);
    void appendDtmfDigits(const QString &);
    void setSelectMode(bool);
    bool dialNumbers(const QString & numbers);
    void setGsmMenuItem();
    void setItemActive(const QString &name, bool active);

private:
    QString dtmfActiveCall;
    QString dtmfDigits;
    DialerControl *control;
    QLineEdit *digits;
    QListView *listView;
    QMenu *contextMenu;
    QAction *actionAnswer;
    QAction *actionSendBusy;
    QAction *actionMute;
    QAction *actionHold;
    QAction *actionResume;
    QAction *actionEnd;
    QAction *actionEndAll;
    QAction *actionMerge;
    QAction *actionSplit;
    QAction *actionTransfer;
    QAction *actionGsm;
    int activeCount;
    int holdCount;
    bool incoming;
    bool keypadVisible;
    QVBoxLayout *mLayout;
    QTimer* updateTimer;
    QTimer* gsmActionTimer;
    SecondaryCallScreen *secondaryCallScreen;
    ThemeListModel* m_model;
#ifdef QTOPIA_TELEPHONY
    CallAudioHandler * m_callAudioHandler;
    AbstractAudioHandler* m_callAudio;
#endif
    QWidget* videoWidget;
    bool showWaitDlg;
    QTimer *symbolTimer;
    MouseControlDialog *m_mouseCtrlDlg;
    TaskManagerEntry *m_taskManagerEntry;
};

#endif
