import Foundation
import WorkoutLogCore

// Export a muscle heatmap SVG for a session.
// Usage: swift run wl-map <session.json> [set_count|rep_volume|tonnage] [out.svg]

let args = CommandLine.arguments
guard args.count >= 2 else {
    FileHandle.standardError.write(Data("usage: wl-map <session.json> [set_count|rep_volume|tonnage] [out.svg]\n".utf8))
    exit(2)
}

let sessionPath = URL(fileURLWithPath: args[1])
let mode = WeightingMode(rawValue: args.count >= 3 ? args[2] : "set_count") ?? .setCount

// Repo root = up four levels from this file (…/app/Sources/wl-map/main.swift → repo).
let repoRoot = URL(fileURLWithPath: #filePath)
    .deletingLastPathComponent().deletingLastPathComponent()
    .deletingLastPathComponent().deletingLastPathComponent()
let templateURL = repoRoot.appendingPathComponent("app/Resources/muscle-map.svg")
let catalogueURL = repoRoot.appendingPathComponent("exercises.json")

let outPath = args.count >= 4
    ? URL(fileURLWithPath: args[3])
    : sessionPath.deletingPathExtension().appendingPathExtension("\(mode.rawValue).svg")

do {
    let catalogue = try SessionCoding.decodeCatalogue(try Data(contentsOf: catalogueURL))
    let session = try SessionCoding.decode(try Data(contentsOf: sessionPath))
    let template = try String(contentsOf: templateURL, encoding: .utf8)

    let scores = MuscleActivation().forSession(session, catalogue: catalogue, mode: mode)
    let svg = MuscleMapSVG.colorize(template: template, scores: scores)
    try svg.write(to: outPath, atomically: true, encoding: .utf8)

    let ranked = scores.sorted { $0.value > $1.value }.prefix(5)
        .map { "\($0.key) \(String(format: "%.2f", $0.value))" }.joined(separator: ", ")
    print("wrote \(outPath.lastPathComponent)  [\(mode.rawValue)]  top: \(ranked)")
} catch {
    FileHandle.standardError.write(Data("error: \(error)\n".utf8))
    exit(1)
}
