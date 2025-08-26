#include <napi.h>
#include "gamepad.h"

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  return Gamepad::Init(env, exports);
}

NODE_API_MODULE(gamepad, InitAll)
