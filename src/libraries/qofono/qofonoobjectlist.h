#ifndef QOFONOOBJECTLIST_H
#define QOFONOOBJECTLIST_H

#include <QtDBus>
#include <qofonoobject.h>

#if defined(QOFONO_LIBRARY)
#define QOFONO_EXPORT Q_DECL_EXPORT
#else
#define QOFONO_EXPORT Q_DECL_IMPORT
#endif

typedef QList<QOFonoObject> QOFonoObjectList;
Q_DECLARE_METATYPE(QOFonoObjectList)

#endif // QOFONOOBJECTLIST_H
