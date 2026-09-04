// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "WorkoutLog2",
    platforms: [.macOS(.v13)],
    products: [
        .library(name: "WorkoutLogCore", targets: ["WorkoutLogCore"]),
        .executable(name: "wl-verify", targets: ["wl-verify"]),
        .executable(name: "wl-map", targets: ["wl-map"]),
        .executable(name: "WorkoutLogApp", targets: ["WorkoutLogApp"])
    ],
    targets: [
        // Domain core — ZERO SwiftUI/AppKit. Parsing, models, IO, validation (ADR-004).
        .target(name: "WorkoutLogCore"),

        // CLI verifier: round-trips every data/*.json and checks the catalogue.
        // Works under Command Line Tools (no Xcode needed): `swift run wl-verify`.
        .executableTarget(name: "wl-verify", dependencies: ["WorkoutLogCore"]),

        // Export a muscle heatmap SVG for a session: `swift run wl-map <file> <mode>`.
        .executableTarget(name: "wl-map", dependencies: ["WorkoutLogCore"]),

        // SwiftUI entry app — pure presentation over WorkoutLogCore (ADR-004).
        // Run: `WORKOUTLOG_DATA=../data swift run WorkoutLogApp`.
        .executableTarget(name: "WorkoutLogApp", dependencies: ["WorkoutLogCore"]),

        // XCTest suite — same checks, richer reporting. Requires full Xcode
        // (XCTest is not in Command Line Tools). Run with `swift test` there.
        .testTarget(name: "WorkoutLogCoreTests", dependencies: ["WorkoutLogCore"])
    ]
)
