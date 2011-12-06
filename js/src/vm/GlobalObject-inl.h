/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey global object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GlobalObject_inl_h___
#define GlobalObject_inl_h___

namespace js {

inline void
GlobalObject::setFlags(int32 flags)
{
    setSlot(FLAGS, Int32Value(flags));
}

inline void
GlobalObject::initFlags(int32 flags)
{
    initSlot(FLAGS, Int32Value(flags));
}

inline void
GlobalObject::setDetailsForKey(JSProtoKey key, JSObject *ctor, JSObject *proto)
{
    HeapValue &ctorVal = getSlotRef(key);
    HeapValue &protoVal = getSlotRef(JSProto_LIMIT + key);
    HeapValue &visibleVal = getSlotRef(2 * JSProto_LIMIT + key);
    JS_ASSERT(ctorVal.isUndefined());
    JS_ASSERT(protoVal.isUndefined());
    JS_ASSERT(visibleVal.isUndefined());
    ctorVal = ObjectValue(*ctor);
    protoVal = ObjectValue(*proto);
    visibleVal = ctorVal;
}

inline void
GlobalObject::setObjectClassDetails(JSFunction *ctor, JSObject *proto)
{
    setDetailsForKey(JSProto_Object, ctor, proto);
}

inline void
GlobalObject::setFunctionClassDetails(JSFunction *ctor, JSObject *proto)
{
    setDetailsForKey(JSProto_Function, ctor, proto);
}

void
GlobalObject::setThrowTypeError(JSFunction *fun)
{
    HeapValue &v = getSlotRef(THROWTYPEERROR);
    JS_ASSERT(v.isUndefined());
    v = ObjectValue(*fun);
}

void
GlobalObject::setOriginalEval(JSObject *evalobj)
{
    HeapValue &v = getSlotRef(EVAL);
    JS_ASSERT(v.isUndefined());
    v = ObjectValue(*evalobj);
}

} // namespace js

#endif