// swift-tools-version: 5.9
// Package.swift — Swift Package Manager config for MkParser (M8d)
//
// Consumers add this as a dependency:
//   .package(url: "…/mk_p", from: "0.1.0")
// Then:
//   import MkParser

import PackageDescription

let package = Package(
    name: "MkParser",
    platforms: [
        .iOS(.v14),
        .macOS(.v12),
        .tvOS(.v14),
        .watchOS(.v7),
    ],
    products: [
        .library(
            name: "MkParser",
            targets: ["MkParser"]
        ),
    ],
    targets: [
        // C core + getters, compiled as a C target
        .target(
            name: "mk_parser_c",
            path: ".",
            exclude: [
                "bindings",
                "build",
                "cmake",
                "demo",
                "docs",
                "tests",
            ],
            sources: [
                "src/arena.c",
                "src/ast.c",
                "src/parser.c",
                "src/block.c",
                "src/inline_parser.c",
                "src/plugin.c",
                "src/getters.c",
            ],
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("src"),
                .define("_XOPEN_SOURCE", to: "700"),
            ]
        ),

        // Swift wrapper
        .target(
            name: "MkParser",
            dependencies: ["mk_parser_c"],
            path: "bindings/ios",
            sources: ["MkParser.swift"],
            swiftSettings: [
                .define("SWIFT_PACKAGE"),
            ]
        ),

        // Tests
        .testTarget(
            name: "MkParserTests",
            dependencies: ["MkParser"],
            path: "bindings/ios/Tests"
        ),
    ]
)
