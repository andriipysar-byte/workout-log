import Foundation
import WorkoutLogCore

// Lightweight, XCTest-free verifier so the core can be checked under Command Line
// Tools (no Xcode). Exits non-zero if any check fails.

var failures = 0
func check(_ passed: Bool, _ message: String) {
    print(passed ? "  ok   \(message)" : "  FAIL \(message)")
    if !passed { failures += 1 }
}

// This file is at app/Sources/wl-verify/, so walk up 4 levels to the repo root.
let repoRoot = URL(fileURLWithPath: #filePath)
    .deletingLastPathComponent()
    .deletingLastPathComponent()
    .deletingLastPathComponent()
    .deletingLastPathComponent()
let dataDir = repoRoot.appendingPathComponent("data")

let files = (try? FileManager.default.contentsOfDirectory(
    at: dataDir, includingPropertiesForKeys: nil))?
    .filter { $0.pathExtension == "json" }
    .sorted { $0.lastPathComponent < $1.lastPathComponent } ?? []

print("Round-trip over \(files.count) session file(s) in \(dataDir.path):")
let stubNames = ["2026-07-21_A1", "2026-07-23_A2", "2026-07-26_B1", "2026-07-28_B2",
                 "2026-07-30_C1", "2026-08-02_C2", "2026-08-04_D1", "2026-08-06_D2"]
let present = Set(files.map { $0.deletingPathExtension().lastPathComponent })
check(stubNames.allSatisfy(present.contains), "all 8 generated cycle stubs present")
for f in files {
    do {
        let original = try Data(contentsOf: f)
        let s1 = try SessionCoding.decode(original)
        let enc1 = try SessionCoding.encode(s1)
        let s2 = try SessionCoding.decode(enc1)
        let enc2 = try SessionCoding.encode(s2)
        check(s1 == s2 && enc1 == enc2, "round-trip \(f.lastPathComponent)")
    } catch {
        check(false, "decode \(f.lastPathComponent): \(error)")
    }
}

print("Catalogue (exercises.json):")
do {
    let data = try Data(contentsOf: repoRoot.appendingPathComponent("exercises.json"))
    let cat = try SessionCoding.decodeCatalogue(data)
    check(!cat.exercises.isEmpty, "catalogue non-empty (\(cat.exercises.count) exercises)")
    check(cat.resolve("присід фронтальний")?.pattern == .squat, "canonical name resolves")
    check(cat.resolve("фр. присід")?.name == "присід фронтальний", "alias resolves to canonical")
    check(cat.resolve("невідома вправа") == nil, "unknown name returns nil (warn-not-block)")
    let bench = cat.resolve("жим лежачи")
    check(bench?.primaryMuscles == ["chest", "triceps"], "жим лежачи resolves with primary muscles")
    check(cat.resolve("гирі")?.name == "махи гирею", "metcon alias 'гирі' resolves to махи гирею")
    check(cat.exercises.allSatisfy { !$0.primaryMuscles.isEmpty },
          "every exercise has primary muscles")
    let ungrouped = Set(cat.exercises.flatMap { $0.primaryMuscles + $0.secondaryMuscles })
        .filter { MuscleGroup.of($0) == nil }
    check(ungrouped.isEmpty, "every catalogue muscle maps to a group (ungrouped: \(ungrouped.sorted()))")
    check(MuscleGroup.dominant(in: ["chest": 3, "biceps": 1]) == .chest,
          "dominant group is the highest-scoring region")
    check(cat.resolve("ривок")?.category == .power && cat.resolve("ривок")?.isExplosive == true,
          "category enum decodes (ривок = power)")
    let byCategory = Dictionary(grouping: cat.exercises, by: { $0.category }).mapValues(\.count)
    check((byCategory[.power] ?? 0) == 6 && (byCategory[.speed] ?? 0) == 3,
          "6 power + 3 speed = former explosive set (\(byCategory.map { "\($0.key.rawValue):\($0.value)" }.sorted()))")
    check(cat.exercises.filter(\.isExplosive).count == 9, "9 movements are explosive (power ∪ speed)")
} catch {
    check(false, "catalogue load: \(error)")
}

print("MuscleActivation:")
do {
    let catData = try Data(contentsOf: repoRoot.appendingPathComponent("exercises.json"))
    let catalogue = try SessionCoding.decodeCatalogue(catData)
    let activation = MuscleActivation()

    if let frontSquat = catalogue.resolve("присід фронтальний") {
        let m = activation.forExercise(frontSquat)
        check(m["quads"] == 1.0 && m["glutes"] == 1.0 && m["spinal_erectors"] == 0.5,
              "per-exercise map: front squat quads/glutes=1.0, erectors=0.5")
    } else { check(false, "front squat missing from catalogue") }

    // The three modes surface DIFFERENT hottest muscles: set_count → forearms
    // (8 grip sets vs 5 squat sets); rep_volume/tonnage → the loaded squat pattern.
    let c2 = try SessionStore(folder: dataDir).load(dataDir.appendingPathComponent("2026-06-23_C2.json"))
    var peaks: [WeightingMode: String] = [:]
    for mode in WeightingMode.allCases {
        let map = activation.forSession(c2, catalogue: catalogue, mode: mode)
        check(abs((map.values.max() ?? 0) - 1.0) < 1e-9, "\(mode.rawValue): normalized peak = 1.0")
        peaks[mode] = map.max { $0.value < $1.value }?.key
    }
    check(peaks[.setCount] == "forearms", "set_count peak = forearms (grip-set heavy): \(peaks[.setCount] ?? "—")")
    check(["quads", "glutes"].contains(peaks[.repVolume] ?? ""), "rep_volume peak is squat pattern: \(peaks[.repVolume] ?? "—")")
    check(["quads", "glutes"].contains(peaks[.tonnage] ?? ""), "tonnage peak is squat pattern: \(peaks[.tonnage] ?? "—")")
    check(peaks[.setCount] != peaks[.tonnage], "weighting modes differ (set_count ≠ tonnage)")
} catch {
    check(false, "MuscleActivation: \(error)")
}

print("MuscleMapSVG colorizer:")
do {
    let template = try String(contentsOf: repoRoot.appendingPathComponent("app/Resources/muscle-map.svg"), encoding: .utf8)
    let catData = try Data(contentsOf: repoRoot.appendingPathComponent("exercises.json"))
    let catalogue = try SessionCoding.decodeCatalogue(catData)
    let usedMuscles = Set(catalogue.exercises.flatMap { $0.primaryMuscles + $0.secondaryMuscles })
    let templateMuscles = Set(
        try NSRegularExpression(pattern: #"data-muscle="([a-z_]+)""#)
            .matches(in: template, range: NSRange(template.startIndex..., in: template))
            .map { (template as NSString).substring(with: $0.range(at: 1)) })
    let missing = usedMuscles.subtracting(templateMuscles)
    check(missing.isEmpty, "SVG template covers every catalogue muscle (missing: \(missing.sorted()))")

    let svg = MuscleMapSVG.colorize(template: template, scores: ["quads": 1.0, "chest": 0.4])
    check(svg.contains("data-muscle=\"quads\" style=\"fill:#0d366b\""), "peak muscle (quads=1.0) filled with high colour")
    check(svg.contains("data-muscle=\"forearms\" style=\"fill:#e8e8e3\""), "unworked muscle filled with zero colour")
    check(MuscleMapSVG.color(for: 0, low: "#86b6ef", high: "#0d366b", zero: "#e8e8e3") == "#e8e8e3", "score 0 → zero colour")
    check(MuscleMapSVG.color(for: 1, low: "#86b6ef", high: "#0d366b", zero: "#e8e8e3") == "#0d366b", "score 1 → high colour")
} catch {
    check(false, "MuscleMapSVG: \(error)")
}

print("SessionStore (save → reload):")
do {
    let store = SessionStore(folder: dataDir)
    let urls = try store.listURLs()
    if let first = urls.first {
        let loaded = try store.load(first)
        let tmp = URL(fileURLWithPath: NSTemporaryDirectory())
            .appendingPathComponent("wl-verify-\(loaded.cycleDay)")
        let tmpStore = SessionStore(folder: tmp)
        let written = try tmpStore.save(loaded)
        let reloaded = try tmpStore.load(written)
        check(reloaded == loaded, "save then reload is identical (\(written.lastPathComponent))")
        check(written.lastPathComponent == "\(loaded.date)_\(loaded.cycleDay).json",
              "filename follows YYYY-MM-DD_<cycleDay>.json")
        try? FileManager.default.removeItem(at: tmp)
    } else {
        check(false, "no session files to exercise SessionStore")
    }
} catch {
    check(false, "SessionStore: \(error)")
}

print("Notation parser (docs/03-log-notation.md examples):")

let ramp = Notation.parseStrengthSets("6 × [70, 80, 90, 100, 110]")
check(ramp.sets.count == 5 && ramp.sets.allSatisfy { $0.reps == 6 }
        && ramp.sets.map(\.weightKg) == [70, 80, 90, 100, 110],
      "ramp expands to 5 sets at reps 6")

let backoff = Notation.parseStrengthSets("6 × [70, 80, 90, 100, 110] + 6 × [30]")
check(backoff.sets.count == 6
        && backoff.sets.last?.weightKg == 30 && backoff.sets.last?.isBackoff == true
        && backoff.sets.prefix(5).allSatisfy { $0.isBackoff == nil },
      "trailing group is a back-off set")

let override = Notation.parseStrengthSets("6 × [30, 60, 80(4), 86(2), 90(1)]")
check(override.sets.map(\.reps) == [6, 6, 4, 2, 1],
      "per-set rep overrides applied")

let cluster = Notation.parseStrengthSets("5+5+4+3+3 (20)")
check(cluster.sets.count == 1 && cluster.sets[0].cluster == [5, 5, 4, 3, 3]
        && cluster.sets[0].totalReps == 20,
      "cluster with explicit total")

let cluster2 = Notation.parseStrengthSets("4+4+3+2+2+2+2+1")
check(cluster2.sets.first?.totalReps == 20, "cluster total derived from sum")

let holds = Notation.parseStrengthSets("4 × [54c, 40c, 36c, 42c]")
check(holds.sets.count == 4 && holds.sets.map(\.durationSec) == [54, 40, 36, 42]
        && holds.sets.allSatisfy { $0.reps == nil },
      "timed holds → duration sets, no reps")

let braces = Notation.parseStrengthSets("{60с, 70с, 30с, 35с}")
check(braces.sets.count == 4 && braces.sets.map(\.durationSec) == [60, 70, 30, 35],
      "bare brace list with Cyrillic 'с' suffix")

let compose = Notation.parseStrengthSets("6 × [70, 80]")
let block = StrengthBlock(exercise: "присід фронтальний", sets: compose.sets)
if let data = try? SessionCoding.makeEncoder().encode(
    Session(date: "2026-07-21", cycleDay: "A1", blocks: [.strength(block)])),
   let round = try? SessionCoding.decode(data),
   case let .strength(sb)? = round.blocks.first {
    check(sb.sets.count == 2 && sb.sets[0].weightKg == 70,
          "parsed sets survive a JSON round-trip in a session")
} else {
    check(false, "parsed sets survive a JSON round-trip in a session")
}

print(failures == 0 ? "\n✅ ALL CHECKS PASSED" : "\n❌ \(failures) CHECK(S) FAILED")
exit(failures == 0 ? 0 : 1)
