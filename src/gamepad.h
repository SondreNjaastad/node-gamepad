#pragma once
#include <napi.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <mutex>

struct hid_device_;
typedef struct hid_device_ hid_device;

class Gamepad : public Napi::ObjectWrap<Gamepad> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);

  Gamepad(const Napi::CallbackInfo& info);
  ~Gamepad();

private:
  // JS API
  Napi::Value Start(const Napi::CallbackInfo& info);
  Napi::Value Stop(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);
  Napi::Value Write(const Napi::CallbackInfo& info);
  static Napi::Value Enumerate(const Napi::CallbackInfo& info);

  // worker
  void readLoop();

  // internal helpers (usable from destructor)
  void StopInternal();
  void CloseInternal();

  // state
  hid_device* dev_ = nullptr;
  std::atomic<bool> running_{false};
  std::thread reader_;
  Napi::ThreadSafeFunction tsfn_;
  std::mutex devMutex_;

  // config
  std::string path_;
  uint16_t vid_ = 0;
  uint16_t pid_ = 0;
  bool usePath_ = false;
};
