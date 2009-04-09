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

#ifndef JSHTMLTableCellElement_h
#define JSHTMLTableCellElement_h

#include "JSHTMLElement.h"

namespace WebCore {

class HTMLTableCellElement;

class JSHTMLTableCellElement : public JSHTMLElement {
    typedef JSHTMLElement Base;
public:
    JSHTMLTableCellElement(PassRefPtr<JSC::Structure>, PassRefPtr<HTMLTableCellElement>);
    static JSC::JSObject* createPrototype(JSC::ExecState*);
    virtual bool getOwnPropertySlot(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::PropertySlot&);
    virtual void put(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::JSValuePtr, JSC::PutPropertySlot&);
    virtual const JSC::ClassInfo* classInfo() const { return &s_info; }
    static const JSC::ClassInfo s_info;

    static PassRefPtr<JSC::Structure> createStructure(JSC::JSValuePtr prototype)
    {
        return JSC::Structure::create(prototype, JSC::TypeInfo(JSC::ObjectType));
    }

    static JSC::JSValuePtr getConstructor(JSC::ExecState*);
};


class JSHTMLTableCellElementPrototype : public JSC::JSObject {
public:
    static JSC::JSObject* self(JSC::ExecState*);
    virtual const JSC::ClassInfo* classInfo() const { return &s_info; }
    static const JSC::ClassInfo s_info;
    JSHTMLTableCellElementPrototype(PassRefPtr<JSC::Structure> structure) : JSC::JSObject(structure) { }
};

// Attributes

JSC::JSValuePtr jsHTMLTableCellElementCellIndex(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
JSC::JSValuePtr jsHTMLTableCellElementAbbr(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementAbbr(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementAlign(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementAlign(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementAxis(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementAxis(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementBgColor(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementBgColor(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementCh(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementCh(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementChOff(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementChOff(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementColSpan(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementColSpan(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementHeaders(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementHeaders(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementHeight(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementHeight(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementNoWrap(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementNoWrap(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementRowSpan(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementRowSpan(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementScope(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementScope(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementVAlign(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementVAlign(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementWidth(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
void setJSHTMLTableCellElementWidth(JSC::ExecState*, JSC::JSObject*, JSC::JSValuePtr);
JSC::JSValuePtr jsHTMLTableCellElementConstructor(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);

} // namespace WebCore

#endif
