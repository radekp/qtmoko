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

#include "JSScreen.h"

#include <wtf/GetPtr.h>

#include "Screen.h"

#include <runtime/JSNumberCell.h>

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSScreen)

/* Hash table */

static const HashTableValue JSScreenTableValues[9] =
{
    { "height", DontDelete|ReadOnly, (intptr_t)jsScreenHeight, (intptr_t)0 },
    { "width", DontDelete|ReadOnly, (intptr_t)jsScreenWidth, (intptr_t)0 },
    { "colorDepth", DontDelete|ReadOnly, (intptr_t)jsScreenColorDepth, (intptr_t)0 },
    { "pixelDepth", DontDelete|ReadOnly, (intptr_t)jsScreenPixelDepth, (intptr_t)0 },
    { "availLeft", DontDelete|ReadOnly, (intptr_t)jsScreenAvailLeft, (intptr_t)0 },
    { "availTop", DontDelete|ReadOnly, (intptr_t)jsScreenAvailTop, (intptr_t)0 },
    { "availHeight", DontDelete|ReadOnly, (intptr_t)jsScreenAvailHeight, (intptr_t)0 },
    { "availWidth", DontDelete|ReadOnly, (intptr_t)jsScreenAvailWidth, (intptr_t)0 },
    { 0, 0, 0, 0 }
};

static const HashTable JSScreenTable =
#if ENABLE(PERFECT_HASH_SIZE)
    { 63, JSScreenTableValues, 0 };
#else
    { 18, 15, JSScreenTableValues, 0 };
#endif

/* Hash table for prototype */

static const HashTableValue JSScreenPrototypeTableValues[1] =
{
    { 0, 0, 0, 0 }
};

static const HashTable JSScreenPrototypeTable =
#if ENABLE(PERFECT_HASH_SIZE)
    { 0, JSScreenPrototypeTableValues, 0 };
#else
    { 1, 0, JSScreenPrototypeTableValues, 0 };
#endif

const ClassInfo JSScreenPrototype::s_info = { "ScreenPrototype", 0, &JSScreenPrototypeTable, 0 };

JSObject* JSScreenPrototype::self(ExecState* exec)
{
    return getDOMPrototype<JSScreen>(exec);
}

const ClassInfo JSScreen::s_info = { "Screen", 0, &JSScreenTable, 0 };

JSScreen::JSScreen(PassRefPtr<Structure> structure, PassRefPtr<Screen> impl)
    : DOMObject(structure)
    , m_impl(impl)
{
}

JSScreen::~JSScreen()
{
    forgetDOMObject(*Heap::heap(this)->globalData(), m_impl.get());

}

JSObject* JSScreen::createPrototype(ExecState* exec)
{
    return new (exec) JSScreenPrototype(JSScreenPrototype::createStructure(exec->lexicalGlobalObject()->objectPrototype()));
}

bool JSScreen::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSScreen, Base>(exec, &JSScreenTable, this, propertyName, slot);
}

JSValuePtr jsScreenHeight(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    Screen* imp = static_cast<Screen*>(static_cast<JSScreen*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->height());
}

JSValuePtr jsScreenWidth(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    Screen* imp = static_cast<Screen*>(static_cast<JSScreen*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->width());
}

JSValuePtr jsScreenColorDepth(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    Screen* imp = static_cast<Screen*>(static_cast<JSScreen*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->colorDepth());
}

JSValuePtr jsScreenPixelDepth(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    Screen* imp = static_cast<Screen*>(static_cast<JSScreen*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->pixelDepth());
}

JSValuePtr jsScreenAvailLeft(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    Screen* imp = static_cast<Screen*>(static_cast<JSScreen*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->availLeft());
}

JSValuePtr jsScreenAvailTop(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    Screen* imp = static_cast<Screen*>(static_cast<JSScreen*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->availTop());
}

JSValuePtr jsScreenAvailHeight(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    Screen* imp = static_cast<Screen*>(static_cast<JSScreen*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->availHeight());
}

JSValuePtr jsScreenAvailWidth(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    Screen* imp = static_cast<Screen*>(static_cast<JSScreen*>(asObject(slot.slotBase()))->impl());
    return jsNumber(exec, imp->availWidth());
}

JSC::JSValuePtr toJS(JSC::ExecState* exec, Screen* object)
{
    return getDOMObjectWrapper<JSScreen>(exec, object);
}
Screen* toScreen(JSC::JSValuePtr value)
{
    return value->isObject(&JSScreen::s_info) ? static_cast<JSScreen*>(asObject(value))->impl() : 0;
}

}
