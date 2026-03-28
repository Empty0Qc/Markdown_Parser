// swift-tools-version: 5.9
// Package.swift — MkParser Demo App (iOS 16+)
//
// References the root MkParser package as a local dependency.
// Build in Xcode: File → Open → demo/ios/Package.swift
// Or: swift run MkParserDemo  (macOS Catalyst only)

import PackageDescription

let package = Package(
    name: "MkParserDemo",
    platforms: [
        .iOS(.v16),
        .macOS(.v13),
    ],
    dependencies: [
        // Reference the root mk_parser Swift package
        .package(path: "../.."),
    ],
    targets: [
        .executableTarget(
            name: "MkParserDemo",
            dependencies: [
                .product(name: "MkParser", package: "MkParser"),
            ],
            path: "App",
            swiftSettings: [
                .define("SWIFT_PACKAGE"),
            ]
        ),
    ]
)
