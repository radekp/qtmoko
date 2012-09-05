#ifdef QTOPIA

#include "neocontrol.h"
#include <qtopiaapplication.h>
QTOPIA_ADD_APPLICATION(QTOPIA_TARGET,NeoControl)
QTOPIA_MAIN

#else

#include <QtGui/QApplication>
#include "neocontrol.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NeoControl w;
    w.show();
    return a.exec();
}


#endif
