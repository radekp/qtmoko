#ifndef QOFONOOBJECT_H
#define QOFONOOBJECT_H

#include <QtDBus>

#if defined(QOFONO_LIBRARY)
#define QOFONO_EXPORT Q_DECL_EXPORT
#else
#define QOFONO_EXPORT Q_DECL_IMPORT
#endif

class QOFONO_EXPORT QOFonoObject
{
public:
    explicit QOFonoObject();
    QDBusObjectPath object;
    QVariantMap properties;
    QOFONO_EXPORT friend QDBusArgument &operator<<(QDBusArgument &argument, const QOFonoObject & value);
    QOFONO_EXPORT friend const QDBusArgument &operator>>(const QDBusArgument &argument, QOFonoObject & value);
};

Q_DECLARE_METATYPE(QOFonoObject)

#endif // QOFONOOBJECT_H
