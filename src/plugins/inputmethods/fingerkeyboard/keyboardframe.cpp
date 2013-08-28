/****************************************************************************
**
** Copyright (C) 2000-2008 TROLLTECH ASA. All rights reserved,
** Leonardo Maccari, Anton Olkhovik. All rights expressed in the GPL license.
**
**
** This software is licensed under the terms of the GNU General Public
** License (GPL) version 2.
**
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
**
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "keyboardframe.h"

#ifdef Q_WS_QWS
#include <qwindowsystem_qws.h>
#endif
#include <qpainter.h>
#include <qfontmetrics.h>
#include <qtimer.h>
#include <ctype.h>
#include <QtopiaServiceRequest>

#include <QPaintEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QApplication>
#include <QDesktopWidget>
#include <QMenu>
#include <QSoftMenuBar>

#include <qtopialog.h>

#include <QTextCodec>

#include <QDir>
#include <QSettings>
#include <Qtopia>
#include <QList>
#include <QStringList>


#define USE_SMALL_BACKSPACE
#define USE_LARGE_KEYS 1 
#define KEYBOARD_ROWS 6

#define KEYBOARD_PICS 6
#define KEYBOARD_OPTI_PICS 6 
#define PICK_PROPORTION 15

enum { BSCode = 0x80, TabCode, CapsCode, RetCode,
       ShiftCode, CtrlCode, AltCode, SpaceCode, BackSlash,
       UpCode, LeftCode, DownCode, RightCode, Blank, Expand,
       Opti, ResetDict,
       Divide, Multiply, Add, Subtract, Decimal, Equal,
       Percent, Sqrt, Inverse, Escape };

typedef struct SpecialMap {
    int qcode;
    ushort unicode;
    const char * label;
    const char * picName;
    QPixmap *pic;
};

static uchar *keyboard_national_6[KEYBOARD_ROWS]; //

#define SPEC_SYMBOL_COUNT  33
static SpecialMap specialM[] = {
    // LM: backspace pixmaps obviously doesn't scale
    {   Qt::Key_Backspace,      8,      "<",     "backspace", 0 },
    {   Qt::Key_Tab,            9,      "Tab",   NULL, 0}, // No tr
    {   Qt::Key_CapsLock,       0xffff, "Caps",  NULL, 0 }, // No tr
    {   Qt::Key_Return,         13,     "Ret",   NULL, 0 }, // No tr
    {   Qt::Key_Shift,          0xffff, "Shift", NULL, 0 }, // No tr
    {   Qt::Key_Control,        0xffff, "Ctrl",  NULL, 0 }, // No tr
    {   Qt::Key_Alt,            0xffff, "Alt",   NULL, 0 }, // No tr
    {   Qt::Key_Space,          ' ',    "",      NULL, 0 },
    {   BackSlash,              43,     "\\",    NULL, 0 },

    // Need images?
    {   Qt::Key_Up,             0xffff, "^",     "uparrow", 0 },
    {   Qt::Key_Left,           0xffff, "<",     "leftarrow", 0 },
    {   Qt::Key_Down,           0xffff, "v",     "downarrow", 0 },
    {   Qt::Key_Right,          0xffff, ">",     "rightarrow", 0 },
    {   Qt::Key_Insert,         0xffff, "I",     "insert", 0 },
    {   Qt::Key_Home,           0xffff, "H",     "home", 0 },
    {   Qt::Key_PageUp,         0xffff, "U",     "pageup", 0 },
    {   Qt::Key_End,            0xffff, "E",     "end", 0 },
    {   Qt::Key_Delete,         0xffff, "X",     "delete", 0 },
    {   Qt::Key_PageDown,       0xffff, "D",     "pagedown", 0 },
    {   Blank,                  0,      " ",     NULL, 0 },
    {   Expand,                 0xffff, "->",    "expand", 0 },
    {   Opti,                   0xffff, "Switch",     NULL, 0 },
    {   ResetDict,              0xffff, "R",     NULL, 0 },

    // number pad stuff
    {   Divide,                 0,      "/",     NULL, 0 },
    {   Multiply,               0,      "*",     NULL, 0 },
    {   Add,                    0,      "+",     NULL, 0 },
    {   Subtract,               0,      "-",     NULL, 0 },
    {   Decimal,                0,      ".",     NULL, 0 },
    {   Equal,                  0,      "=",     NULL, 0 },
    {   Percent,                0,      "%",     NULL, 0 },
    {   Sqrt,                   0,      "^1/2",  NULL, 0 },
    {   Inverse,                0,      "1/x",   NULL, 0 },

    {   Escape,                 27,     "ESC",   "escape", 0 }, // <<---[32]

    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 }, { 0,0,NULL,NULL,0 },
    { 0,0,NULL,NULL,0 }
};


KeyboardFrame::KeyboardFrame(QWidget* parent, Qt::WFlags f) :
    QFrame(parent, f), shift(false), lock(false), ctrl(false),
    alt(false), useLargeKeys(USE_LARGE_KEYS), layoutNumber(0), pressedKey(-1),
    unicode(-1), qkeycode(0), modifiers(Qt::NoModifier), pressTid(0), pressed(false),
    positionTop(true), nationalLoaded(false), useSkin(false),
    layoutFileName("/etc/fingerkeyboard/extralayout.conf"),
    skinFileName("/etc/fingerkeyboard/skin.conf")
{

    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    LoadNationalLayout();

/* begin of hack code for screen rotation

	QtopiaServiceRequest svreq("RotationManager", "setCurrentRotation(int)");
	svreq << 90;
	svreq.send();

 end of hack code for screen rotation*/ 
    setAttribute(Qt::WA_InputMethodTransparent, true);

    setPalette(QPalette(QColor(220,220,220))); // Gray
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setFrameStyle(QFrame::Plain | QFrame::Box);

    // need to restrict font for screen height()
    QRect mwr = QApplication::desktop()->availableGeometry();
    QFont fnt = QApplication::font();


    qreal maxPixHeight = mwr.height()/(PICK_PROPORTION); 
                                            // LM: each Pick (a letterblock) is 1/PICK_PROPORTION of screen in height 
                                            // there are KEYBOARD_ROWS rows, fontHeight being half a row height;
                                            // did I get this right?
    qreal maxPointHeight = (maxPixHeight*72)/logicalDpiY();

    fnt.setPointSizeF(qMin(maxPointHeight, fnt.pointSizeF()));
    setFont(fnt);

    QPalette pal(palette());
    QColor col(Qt::lightGray);
    col.setAlpha(192);
    pal.setColor(QPalette::Background, col);
    setPalette(pal);
    setAutoFillBackground(true);

    int specialIndex=0;
    while  (specialM[specialIndex].qcode != 0 ){
        if (specialM[specialIndex].picName != NULL) {
            QString pName = QString(":image/keyboard/") + specialM[specialIndex].picName;
            specialM[specialIndex].pic = new QPixmap(pName);
        }else{
        }
        specialIndex++;
    }

    LoadSkin(); //

    picks = new KeyboardPicks( this );
    picks->initialise();

    repeatTimer = new QTimer( this );
    connect( repeatTimer, SIGNAL(timeout()), this, SLOT(repeat()) );

    emit needsPositionConfirmation();
}

KeyboardFrame::~KeyboardFrame()
{
    int specialIndex=0;
    while  (specialM[specialIndex].qcode != 0 ){
        delete specialM[specialIndex].pic;
        specialIndex++;
    }
//	QtopiaServiceRequest svreq("RotationManager", "defaultRotation()");
//	svreq.send();

    //free memory allocated for additional layout
    if (nationalLoaded)
    {
        for (int i=0; i<KEYBOARD_ROWS; i++)
            free(keyboard_national_6[i]);
    }

    if (useSkin)
    {
        delete btn_left;
        delete btn_right;
        delete btn_mid;
        delete btn_pressed_left;
        delete btn_pressed_right;
        delete btn_pressed_mid;
    }
}

void KeyboardFrame::showEvent(QShowEvent *e)
{
    qwsServer->sendIMQuery ( Qt::ImMicroFocus );

    QRect mwr = QApplication::desktop()->availableGeometry();

    releaseKeyboard();
    int ph = picks->sizeHint().height();

//    picks->setGeometry( 0, 0, width(), ph );
//    move(0,mwr.height()-height());

    keyHeight = (height()-ph)/KEYBOARD_ROWS;
    int nk;
    if ( layoutNumber == OPTI_LAYOUT_NUMBER ) {
        nk = KEYBOARD_OPTI_PICS;
    } else if ( useLargeKeys ) {
        nk = KEYBOARD_PICS; // LM: unused ?
    } else {
        nk = KEYBOARD_PICS; 
    }
    defaultKeyWidth = width()/nk;
    xoffs = (width()-defaultKeyWidth*nk)/2;
    QFrame::showEvent(e);
    // Let keyboardimpl know that we're hidden now, and it should update the menu
    emit showing();
}

void KeyboardFrame::hideEvent( QHideEvent * )
{
    // don't want keypresses to keep going if the widget is hidden, so reset state
    resetState();
    // Let keyboardimpl know that we're hidden now, and it should update the menu
    emit hiding();
}


void KeyboardFrame::resizeEvent(QResizeEvent*)
{
    int ph = picks->sizeHint().height();
//    picks->setGeometry( 0, 50, width(), ph );
    keyHeight = (height()-ph)/KEYBOARD_ROWS;
    int nk;
    if ( layoutNumber == OPTI_LAYOUT_NUMBER ) {
        nk = KEYBOARD_OPTI_PICS;
    } else if ( useLargeKeys ) {
        nk = KEYBOARD_PICS; // unused ?
    } else {
        nk = KEYBOARD_PICS; 
    }
    defaultKeyWidth = width()/nk;
    xoffs = (width()-defaultKeyWidth*nk)/2;
}


KeyboardPicks::~KeyboardPicks()
{
    delete dc;
}

void KeyboardPicks::initialise()
{
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed));
    mode = 0;
    dc = new KeyboardConfig(this);
    configs.append(dc);
}

QSize KeyboardPicks::sizeHint() const
{
    return QSize(240, 1);
// LM resize prediction bar
//    return QSize(240, fontMetrics().lineSpacing());
}


void  KeyboardConfig::generateText(const QString &s)
{
#if defined(Q_WS_QWS) || defined(Q_WS_QWS)
                  int i;
    for ( i=0; i<(int)backspaces; i++) {
        qwsServer->processKeyEvent( 8, Qt::Key_Backspace, 0, true, false );
        qwsServer->processKeyEvent( 8, Qt::Key_Backspace, 0, false, false );
    }
    for ( i=0; i<(int)s.length(); i++) {
        uint code = 0;
        if ( s[i].unicode() >= 'a' && s[i].unicode() <= 'z' ) {
            code = s[i].unicode() - 'a' + Qt::Key_A;
        }
        qwsServer->processKeyEvent( s[i].unicode(), code, 0, true, false );
        qwsServer->processKeyEvent( s[i].unicode(), code, 0, false, false );
    }
    qwsServer->processKeyEvent( ' ', Qt::Key_Space, 0, true, false );
    qwsServer->processKeyEvent( ' ', Qt::Key_Space, 0, false, false );
    backspaces = 0;
#endif
}


//PC keyboard layout and scancodes

/*
  Format: length, code, length, code, ..., 0

  length is measured in half the width of a standard key.
  If code < 0x80, code gives the ASCII value of the key

  If code >= 0x80, the key is looked up in specialM[].

  When changing keymap, remember to resize also KEYBOARD_OPTI_PICS
  and KEYBOARD_PICS
 */


/*
static const uchar * const keyboard_opti[KEYBOARD_ROWS] = {
    (const uchar *const) "\001\223\002\240\002\20\002\41\002\26\002\62\002\56\002\45\002\54\003\200\001\223\002\226\002\235\002\234\002\236",
    (const uchar *const) "\001\223\002\201\004\207\002\30\002\24\002\43\004\207\003\203\001\223\006\002\002\065",
    (const uchar *const) "\001\223\002\202\002\60\002\37\002\23\002\22\002\36\002\21\002\55\003\203\001\223\006\005\002\055",
    (const uchar *const) "\001\223\002\205\004\207\002\27\002\61\002\40\004\207\003\204\001\223\006\010\002\014",
    (const uchar *const) "\001\223\002\206\002\44\002\31\002\57\002\42\002\46\002\25\002\207\003\204\001\223\002\013\002\064\002\015\002\230"
};
*/

static const uchar * const keyboard_opti_6[KEYBOARD_ROWS] = {
    (const uchar *const) "\002`\002(\002)\0021\0022\0023",
    (const uchar *const) "\002;\002:\002=\0024\0025\0026",
    (const uchar *const) "\002?\002!\002%\0027\0028\0029",
    (const uchar *const) "\002,\002.\002*\002@\0020\002#",
    (const uchar *const) "\002^\002\\\002/\002$\002-\002+",
    (const uchar *const) "\002\204\002\225\006\207\002\200"
};

static const uchar * const keyboard_opti[KEYBOARD_ROWS - 2] = {
    (const uchar *const) "\0021\0022\0023\0024\0025\0026\0027\0028\0029\0020",
    (const uchar *const) "\002/\002\\\002(\002)\002:\002;\002<\002>\002@\002#",
    (const uchar *const) "\002!\002?\002$\002%\002&\002=\002*\002+\002`\002^",
    (const uchar *const) "\004\204\004\225\010\207\004\200"
};

/*
static const uchar * const keyboard_standard[KEYBOARD_ROWS] = {

#ifdef USE_SMALL_BACKSPACE
    (const uchar *const)"\002\240\002`\0021\0022\0023\0024\0025\0026\0027\0028\0029\0020\002-\002=\002\200\002\223\002\215\002\216\002\217",
#else
    (const uchar *const)"\002\051\0021\0022\0023\0024\0025\0026\0027\0028\0029\0020\002-\002=\004\200\002\223\002\215\002\216\002\217",
#endif
    //~ + 123...+ BACKSPACE //+ INSERT + HOME + PGUP

    (const uchar *const)"\002\201\002q\002w\002e\002r\002t\002y\002u\002i\002o\002p\002[\002]\002\\\001\224\002\223\002\221\002\220\002\222",
    //TAB + qwerty..  + backslash //+ DEL + END + PGDN

    (const uchar *const)"\004\202\002a\002s\002d\002f\002g\002h\002j\002k\002l\002;\002'\004\203",
    //CAPS + asdf.. + RETURN

    (const uchar *const)"\005\204\002z\002x\002c\002v\002b\002n\002m\002,\002.\002/\005\204\002\223\002\223\002\211",
    //SHIFT + zxcv... //+ UP

    (const uchar *const)"\002\205\003\206\022\207\003\206\003\205\002\223\002\212\002\213\002\214"
    //CTRL + ALT + SPACE //+ LEFT + DOWN + RIGHT

};
*/


/* 
 qwertyu
 asdfghj
 zxcvbnm
 iopkl,'
*/

static const uchar * const keyboard_standard_6[KEYBOARD_ROWS] = {

    (const uchar *const)"\002q\002w\002e\002r\002t\002y",

    (const uchar *const)"\002a\002s\002d\002f\002g\002h",

    (const uchar *const)"\002z\002x\002c\002v\002b\002n",
    
    (const uchar *const)"\002i\002o\002p\002k\002l\002\x83",
    
    (const uchar *const)"\002u\002j\002m\002,\002.\002'",

    (const uchar *const)"\002\204\002\225\006\207\002\200"
    // why can't I use \008 for space length and why this last line is 22
    // blocks ??
    //SHIFT + SWITCH + SPACE + BACKSPACE //

};

static const uchar * const keyboard_standard[KEYBOARD_ROWS - 2] = {

    (const uchar *const)"\002q\002w\002e\002r\002t\002y\002u\002i\002o\002p",
    // qwertyuuiop //

    (const uchar *const)"\002a\002s\002d\002f\002g\002h\002j\002k\002l\002-",
    //asdfghjkl-/ 

    (const uchar *const)"\002z\002x\002c\002v\002b\002n\002m\002,\002.\002'",
    //zxcvbnm,.' //

    (const uchar *const)"\004\204\004\225\010\207\004\200"
    // why can't I use \008 for space length and why this last line is 22
    // blocks ??
    //SHIFT + SWITCH + SPACE + BACKSPACE //

};


struct ShiftMap {
    int normal;
    int shifted;
};

#define SHIFT_SYMBOL_COUNT  12
static ShiftMap shiftMap[] = {
    { '1', '<' },
    { '2', '>' },
    { '3', '"' },
    { '4', '[' },
    { '5', ']' },
    { '6', '{' },
    { '7', '}' },
    { '8', '|' },
    { '9', '~' },
    { '0', '^' },
    { '.', '-' },
    { ',', '\'' }, // <<---[11]

    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0}
};


int KeyboardFrame::keycode( int i2, int j, const uchar **keyboard, QRect *repaintrect )
{
    if ( j <0 || j >= KEYBOARD_ROWS )
        return 0;

    const uchar *row = keyboard[j];

    int x = 0;
    while ( *row && x+*row <= i2 ) {
        x += *row;
        row += 2;
    }

    if ( !*row ) return 0;

    if ( repaintrect ) {
        *repaintrect = QRect(
            x*defaultKeyWidth/2+xoffs,
            j*keyHeight+picks->height(),
            *row*defaultKeyWidth/2,keyHeight);
    }

    int k;
    if ( row[1] >= 0x80 ) {
        k = row[1];
    } else {
        k = row[1]+(i2-x)/2;
    }

    return k;
}


/*
  return scancode and width of first key in row \a j if \a j >= 0,
  or next key on current row if \a j < 0.

*/

int KeyboardFrame::getKey( int &w, int j ) {
    static const uchar *row = 0;
    static int key_i = 0;
    static int scancode = 0;
    static int half = 0;

    if ( j >= 0 && j < KEYBOARD_ROWS ) {
        if (layoutNumber == OPTI_LAYOUT_NUMBER)
            row = keyboard_opti_6[j];
        else if (layoutNumber == NATIONAL_LAYOUT_NUMBER)
            row = keyboard_national_6[j];
        else
            row = keyboard_standard_6[j];
        half=0;
    }

    if ( !row || !*row ) {
        return 0;
    } else if ( row[1] >= 0x80 ) {
        scancode = row[1];
        w = (row[0] * w + (half++&1)) / 2;
        row += 2;
        return scancode;
    } else if ( key_i <= 0 ) {
        key_i = row[0]/2;
        scancode = row[1];
    }
    key_i--;
    if ( key_i <= 0 )
        row += 2;
    return scancode++;
}


void KeyboardFrame::paintEvent(QPaintEvent* e)
{
    QPainter painter(this);
    painter.setClipRect(e->rect());
    drawKeyboard( painter, e->rect() );
    picks->dc->draw( &painter );
}

int KeyboardFrame::FindShifted(int k)
{
    int shifted = k;
    bool shiftFound = false;
    for ( unsigned i = 0; shiftMap[i].normal != 0; i++ )
        if ( shiftMap[i].normal == k )
        {
            shifted = shiftMap[i].shifted;
            shiftFound = true;
            break;
        }

    if ( (!shiftFound) && (k < 0x80) )
        if ( isalpha( k ) ) {
            shifted = toupper( k );
        }
    return shifted;
}

/*
  Draw the KeyboardFrame.

  If key >= 0, only the specified key is drawn.
*/
void KeyboardFrame::drawKeyboard( QPainter &p, const QRect& clip, int key )
{
    const bool threeD = false;
    QColor keycolor = palette().button().color();
    QColor keycolor_pressed = palette().mid().color();
    QColor keycolor_lo = palette().dark().color();
    QColor keycolor_hi = palette().light().color();
    QColor textcolor = palette().text().color();

    int margin = threeD ? 1 : 0;

//    p.fillRect( 0, , kw-1, keyHeight-2, keycolor_pressed );

    for ( int j = 0; j < KEYBOARD_ROWS; j++ ) {
        int y = j * keyHeight + picks->height() + 1;
        if ( y <= clip.bottom() && y+keyHeight >= clip.top() ) {
            int x = xoffs;
            int kw = defaultKeyWidth;
            int k= getKey( kw, j );
            while ( k ) {
                if ( (key < 0 || k == key) && x <= clip.right() && x+kw > clip.left() ) {
                    QString s;
                    bool pressed = (k == pressedKey);
                    bool blank = (k == 0223);
                    QPixmap *pic = 0;

                    bool catched = false;
                    int kcode = k;

                    if (k >= 0x80 + SPEC_SYMBOL_COUNT)
                    {
                        kcode = specialM[k - 0x80].unicode;
                    }
                    else if ( k >= 0x80 )
                    {
                        catched = true;
                        s = specialM[k - 0x80].label;
                        pic = specialM[k - 0x80].pic;

                        if ( k == ShiftCode ) {
                            pressed = shift;
                        } else if ( k == CapsCode ) {
                            pressed = lock;
                        } else if ( k == CtrlCode ) {
                            pressed = ctrl;
                        } else if ( k == AltCode ) {
                            pressed = alt;
                        }
                    }

                    if (!catched)
                    {
    #if defined(Q_WS_QWS) || defined(Q_WS_QWS)
    /*
                        s = QChar( shift^lock ? QWSServer::keyMap()[k].shift_unicode :
                                   QWSServer::keyMap()[k].unicode);
    */
                        // ### Fixme, bad code, needs improving, whole thing needs to
                        // be re-coded to get rid of the way it did things with scancodes etc

                        int shifted = FindShifted(kcode);
                        s = QChar( shift^lock ? shifted : kcode );
    #endif
                    }

                    if (useSkin)
                    {
                        QPixmap *bleft, *bright, *bmid;
                        if (pressed)
                        {
                            bleft = btn_pressed_left;
                            bright = btn_pressed_right;
                            bmid = btn_pressed_mid;
                        }
                        else
                        {
                            bleft = btn_left;
                            bright = btn_right;
                            bmid = btn_mid;
                        }
                        int left_w = bleft->width();
                        int right_w = bright->width();
                        p.drawPixmap(x, y, *bleft);
                        p.drawPixmap(x+left_w, y,
                                     kw-left_w-right_w, bmid->height(),
                                     *bmid);
                        p.drawPixmap(x+kw-right_w, y, *bright);

                        if (pic && !pic->isNull()) {
                            p.drawPixmap( x + 1, y + 2, *pic );
                        } else {
                            p.setPen(textcolor);
                            p.drawText( x - 1, y, kw, keyHeight-2, Qt::AlignCenter, s ); //
                        }
                    }
                    else
                    {
                        if (!blank) {
                            if ( pressed )
                                p.fillRect( x+margin, y+margin, kw-margin, keyHeight-margin-1, keycolor_pressed );
                            else
                                p.fillRect( x+margin, y+margin, kw-margin, keyHeight-margin-1, keycolor );

                            if ( threeD ) {
                                p.setPen(pressed ? keycolor_lo : keycolor_hi);
                                p.drawLine( x, y+1, x, y+keyHeight-2 );
                                p.drawLine( x+1, y+1, x+1, y+keyHeight-3 );
                                p.drawLine( x+1, y+1, x+1+kw-2, y+1 );
                            } else if ( j == 0 ) {
                                p.setPen(pressed ? keycolor_hi : keycolor_lo);
                                p.drawLine( x, y, x+kw, y );
                            }

                            // right
                            p.setPen(pressed ? keycolor_hi : keycolor_lo);
                            p.drawLine( x+kw-1, y, x+kw-1, y+keyHeight-2 );

                            if ( threeD ) {
                                p.setPen(keycolor_lo.light());
                                p.drawLine( x+kw-2, y+keyHeight-2, x+kw-2, y+1 );
                                p.drawLine( x+kw-2, y+keyHeight-2, x+1, y+keyHeight-2 );
                            }

                            if (pic && !pic->isNull()) {
                                p.drawPixmap( x + 1, y + 2, *pic );
                            } else {
                                p.setPen(textcolor);
                                p.drawText( x - 1, y, kw, keyHeight-2, Qt::AlignCenter, s ); //
                            }

                            if ( threeD ) {
                                p.setPen(keycolor_hi);
                                p.drawLine( x, y, x+kw-1, y );
                            }
                        } else {
                            p.fillRect( x, y, kw, keyHeight, keycolor );
                        }
                    }
                }

                x += kw;
                kw = defaultKeyWidth;
                k = getKey( kw );
            }
        }
    }
}


void KeyboardFrame::mousePressEvent(QMouseEvent *e)
{
    clearHighlight(); // typing fast?

    int i2 = ((e->x() - xoffs) * 2) / defaultKeyWidth;
    int j = (e->y() - picks->height()) / keyHeight;

    QRect keyrect;

    const uchar **cur_keys;
    if (layoutNumber == OPTI_LAYOUT_NUMBER) 
        cur_keys = (const uchar **)keyboard_opti_6;
    else if (layoutNumber == NATIONAL_LAYOUT_NUMBER)
        cur_keys = (const uchar **)keyboard_national_6;
    else
        cur_keys = (const uchar **)keyboard_standard_6;
    
    int k = keycode( i2, j, cur_keys, &keyrect );

    bool key_down = false;
    unicode = -1;
    qkeycode = 0;

    bool catched = false;
    int kcode = k;

    if (k >= 0x80 + SPEC_SYMBOL_COUNT)
    {
        kcode = specialM[k - 0x80].unicode;
    }
    else if ( k >= 0x80 )
    {
        catched = true;        
        if ( k == ShiftCode ) {
            shift = !shift;
            keyrect = rect();
        } else if ( k == AltCode ){
            alt = !alt;
        } else if ( k == CapsCode ) {
            lock = !lock;
            keyrect = rect();
        } else if ( k == CtrlCode ) {
            ctrl = !ctrl;
        } else if ( k == 0224 /* Expand */ ) {
            useLargeKeys = !useLargeKeys;
            resizeEvent(0);
            repaint( ); // need it to clear first
        } else if ( k == 0225 /* Opti/Toggle */ ) {
            //useOptiKeys = !useOptiKeys;
            layoutNumber++;
            if ((layoutNumber == NATIONAL_LAYOUT_NUMBER) && (!nationalLoaded)) layoutNumber++;
            if (layoutNumber >= LAYOUTS_COUNT) layoutNumber=0;
            resizeEvent(0);
            repaint( ); // need it to clear first
        } else {
            qkeycode = specialM[ k - 0x80 ].qcode;
            unicode = specialM[ k - 0x80 ].unicode;
        }
    }

    if (!catched)
    {
        int shifted = FindShifted(kcode);
        qkeycode = shifted;
        unicode = shift^lock ? shifted : kcode;
/*
        //due to the way the keyboard is defined, we know that
        //k is within the ASCII range, and can be directly mapped to
        //a qkeycode; except letters, which are all uppercase
        qkeycode = toupper(k);
        if ( shift^lock ) {
            if ( !isalpha( k ) ) {
            for ( unsigned i = 0; i < sizeof(shiftMap)/sizeof(ShiftMap); i++ )
                if ( shiftMap[i].normal == k ) {
                    unicode = shiftMap[i].shifted;
                    qkeycode = unicode;
                    break;
                }
            } else {
                unicode = toupper( k );
            }
        } else {
            unicode = k;
        }
*/
    }
    if  ( unicode != -1 ) {
        if ( ctrl && unicode >= 'a' && unicode <= 'z' )
            unicode = unicode - 'a'+1;

        modifiers = Qt::NoModifier;
        if (shift) {
            modifiers |= Qt::ShiftModifier;
            keyrect = rect();
        }
        if (ctrl)
            modifiers |= Qt::ControlModifier;
        if (alt)
            modifiers |= Qt::AltModifier;

        qLog(Input) << "keypressed: code=" << unicode;

        qwsServer->processKeyEvent( unicode, qkeycode, modifiers, true, false );

        shift = alt = ctrl = false;


        KeyboardConfig *dc = picks->dc;

        if (dc) {
            if (qkeycode == Qt::Key_Backspace) {
                if (dc->input.count()) {
                    dc->input.removeLast();
                    dc->decBackspaces();
                }
            } else if ( k == 0226 || qkeycode == Qt::Key_Return ||
                        qkeycode == Qt::Key_Space ||
                        QChar(unicode).isPunct() ) {
                dc->input.clear();
                dc->resetBackspaces();
            } else {
// LM uncommenting these lines turns prediction off 
//                dc->add(QString(QChar(unicode)));
//                dc->incBackspaces();
            }
        }

        picks->repaint();

        key_down = true;
    }
    pressedKey = k;
    pressedKeyRect = keyrect;
    repaint(keyrect);
    if ( pressTid )
        killTimer(pressTid);
    pressTid = startTimer(80);
    pressed = true;
    if(key_down)
    {
        repeatTimer->start( 500 );
    };
    emit needsPositionConfirmation();
}


void KeyboardFrame::mouseReleaseEvent(QMouseEvent*)
{
    repeatTimer->stop();
    if ( pressTid == 0 )
        clearHighlight();
#if defined(Q_WS_QWS) || defined(Q_WS_QWS)
    if ( unicode != -1 || qkeycode != 0) {
        qLog(Input) << "keyrelease: code=" << unicode;
        qwsServer->processKeyEvent( unicode, qkeycode, modifiers, false, false );
    }
#endif
    pressed = false;
}

void KeyboardFrame::timerEvent(QTimerEvent* e)
{
    if ( e->timerId() == pressTid ) {
        killTimer(pressTid);
        pressTid = 0;
        if ( !pressed )
            clearHighlight();
    }
}

void KeyboardFrame::repeat()
{
    if ( pressed && (unicode != -1 || qkeycode != 0)) {
        repeatTimer->start( 150 );
        qwsServer->processKeyEvent( unicode, qkeycode, modifiers, true, true );
    } else
        repeatTimer->stop();
}

void KeyboardFrame::clearHighlight()
{
    if ( pressedKey >= 0 ) {
        pressedKey = -1;
        repaint(pressedKeyRect);
    }
}


QSize KeyboardFrame::sizeHint() const
{
    QFontMetrics fm=fontMetrics();
    int keyHeight = fm.lineSpacing()+2;

    if (layoutNumber == OPTI_LAYOUT_NUMBER)
        keyHeight += 1;

    return QSize( 320, keyHeight * KEYBOARD_ROWS + picks->sizeHint().height() + 1 );
}


void KeyboardFrame::resetState()
{
    picks->resetState();
    shift = false;
    lock = false;
    ctrl = false;
    alt = false;
    pressedKey = -1;
    unicode = -1;
    qkeycode = 0;
    modifiers = Qt::NoModifier;
    pressed = false;
    if ( pressTid ) {
        killTimer(pressTid);
        pressTid = 0;
    };
    repeatTimer->stop();

}


bool KeyboardFrame::obscures(const QPoint &point)
{
    QRect mwr = QApplication::desktop()->availableGeometry();
    bool isTop = point.y() < (mwr.y()+mwr.height()>>1);
    return (isTop == positionTop);
}

void KeyboardFrame::swapPosition()
{
    QRect mwr = QApplication::desktop()->availableGeometry();
    if(positionTop)
    {
        move(mwr.bottomLeft()-QPoint(0,height()));
        positionTop = false;
    } else
    {
        move(mwr.topLeft());
        positionTop = true;
    }
}


//====================================================================
//== Extra layout
//====================================================================

void KeyboardFrame::LoadNationalLayout()
{
    QString layout;
    if (QFile::exists(Qtopia::qtopiaDir() + layoutFileName))
    {
        layout = Qtopia::qtopiaDir() + layoutFileName;
    }
    else
    {
        layout = FindLayoutInSandboxes();
    }

    if (!layout.isNull())
    {
        qLog(Input) << "FingerKeyboard - loading additional layout";
        qLog(Input) << layout;

        //allocating memory for additional layout
        for (int i=0; i<KEYBOARD_ROWS; i++)
            keyboard_national_6[i] = (uchar*)malloc(12*2+2); //max 12 keys is the row

        LoadSet(layout);
        nationalLoaded = true;
    }
}

QString KeyboardFrame::FindLayoutInSandboxes()
{
     QString homePath = QDir::homePath();
     QString packagesPath = homePath+"/packages";
     QDir pkgs = QDir(packagesPath);
     QStringList dirs;
     dirs = pkgs.entryList(QStringList("*"), QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);
     for (int i=0; i<dirs.count(); i++)
     {
         QString fullName = packagesPath+"/"+dirs[i]+layoutFileName;
         if (QFile::exists(fullName))
         {
             return fullName;
         }
     }
     return QString();
}

void KeyboardFrame::LoadSet(QString s)
{
    QSettings settings(s, QSettings::IniFormat);

    settings.beginGroup("Letters");
    QStringList list_lower, list_upper;
    list_lower = settings.value("lower").toStringList();
    list_upper = settings.value("upper").toStringList();
    for (int i=0; i<list_lower.count(); i++)
    {
        QString sl = list_lower[i];
        QString su = list_upper[i];
        int code_sl = sl.toInt();
        int code_su = su.toInt();
        specialM[SPEC_SYMBOL_COUNT + i].qcode = code_sl;
        specialM[SPEC_SYMBOL_COUNT + i].unicode = code_sl;
        shiftMap[SHIFT_SYMBOL_COUNT + i].normal = code_sl;
        shiftMap[SHIFT_SYMBOL_COUNT + i].shifted = code_su;
    }
    settings.endGroup();

    settings.beginGroup("Board");
    QStringList list_rows;
    list_rows = settings.value("keyrows").toStringList();
    for (int i=0; i<list_rows.count(); i++)
    {
        QString row = list_rows[i];
        QStringList keys = row.split("|");
        for (int j=0; j<keys.count(); j++)
        {
            QString key = keys[j];
            QStringList fields = key.split(" ");
            int key_width = fields[0].toInt();
            QString key_chartype = fields[1];
            QString key_char = fields[2];
            int key_code = 0;
            if (key_chartype=="ch") key_code=(int)(key_char.toAscii().data()[0]);
            if (key_chartype=="id") key_code=key_char.toInt();
            if (key_chartype=="ex") key_code=key_char.toInt() + 0x80 + SPEC_SYMBOL_COUNT;
            keyboard_national_6[i][j*2+0] = (uchar)(key_width);
            keyboard_national_6[i][j*2+1] = (uchar)(key_code);
        }
        keyboard_national_6[i][list_rows.count()*2+0] = 0;
        keyboard_national_6[i][list_rows.count()*2+1] = 0;
    }
    settings.endGroup();
}

//====================================================================
//== Skin
//====================================================================

void KeyboardFrame::LoadSkin()
{
    QString baseDir;
    if (QFile::exists(Qtopia::qtopiaDir() + skinFileName))
    {
        baseDir = Qtopia::qtopiaDir();
    }
    else
    {
        baseDir = FindSkinInSandboxes();
    }

    if (!baseDir.isNull())
    {       
        qLog(Input) << "FingerKeyboard - loading skin settings";
        qLog(Input) << baseDir + skinFileName;

        QSettings settings(baseDir + skinFileName, QSettings::IniFormat);
        settings.beginGroup("Skin");
        QString skinName = settings.value("name").toString();
        settings.endGroup();

        LoadSkinPics(baseDir + "/pics/fingerkeyboard/skins/" + skinName + "/");
    }
}

void KeyboardFrame::LoadSkinPics(QString skinPicsPath)
{
    qLog(Input) << "FingerKeyboard - loading skin pixmaps";
    qLog(Input) << skinPicsPath;

    btn_left = new QPixmap(skinPicsPath + "btn-left.png");
    btn_right = new QPixmap(skinPicsPath + "btn-right.png");
    btn_mid = new QPixmap(skinPicsPath + "btn-mid.png");

    btn_pressed_left = new QPixmap(skinPicsPath + "btn-pressed-left.png");
    btn_pressed_right = new QPixmap(skinPicsPath + "btn-pressed-right.png");
    btn_pressed_mid = new QPixmap(skinPicsPath + "btn-pressed-mid.png");

    int specialIndex=0;
    while  (specialM[specialIndex].qcode != 0 )
    {
        QString picName(specialM[specialIndex].picName);
        if (picName.isNull())
        {
            QString label(specialM[specialIndex].label);
            if (label=="Tab") picName = "tab";
            if (label=="Ret") picName = "enter";
            if (label=="Shift") picName = "shift";
            if (label=="Switch") picName = "switch";
        }
        if (!picName.isNull())
        {
            QString pName = skinPicsPath + picName + ".png";
            if (QFile::exists(pName))
            {
                delete specialM[specialIndex].pic;
                specialM[specialIndex].pic = new QPixmap(pName);
            }
        }
        specialIndex++;
    }

    useSkin = true;
}

QString KeyboardFrame::FindSkinInSandboxes()
{
     QString homePath = QDir::homePath();
     QString packagesPath = homePath+"/packages";
     QDir pkgs = QDir(packagesPath);
     QStringList dirs;
     dirs = pkgs.entryList(QStringList("*"), QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);
     for (int i=0; i<dirs.count(); i++)
     {
         QString fullPath = packagesPath+"/"+dirs[i];
         QString fullName = fullPath+skinFileName;
         if (QFile::exists(fullName))
         {
             return fullPath;
         }
     }
     return QString();
}
