#include "subdevil.h"
#include "utils.h"

#include <v8.h>
#include <node.h>

// Throws a JS error and returns from the current function
#define THROW_AND_RETURN(isolate, msg)                                  \
  do {                                                                  \
    isolate->ThrowException(v8::Exception::TypeError(                   \
                               v8::String::NewFromUtf8(isolate, msg))); \
    return;                                                             \
  }                                                                     \
  while(0)

namespace Subdevil
{
  namespace NodeJS
  {
    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Handle;
    using v8::Persistent;
    using v8::Exception;

    using v8::String;
    using v8::Number;
    using v8::Boolean;
    using v8::Object;
    using v8::Array;
    using v8::Value;
    using v8::Null;
    using v8::Undefined;


    static Local<Object> USBDrive_to_Object(Isolate *isolate, Subdevil::USBDevicePtr usbDrive)
    {
      Local<Object> obj = Object::New(isolate);

#define OBJ_ATTR_STR(name, val)                                       \
      do {                                                            \
        Local<String> _name = String::NewFromUtf8(isolate, name);     \
        if (val.size() > 0) {                                         \
          obj->Set(_name, String::NewFromUtf8(isolate, val.c_str())); \
        }                                                             \
        else {                                                        \
          obj->Set(_name, Null(isolate));                             \
        }                                                             \
      }                                                               \
      while (0)

#define OBJ_ATTR_NUMBER(name, val)                                      \
      do {                                                              \
        Local<String> _name = String::NewFromUtf8(isolate, name);       \
        obj->Set(_name, Number::New(isolate, static_cast<double>(val))); \
      }                                                                 \
      while(0)

      OBJ_ATTR_STR("id", usbDrive->uid);
      OBJ_ATTR_NUMBER("productId", usbDrive->productID);
      OBJ_ATTR_NUMBER("vendorId", usbDrive->vendorID);
      OBJ_ATTR_STR("product", usbDrive->product);
      OBJ_ATTR_STR("serialNumber", usbDrive->serialNumber);
      OBJ_ATTR_STR("manufacturer", usbDrive->vendor);
      OBJ_ATTR_STR("mount", usbDrive->mountPoint);

#undef OBJ_ATTR_STR
      return obj;
    }

    void Unmount(const FunctionCallbackInfo<Value> &info)
    {
      auto isolate = info.GetIsolate();

      if(info.Length() < 1)
        THROW_AND_RETURN(isolate, "Wrong number of arguments");

      if(!info[0]->IsString())
        THROW_AND_RETURN(isolate, "Expected the first argument to by of type string");

      String::Utf8Value utf8_string(info[0]->ToString());
      Local<Boolean> ret;

      if(Subdevil::unmount(*utf8_string)) {
        ret = Boolean::New(isolate, true);
      } else {
        ret = Boolean::New(isolate, false);
      }

      info.GetReturnValue().Set(ret);
    }

    void GetDevice(const FunctionCallbackInfo<Value> &info)
    {
      auto isolate = info.GetIsolate();

      if(info.Length() < 1)
        THROW_AND_RETURN(isolate, "Wrong number of arguments");

      if(!info[0]->IsString())
        THROW_AND_RETURN(isolate, "Expected the first argument to be of type string");

      String::Utf8Value str(info[0]->ToString());

      auto usbDrive = Subdevil::getDevice(*str);

      if(usbDrive == NULL) {
        info.GetReturnValue().SetNull();
      } else {
        info.GetReturnValue().Set(USBDrive_to_Object(isolate, usbDrive));
      }
    }

    void PollDevices(const FunctionCallbackInfo<Value> &info)
    {
      auto isolate = info.GetIsolate();
      auto devices = Subdevil::getDevices();

      Handle<Array> array = Array::New(isolate, static_cast<int>(devices.size()));

      if(array.IsEmpty())
        THROW_AND_RETURN(isolate, "Array creation failed");

      for(size_t i = 0; i < devices.size(); ++i) {
        auto device_obj = USBDrive_to_Object(isolate, devices[i]);

        array->Set((int)i, device_obj);
      }

      info.GetReturnValue().Set(array);
    }

    void SetLogFile(const FunctionCallbackInfo<Value> &info)
    {
      auto isolate = info.GetIsolate();

      if(info.Length() < 1)
        THROW_AND_RETURN(isolate, "Wrong number of arguments");

      if(!info[0]->IsString())
        THROW_AND_RETURN(isolate, "Expected the first argument to be of type string");

      String::Utf8Value str(info[0]->ToString());

      Logger::instance().setLogFile(*str);

      info.GetReturnValue().Set(Undefined(isolate));
    }

    void Init(Handle<Object> exports)
    {
      Logger::instance().setLogFile("usb-driver.log");

      NODE_SET_METHOD(exports, "setLogFile", SetLogFile);
      NODE_SET_METHOD(exports, "unmount", Unmount);
      NODE_SET_METHOD(exports, "get", GetDevice);
      NODE_SET_METHOD(exports, "poll", PollDevices);
    }
  }  // namespace NodeJS
} // namepsace Subdevil

NODE_MODULE(usb_driver, Subdevil::NodeJS::Init)

