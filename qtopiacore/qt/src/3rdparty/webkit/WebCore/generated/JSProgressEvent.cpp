/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#include "JSProgressEvent.h"

#include <wtf/GetPtr.h>

#include "ProgressEvent.h"

#include <runtime/Error.h>
#include <runtime/JSNumberCell.h>

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSProgressEvent)

/* Hash table */

static const HashTableValue JSProgressEventTableValues[5] =
{
    { "lengthComputable", DontDelete|ReadOnly, (intptr_t)jsProgressEventLengthComputable, (intptr_t)0 },
    { "loaded", DontDelete|ReadOnly, (intptr_t)jsProgressEventLoaded, (intptr_t)0 },
    { "total", DontDelete|ReadOnly, (intptr_t)jsProgressEventTotal, (intptr_t)0 },
    { "constructor", DontEnum|ReadOnly, (intptr_t)jsProgressEventConstructor, (intptr_t)0 },
    { 0, 0, 0, 0 }
};

static const HashTable JSProgressEventTable =
#if ENABLE(PERFECT_HASH_SIZE)
    { 15, JSProgressEventTableValues, 0 };
#else
    { 9, 7, JSProgressEventTableValues, 0 };
#endif

/* Hash table for constructor */

static const HashTableValue JSProgressEventConstructorTableValues[1] =
{
    { 0, 0, 0, 0 }
};

static const HashTable JSProgressEventConstructorTable =
#if ENABLE(PERFECT_HASH_SIZE)
    { 0, JSProgressEventConstructorTableValues, 0 };
#else
    { 1, 0, JSProgressEventConstructorTableValues, 0 };
#endif

class JSProgressEventConstructor : public DOMObject {
public:
    JSProgressEventConstructor(ExecState* exec)
        : DOMObject(JSProgressEventConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
    {
        putDirect(exec->propertyNames().prototype, JSProgressEventPrototype::self(exec), None);
    }
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual const ClassInfo* classInfo() const { return &s_info; }
    static const ClassInfo s_info;

    static PassRefPtr<Structure> createStructure(JSValuePtr proto) 
    { 
        return Structure::create(proto, TypeInfo(ObjectType, ImplementsHasInstance)); 
    }
};

const ClassInfo JSProgressEventConstructor::s_info = { "ProgressEventConstructor", 0, &JSProgressEventConstructorTable, 0 };

bool JSProgressEventConstructor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSProgressEventConstructor, DOMObject>(exec, &JSProgressEventConstructorTable, this, propertyName, slot);
}

/* Hash table for prototype */

static const HashTableValue JSProgressEventPrototypeTableValues[2] =
{
    { "initProgressEvent", DontDelete|Function, (intptr_t)jsProgressEventPrototypeFunctionInitProgressEvent, (intptr_t)6 },
    { 0, 0, 0, 0 }
};

static const HashTable JSProgressEventPrototypeTable =
#if ENABLE(PERFECT_HASH_SIZE)
    { 0, JSProgressEventPrototypeTableValues, 0 };
#else
    { 2, 1, JSProgressEventPrototypeTableValues, 0 };
#endif

const ClassInfo JSProgressEventPrototype::s_info = { "ProgressEventPrototype", 0, &JSProgressEventPrototypeTable, 0 };

JSObject* JSProgressEventPrototype::self(ExecState* exec)
{
    return getDOMPrototype<JSProgressEvent>(exec);
}

bool JSProgressEventPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, &JSProgressEventPrototypeTable, this, propertyName, slot);
}

const ClassInfo JSProgressEvent::s_info = { "ProgressEvent", &JSEvent::s_info, &JSProgressEventTable, 0 };

JSProgressEvent::JSProgressEvent(PassRefPtr<Structure> structure, PassRefPtr<ProgressEvent> impl)
    : JSEvent(structure, impl)
{
}

JSObject* JSProgressEvent::createPrototype(ExecState* exec)
{
    return new (exec) JSProgressEventPrototype(JSProgressEventPrototype::createStructure(JSEventPrototype::self(exec)));
}

bool JSProgressEvent::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSProgressEvent, Base>(exec, &JSProgressEventTable, this, propertyName, slot);
}

JSValuePtr jsProgressEventLengthComputable(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    ProgressEvent* imp = static_cast<ProgressEvent*>(static_cast<JSProgressEvent*>(asObject(slot.slotBase()))->impl());
    return jsBoolean(imp->lengthComputable());
}

JSValuePtr jsProgressEventLoaded(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    ProgressEvent* imp = static_cast<ProgressEvent*>(static_cast<JSProgressEvent*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->loaded());
}

JSValuePtr jsProgressEventTotal(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    ProgressEvent* imp = static_cast<ProgressEvent*>(static_cast<JSProgressEvent*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->total());
}

JSValuePtr jsProgressEventConstructor(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return static_cast<JSProgressEvent*>(asObject(slot.slotBase()))->getConstructor(exec);
}
JSValuePtr JSProgressEvent::getConstructor(ExecState* exec)
{
    return getDOMConstructor<JSProgressEventConstructor>(exec);
}

JSValuePtr jsProgressEventPrototypeFunctionInitProgressEvent(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&JSProgressEvent::s_info))
        return throwError(exec, TypeError);
    JSProgressEvent* castedThisObj = static_cast<JSProgressEvent*>(asObject(thisValue));
    ProgressEvent* imp = static_cast<ProgressEvent*>(castedThisObj->impl());
    const UString& typeArg = args.at(exec, 0)->toString(exec);
    bool canBubbleArg = args.at(exec, 1)->toBoolean(exec);
    bool cancelableArg = args.at(exec, 2)->toBoolean(exec);
    bool lengthComputableArg = args.at(exec, 3)->toBoolean(exec);
    unsigned loadedArg = args.at(exec, 4)->toInt32(exec);
    unsigned totalArg = args.at(exec, 5)->toInt32(exec);

    imp->initProgressEvent(typeArg, canBubbleArg, cancelableArg, lengthComputableArg, loadedArg, totalArg);
    return jsUndefined();
}


}
