{
  "targets": [
    {
      "target_name": "icebug",
      "sources": ["src/addon.cpp"],

      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "vendor/include",
        "/opt/homebrew/opt/apache-arrow/include",
        "/opt/homebrew/opt/libomp/include"
      ],

      "libraries": [
        "<(module_root_dir)/vendor/lib/libnetworkit.dylib",
        "-L/opt/homebrew/opt/apache-arrow/lib", "-larrow",
        "-L/opt/homebrew/opt/libomp/lib",       "-lomp"
      ],

      "cflags":    ["-fexceptions", "-frtti"],
      "cflags_cc": [
        "-std=c++20",
        "-fexceptions",
        "-frtti",
        "-Wno-deprecated-declarations"
      ],

      "conditions": [
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS":   "YES",
            "GCC_ENABLE_CPP_RTTI":         "YES",
            "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
            "CLANG_CXX_LIBRARY":           "libc++",
            "MACOSX_DEPLOYMENT_TARGET":    "11.0",
            "OTHER_CPLUSPLUSFLAGS":        ["-Wno-deprecated-declarations", "-frtti"],
            "OTHER_LDFLAGS": [
              "-Wl,-rpath,@loader_path/../../vendor/lib",
              "-Wl,-rpath,/opt/homebrew/opt/apache-arrow/lib",
              "-Wl,-rpath,/opt/homebrew/opt/libomp/lib"
            ]
          }
        }],
        ["OS=='linux'", {
          "cflags_cc": ["-fopenmp"],
          "ldflags":   [
            "-Wl,-rpath,$$ORIGIN/../../vendor/lib",
            "-fopenmp"
          ]
        }]
      ]
    }
  ]
}
