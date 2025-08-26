#include "gamepad.h"
// Important: our include_dirs points to third_party/hidapi/hidapi
// so we include as a plain header name (not <hidapi/hidapi.h>)
#include "hidapi.h"
#include <cstring>

namespace {
  struct DeviceInfo {
    std::string path;
    uint16_t vid{};
    uint16_t pid{};
    std::wstring manufacturer;
    std::wstring product;
    unsigned short usagePage = 0;
    unsigned short usage = 0;
    int interfaceNumber = -1;
  };

  // Generic Desktop Page: Joystick (0x04), Game Pad (0x05), Multi-axis (0x08)
  bool isGamepadUsage(unsigned short usagePage, unsigned short usage) {
    return usagePage == 0x01 && (usage == 0x04 || usage == 0x05 || usage == 0x08);
  }

  Napi::Object DeviceToJS(Napi::Env env, const DeviceInfo& d) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("path", Napi::String::New(env, d.path));
    o.Set("vendorId", Napi::Number::New(env, d.vid));
    o.Set("productId", Napi::Number::New(env, d.pid));
    o.Set("manufacturer", Napi::String::New(env, std::string(d.manufacturer.begin(), d.manufacturer.end())));
    o.Set("product", Napi::String::New(env, std::string(d.product.begin(), d.product.end())));
    o.Set("usagePage", Napi::Number::New(env, d.usagePage));
    o.Set("usage", Napi::Number::New(env, d.usage));
    o.Set("interfaceNumber", Napi::Number::New(env, d.interfaceNumber));
    return o;
  }
}

Napi::Object Gamepad::Init(Napi::Env env, Napi::Object exports) {
  if (hid_init() != 0) {
    Napi::TypeError::New(env, "hid_init failed").ThrowAsJavaScriptException();
  }

  Napi::Function func = DefineClass(env, "Gamepad", {
    InstanceMethod("start", &Gamepad::Start),
    InstanceMethod("stop", &Gamepad::Stop),
    InstanceMethod("close", &Gamepad::Close),
    InstanceMethod("write", &Gamepad::Write),
    StaticMethod("enumerate", &Gamepad::Enumerate)
  });

  exports.Set("Gamepad", func);
  return exports;
}

Gamepad::Gamepad(const Napi::CallbackInfo& info)
  : Napi::ObjectWrap<Gamepad>(info) {

  Napi::Env env = info.Env();
  if (info.Length() == 1 && info[0].IsObject()) {
    auto cfg = info[0].As<Napi::Object>();
    if (cfg.Has("path")) {
      usePath_ = true;
      path_ = cfg.Get("path").As<Napi::String>().Utf8Value();
    } else if (cfg.Has("vendorId") && cfg.Has("productId")) {
      vid_ = static_cast<uint16_t>(cfg.Get("vendorId").As<Napi::Number>().Uint32Value());
      pid_ = static_cast<uint16_t>(cfg.Get("productId").As<Napi::Number>().Uint32Value());
      usePath_ = false;
    } else {
      Napi::TypeError::New(env, "Pass either {path} or {vendorId, productId}").ThrowAsJavaScriptException();
    }
  } else {
    Napi::TypeError::New(env, "Expected single options object").ThrowAsJavaScriptException();
  }
}

Gamepad::~Gamepad() {
  StopInternal();
  CloseInternal();
}

Napi::Value Gamepad::Enumerate(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Array out = Napi::Array::New(env);

  hid_device_info* cur = hid_enumerate(0x0, 0x0);
  size_t idx = 0;
  for (hid_device_info* d = cur; d; d = d->next) {
    if (isGamepadUsage(d->usage_page, d->usage)) {
      DeviceInfo di;
      di.path = d->path ? d->path : "";
      di.vid = d->vendor_id;
      di.pid = d->product_id;
      if (d->manufacturer_string) di.manufacturer = d->manufacturer_string;
      if (d->product_string) di.product = d->product_string;
      di.usagePage = d->usage_page;
      di.usage = d->usage;
      di.interfaceNumber = d->interface_number;
      out.Set(idx++, DeviceToJS(env, di));
    }
  }
  hid_free_enumeration(cur);

  return out;
}

Napi::Value Gamepad::Start(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() != 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "start(callback) required").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (running_) return Napi::Boolean::New(env, true);

  {
    std::lock_guard<std::mutex> lock(devMutex_);
    if (!dev_) {
      dev_ = usePath_ ? hid_open_path(path_.c_str())
                      : hid_open(vid_, pid_, nullptr);
      if (!dev_) {
        Napi::Error::New(env, "Failed to open HID device").ThrowAsJavaScriptException();
        return env.Null();
      }
      // Blocking read with short sleeps is fine on macOS.
      hid_set_nonblocking(dev_, 0);
    }
  }

  Napi::Function cb = info[0].As<Napi::Function>();
  tsfn_ = Napi::ThreadSafeFunction::New(env, cb, "gamepad-report", 0, 1);

  running_ = true;
  reader_ = std::thread(&Gamepad::readLoop, this);

  return Napi::Boolean::New(env, true);
}

Napi::Value Gamepad::Stop(const Napi::CallbackInfo& info) {
  StopInternal();
  return Napi::Boolean::New(info.Env(), true);
}

Napi::Value Gamepad::Close(const Napi::CallbackInfo& info) {
  CloseInternal();
  return Napi::Boolean::New(info.Env(), true);
}

Napi::Value Gamepad::Write(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() != 1 || !info[0].IsTypedArray()) {
    Napi::TypeError::New(env, "write(Buffer) required").ThrowAsJavaScriptException();
    return env.Null();
  }
  std::lock_guard<std::mutex> lock(devMutex_);
  if (!dev_) {
    Napi::Error::New(env, "Device not open").ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::Uint8Array arr = info[0].As<Napi::Uint8Array>();
  int res = hid_write(dev_, arr.Data(), arr.ByteLength());
  return Napi::Number::New(env, res);
}

void Gamepad::StopInternal() {
  if (running_) {
    running_ = false;
    if (reader_.joinable()) reader_.join();
    if (tsfn_) {
      tsfn_.Release();
      tsfn_ = {};
    }
  }
}

void Gamepad::CloseInternal() {
  std::lock_guard<std::mutex> lock(devMutex_);
  if (dev_) {
    hid_close(dev_);
    dev_ = nullptr;
  }
}

void Gamepad::readLoop() {
  std::vector<unsigned char> buf(64);
  while (running_) {
    int res = 0;
    {
      std::lock_guard<std::mutex> lock(devMutex_);
      if (!dev_) break;
      res = hid_read(dev_, buf.data(), buf.size());
    }
    if (res > 0) {
      auto copy = std::vector<uint8_t>(buf.begin(), buf.begin() + res);
      tsfn_.BlockingCall([copy = std::move(copy)](Napi::Env env, Napi::Function cb){
        auto b = Napi::Buffer<uint8_t>::Copy(env, copy.data(), copy.size());
        cb.Call({ b });
      });
    } else if (res == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } else {
      // error / disconnect
      running_ = false;
      tsfn_.BlockingCall([](Napi::Env env, Napi::Function cb){
        cb.Call({ env.Null() });
      });
      break;
    }
  }
}
