{
  "targets": [
    {
      "target_name": "gamepad",
      "sources": [
        "src/addon.cc",
        "src/gamepad.cc",

        # Compile exactly one hidapi backend based on OS:
        "<!(node -p \"process.platform==='darwin' ? 'third_party/hidapi/mac/hid.c' : ''\")",
        "<!(node -p \"process.platform==='win32' ? 'third_party/hidapi/windows/hid.c' : ''\")",
        "<!(node -p \"process.platform==='linux' ? 'third_party/hidapi/linux/hid.c' : ''\")"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "third_party/hidapi/hidapi"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],

      "conditions": [
        # -------- macOS --------
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "OTHER_LDFLAGS": [
              "-framework", "IOKit",
              "-framework", "CoreFoundation"
            ],
            "MACOSX_DEPLOYMENT_TARGET": "11.0"
          }
        }],

        # -------- Windows --------
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": { "ExceptionHandling": 1 }
          },
          "defines": [ "UNICODE", "_UNICODE" ],
          "libraries": [ "setupapi.lib" ]   # required by hidapi/windows
        }],

        # -------- Linux --------
        ["OS=='linux'", {
          "cflags_cc": [ "-std=c++17" ],
          "libraries": [
            "-ludev",     # device enumeration
            "-lpthread"   # used internally by some builds
          ],
          "defines": [ "HIDAPI_HIDRAW" ]  # select hidraw backend
        }]
      ]
    }
  ]
}
