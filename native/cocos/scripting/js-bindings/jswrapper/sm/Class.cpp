#include "Class.hpp"
#include "Object.hpp"
#include "Utils.hpp"

#ifdef SCRIPT_ENGINE_SM

namespace se {

    // --- Global Lookup for Constructor Functions

    namespace {
//        std::unordered_map<std::string, Class *> __clsMap;
        JSContext* __cx = nullptr;

        bool empty_constructor(JSContext* cx, uint32_t argc, JS::Value* vp)
        {
            assert(false);
            return true;
        }
    }

    Class::Class()
    : _parent(nullptr)
    , _proto(nullptr)
    , _parentProto(nullptr)
    , _ctor(nullptr)
    , _finalizeOp(nullptr)
    {
        memset(&_jsCls, 0, sizeof(_jsCls));
        memset(&_classOps, 0, sizeof(_classOps));
    }

    Class::~Class()
    {
        SAFE_RELEASE(_parent);
        SAFE_RELEASE(_proto);
        SAFE_RELEASE(_parentProto);
    }

    Class* Class::create(const char* className, Object* obj, Object* parentProto, JSNative ctor)
    {
        Class* cls = new Class();
        if (cls != nullptr && !cls->init(className, obj, parentProto, ctor))
        {
            delete cls;
            cls = nullptr;
        }
        return cls;
    }

    bool Class::init(const char* clsName, Object* parent, Object *parentProto, JSNative ctor)
    {
        _name = clsName;
        _parent = parent;
        if (_parent != nullptr)
            _parent->addRef();
        _parentProto = parentProto;

        if (_parentProto != nullptr)
            _parentProto->addRef();

        _ctor = ctor;
        if (_ctor == nullptr)
        {
            _ctor = empty_constructor;
            LOGD("( %s ) has empty constructor!\n", clsName);
        }

        return true;
    }

    bool Class::install()
    {
//        assert(__clsMap.find(_name) == __clsMap.end());
//
//        __clsMap.emplace(_name, this);

        _jsCls.name = _name;
        if (_finalizeOp != nullptr)
        {
            _jsCls.flags = JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE; //FIXME: Use JSCLASS_BACKGROUND_FINALIZE to improve GC performance
            _classOps.finalize = _finalizeOp;
        }
        else
        {
            _jsCls.flags = JSCLASS_HAS_PRIVATE;
        }

        _jsCls.cOps = &_classOps;

        JSObject* parentObj = _parentProto != nullptr ? _parentProto->_getJSObject() : nullptr;
        JS::RootedObject parent(__cx, _parent->_getJSObject());
        JS::RootedObject parentProto(__cx, parentObj);

        _funcs.push_back(JS_FS_END);
        _properties.push_back(JS_PS_END);
        _staticFuncs.push_back(JS_FS_END);
        _staticProperties.push_back(JS_PS_END);

        JS::RootedObject jsobj(__cx,
                               JS_InitClass(__cx, parent, parentProto, &_jsCls,
                                            _ctor, 0,
                                            _properties.data(), _funcs.data(),
                                            _staticProperties.data(), _staticFuncs.data())
                               );

        if (jsobj)
        {
            _proto = Object::_createJSObject(nullptr, jsobj, true);
            return true;
        }

        return false;
    }

    bool Class::defineFunction(const char *name, JSNative func)
    {
        JSFunctionSpec cb = JS_FN(name, func, 0, JSPROP_PERMANENT | JSPROP_ENUMERATE);
        _funcs.push_back(cb);
        return true;
    }

    bool Class::defineProperty(const char *name, JSNative getter, JSNative setter)
    {
        JSPropertySpec property = JS_PSGS(name, getter, setter, JSPROP_ENUMERATE | JSPROP_PERMANENT);
        _properties.push_back(property);
        return true;
    }

    bool Class::defineStaticFunction(const char *name, JSNative func)
    {
        JSFunctionSpec cb = JS_FN(name, func, 0, JSPROP_PERMANENT | JSPROP_ENUMERATE);
        _staticFuncs.push_back(cb);
        return true;
    }

    bool Class::defineStaticProperty(const char *name, JSNative getter, JSNative setter)
    {
        JSPropertySpec property = JS_PSGS(name, getter, setter, JSPROP_ENUMERATE | JSPROP_PERMANENT);
        _staticProperties.push_back(property);
        return true;
    }

    bool Class::defineFinalizedFunction(JSFinalizeOp func)
    {
        _finalizeOp = func;
        return true;
    }

//    JSObject* Class::_createJSObject(const std::string &clsName, Class** outCls)
//    {
//        auto iter = __clsMap.find(clsName);
//        if (iter == __clsMap.end())
//        {
//            *outCls = nullptr;
//            return nullptr;
//        }
//
//        Class* thiz = iter->second;
//        JS::RootedObject obj(__cx, _createJSObjectWithClass(thiz));
//        *outCls = thiz;
//        return obj;
//    }

    JSObject* Class::_createJSObjectWithClass(Class* cls)
    {
        JSObject* proto = cls->_proto != nullptr ? cls->_proto->_getJSObject() : nullptr;
        JS::RootedObject jsProto(__cx, proto);
        JS::RootedObject obj(__cx, JS_NewObjectWithGivenProto(__cx, &cls->_jsCls, jsProto));
        return obj;
    }

    void Class::setContext(JSContext* cx)
    {
        __cx = cx;
    }

    Object *Class::getProto()
    {
        return _proto;
    }

    JSFinalizeOp Class::_getFinalizeCb() const
    {
        return _finalizeOp;
    }

    void Class::cleanup()
    {// TODO:

    }

} // namespace se {

#endif // SCRIPT_ENGINE_SM
