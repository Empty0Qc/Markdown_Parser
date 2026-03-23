{
  "targets": [
    {
      "target_name": "mk_parser_native",
      "sources": [
        "mk_napi.c",
        "../../src/arena.c",
        "../../src/ast.c",
        "../../src/parser.c",
        "../../src/block.c",
        "../../src/inline_parser.c",
        "../../src/plugin.c",
        "../../src/getters.c"
      ],
      "include_dirs": [
        "../../include",
        "../../src"
      ],
      "cflags": [ "-std=c11", "-Wall", "-Wextra" ],
      "xcode_settings": {
        "OTHER_CFLAGS": [ "-std=c11", "-Wall", "-Wextra" ]
      }
    }
  ]
}
