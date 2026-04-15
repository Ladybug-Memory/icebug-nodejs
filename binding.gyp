{
  "targets": [
    {
      "target_name": "icebug",
      "sources": ["src/addon.cpp"],

      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "vendor/include"
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
          "include_dirs": [
            "/opt/homebrew/opt/apache-arrow/include",
            "/opt/homebrew/opt/libomp/include"
          ],
          "libraries": [
            "-L<(module_root_dir)/vendor/lib", "-lnetworkit",
            "-L/opt/homebrew/opt/apache-arrow/lib", "-larrow",
            "-L/opt/homebrew/opt/libomp/lib",       "-lomp"
          ],
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
          "libraries": [
            "-L<(module_root_dir)/vendor/lib", "-lnetworkit",
            "-larrow"
          ],
          "cflags_cc": ["-fopenmp"],
          "ldflags": [
            "-Wl,-rpath,$$ORIGIN/../../vendor/lib",
            "-fopenmp"
          ]
        }],

        ["OS=='win'", {
          "libraries": [
            "<(module_root_dir)/vendor/lib/networkit.lib",
            "<(module_root_dir)/vendor/lib/networkit/networkit_state.lib",
            "<(module_root_dir)/vendor/lib/tlx.lib",
            "arrow.lib"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "RuntimeLibrary": 2,  # 2 = /MD (MD_DynamicRelease), matching prebuilt networkit.lib
              "AdditionalOptions": ["/std:c++20", "/openmp", "/W0"]
            }
          }
        }]
      ]
    }
  ]
}
