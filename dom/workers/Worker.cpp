/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "Worker.h"

#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/BindingUtils.h"

#include "EventTarget.h"
#include "RuntimeService.h"
#include "WorkerPrivate.h"

#include "WorkerInlines.h"

#define PROPERTY_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_SHARED)

#define FUNCTION_FLAGS \
  JSPROP_ENUMERATE

USING_WORKERS_NAMESPACE

using namespace mozilla::dom;

namespace {

class Worker
{
  static DOMJSClass sClass;
  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];

  enum
  {
    STRING_onerror = 0,
    STRING_onmessage,

    STRING_COUNT
  };

  static const char* const sEventStrings[STRING_COUNT];

protected:
  enum {
    // The constructor function holds a WorkerPrivate* in its first reserved
    // slot.
    CONSTRUCTOR_SLOT_PARENT = 0
  };

public:
  static JSClass*
  Class()
  {
    return sClass.ToJSClass();
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto,
            bool aMainRuntime)
  {
    JSObject* proto =
      js::InitClassWithReserved(aCx, aObj, aParentProto, Class(), Construct, 0,
                                sProperties, sFunctions, NULL, NULL);
    if (!proto) {
      return NULL;
    }

    if (!aMainRuntime) {
      WorkerPrivate* parent = GetWorkerPrivateFromContext(aCx);
      parent->AssertIsOnWorkerThread();

      JSObject* constructor = JS_GetConstructor(aCx, proto);
      if (!constructor)
        return NULL;
      js::SetFunctionNativeReserved(constructor, CONSTRUCTOR_SLOT_PARENT,
                                    PRIVATE_TO_JSVAL(parent));
    }

    return proto;
  }

  static WorkerPrivate*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName);

protected:
  static JSBool
  ConstructInternal(JSContext* aCx, unsigned aArgc, jsval* aVp,
                    bool aIsChromeWorker, JSClass* aClass)
  {
    if (!aArgc) {
      JS_ReportError(aCx, "Constructor requires at least one argument!");
      return false;
    }

    JSString* scriptURL = JS_ValueToString(aCx, JS_ARGV(aCx, aVp)[0]);
    if (!scriptURL) {
      return false;
    }

    jsval priv = js::GetFunctionNativeReserved(JSVAL_TO_OBJECT(JS_CALLEE(aCx, aVp)),
                                               CONSTRUCTOR_SLOT_PARENT);

    RuntimeService* runtimeService;
    WorkerPrivate* parent;

    if (JSVAL_IS_VOID(priv)) {
      runtimeService = RuntimeService::GetOrCreateService();
      if (!runtimeService) {
        JS_ReportError(aCx, "Failed to create runtime service!");
        return false;
      }
      parent = NULL;
    }
    else {
      runtimeService = RuntimeService::GetService();
      parent = static_cast<WorkerPrivate*>(JSVAL_TO_PRIVATE(priv));
      parent->AssertIsOnWorkerThread();
    }

    JSObject* obj = JS_NewObject(aCx, aClass, nsnull, nsnull);
    if (!obj) {
      return false;
    }

    nsRefPtr<WorkerPrivate> worker =
      WorkerPrivate::Create(aCx, obj, parent, scriptURL, aIsChromeWorker);
    if (!worker) {
      return false;
    }

    // Worker now owned by the JS object.
    NS_ADDREF(worker.get());
    js::SetReservedSlot(obj, DOM_OBJECT_SLOT, PRIVATE_TO_JSVAL(worker));

    if (!runtimeService->RegisterWorker(aCx, worker)) {
      return false;
    }

    // Worker now also owned by its thread.
    NS_ADDREF(worker.get());

    JS_SET_RVAL(aCx, aVp, OBJECT_TO_JSVAL(obj));
    return true;
  }

private:
  // No instance of this class should ever be created so these are explicitly
  // left without an implementation to prevent linking in case someone tries to
  // make one.
  Worker();
  ~Worker();

  static JSBool
  GetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];
    WorkerPrivate* worker = GetInstancePrivate(aCx, aObj, name);
    if (!worker) {
      return !JS_IsExceptionPending(aCx);
    }

    NS_ConvertASCIItoUTF16 nameStr(name + 2);
    nsresult rv = NS_OK;
    JSObject* listener = worker->GetEventListener(nameStr, rv);

    if (NS_FAILED(rv)) {
      JS_ReportError(aCx, "Failed to get listener!");
    }

    *aVp = listener ? OBJECT_TO_JSVAL(listener) : JSVAL_NULL;
    return true;
  }

  static JSBool
  SetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, JSBool aStrict,
                   jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];
    WorkerPrivate* worker = GetInstancePrivate(aCx, aObj, name);
    if (!worker) {
      return !JS_IsExceptionPending(aCx);
    }

    JSObject* listener;
    if (!JS_ValueToObject(aCx, *aVp, &listener)) {
      return false;
    }

    NS_ConvertASCIItoUTF16 nameStr(name + 2);
    nsresult rv = NS_OK;
    worker->SetEventListener(nameStr, listener, rv);

    if (NS_FAILED(rv)) {
      JS_ReportError(aCx, "Failed to set listener!");
      return false;
    }

    return true;
  }

  static JSBool
  Construct(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    return ConstructInternal(aCx, aArgc, aVp, false, Class());
  }

  static void
  Finalize(JSFreeOp* aFop, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    WorkerPrivate* worker = UnwrapDOMObject<WorkerPrivate>(aObj, Class());
    if (worker) {
      worker->_Finalize(aFop);
    }
  }

  static void
  Trace(JSTracer* aTrc, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    WorkerPrivate* worker = UnwrapDOMObject<WorkerPrivate>(aObj, Class());
    if (worker) {
      worker->_Trace(aTrc);
    }
  }

  static JSBool
  Terminate(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    const char*& name = sFunctions[0].name;
    WorkerPrivate* worker = GetInstancePrivate(aCx, obj, name);
    if (!worker) {
      return !JS_IsExceptionPending(aCx);
    }

    return worker->Terminate(aCx);
  }

  static JSBool
  PostMessage(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    const char*& name = sFunctions[1].name;
    WorkerPrivate* worker = GetInstancePrivate(aCx, obj, name);
    if (!worker) {
      return !JS_IsExceptionPending(aCx);
    }

    jsval message;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v", &message)) {
      return false;
    }

    return worker->PostMessage(aCx, message);
  }
};

MOZ_STATIC_ASSERT(prototypes::MaxProtoChainLength == 3,
                  "The MaxProtoChainLength must match our manual DOMJSClasses");

DOMJSClass Worker::sClass = {
  {
    "Worker",
    JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(1) |
    JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize,
    NULL, NULL, NULL, NULL, Trace
  },
  { prototypes::id::EventTarget_workers, prototypes::id::_ID_Count,
    prototypes::id::_ID_Count },
  -1, false, DOM_OBJECT_SLOT
};

JSPropertySpec Worker::sProperties[] = {
  { sEventStrings[STRING_onerror], STRING_onerror, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onmessage], STRING_onmessage, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec Worker::sFunctions[] = {
  JS_FN("terminate", Terminate, 0, FUNCTION_FLAGS),
  JS_FN("postMessage", PostMessage, 1, FUNCTION_FLAGS),
  JS_FS_END
};

const char* const Worker::sEventStrings[STRING_COUNT] = {
  "onerror",
  "onmessage"
};

class ChromeWorker : public Worker
{
  static DOMJSClass sClass;

public:
  static JSClass*
  Class()
  {
    return sClass.ToJSClass();
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto,
            bool aMainRuntime)
  {
    JSObject* proto =
      js::InitClassWithReserved(aCx, aObj, aParentProto, Class(), Construct, 0,
                                NULL, NULL, NULL, NULL);
    if (!proto) {
      return NULL;
    }

    if (!aMainRuntime) {
      WorkerPrivate* parent = GetWorkerPrivateFromContext(aCx);
      parent->AssertIsOnWorkerThread();

      JSObject* constructor = JS_GetConstructor(aCx, proto);
      if (!constructor)
        return NULL;
      js::SetFunctionNativeReserved(constructor, CONSTRUCTOR_SLOT_PARENT,
                                    PRIVATE_TO_JSVAL(parent));
    }

    return proto;
  }

private:
  // No instance of this class should ever be created so these are explicitly
  // left without an implementation to prevent linking in case someone tries to
  // make one.
  ChromeWorker();
  ~ChromeWorker();

  static WorkerPrivate*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    if (aObj) {
      JSClass* classPtr = JS_GetClass(aObj);
      if (classPtr == Class()) {
        return UnwrapDOMObject<WorkerPrivate>(aObj, Class());
      }
    }

    return Worker::GetInstancePrivate(aCx, aObj, aFunctionName);
  }

  static JSBool
  Construct(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    return ConstructInternal(aCx, aArgc, aVp, true, Class());
  }

  static void
  Finalize(JSFreeOp* aFop, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    WorkerPrivate* worker = UnwrapDOMObject<WorkerPrivate>(aObj, Class());
    if (worker) {
      worker->_Finalize(aFop);
    }
  }

  static void
  Trace(JSTracer* aTrc, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    WorkerPrivate* worker = UnwrapDOMObject<WorkerPrivate>(aObj, Class());
    if (worker) {
      worker->_Trace(aTrc);
    }
  }
};

MOZ_STATIC_ASSERT(prototypes::MaxProtoChainLength == 3,
                  "The MaxProtoChainLength must match our manual DOMJSClasses");

DOMJSClass ChromeWorker::sClass = {
  { "ChromeWorker",
    JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(1) |
    JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize,
    NULL, NULL, NULL, NULL, Trace,
  },
  { prototypes::id::EventTarget_workers, prototypes::id::_ID_Count,
    prototypes::id::_ID_Count },
  -1, false, DOM_OBJECT_SLOT
};

WorkerPrivate*
Worker::GetInstancePrivate(JSContext* aCx, JSObject* aObj,
                           const char* aFunctionName)
{
  JSClass* classPtr = JS_GetClass(aObj);
  if (classPtr == Class() || classPtr == ChromeWorker::Class()) {
    return UnwrapDOMObject<WorkerPrivate>(aObj, classPtr);
  }

  JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                       Class()->name, aFunctionName, classPtr->name);
  return NULL;
}

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

namespace worker {

JSObject*
InitClass(JSContext* aCx, JSObject* aGlobal, JSObject* aProto,
          bool aMainRuntime)
{
  return Worker::InitClass(aCx, aGlobal, aProto, aMainRuntime);
}

} // namespace worker

WorkerCrossThreadDispatcher*
GetWorkerCrossThreadDispatcher(JSContext* aCx, jsval aWorker)
{
  if (JSVAL_IS_PRIMITIVE(aWorker)) {
    return NULL;
  }

  WorkerPrivate* w =
      Worker::GetInstancePrivate(aCx, JSVAL_TO_OBJECT(aWorker),
                                 "GetWorkerCrossThreadDispatcher");
  if (!w) {
    return NULL;
  }
  return w->GetCrossThreadDispatcher();
}


namespace chromeworker {

bool
InitClass(JSContext* aCx, JSObject* aGlobal, JSObject* aProto,
          bool aMainRuntime)
{
  return !!ChromeWorker::InitClass(aCx, aGlobal, aProto, aMainRuntime);
}

} // namespace chromeworker

bool
ClassIsWorker(JSClass* aClass)
{
  return Worker::Class() == aClass || ChromeWorker::Class() == aClass;
}

END_WORKERS_NAMESPACE
