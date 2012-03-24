#include "qofonoobject.h"

QOFonoObject::QOFonoObject()
{
}

QDBusArgument &operator<<(QDBusArgument &argument, const QOFonoObject & value)
{
    argument.beginStructure();
    argument << value.object;
    argument << value.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QOFonoObject & value)
{
    argument.beginStructure();
    argument >> value.object;
    argument >> value.properties;
    argument.endStructure();
    return argument;
}