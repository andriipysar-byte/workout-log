import XCTest
@testable import WorkoutLogCore

/// Proves decode → encode → decode is model-stable and encoding is idempotent, over every file in data/.
final class RoundTripTests: XCTestCase {

    /// This file is at app/Tests/WorkoutLogCoreTests/, so walk up 4 levels to the repo root.
    private var repoRoot: URL {
        URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
    }

    private var dataDir: URL { repoRoot.appendingPathComponent("data") }

    private func sessionFiles() throws -> [URL] {
        try FileManager.default
            .contentsOfDirectory(at: dataDir, includingPropertiesForKeys: nil)
            .filter { $0.pathExtension == "json" }
            .sorted { $0.lastPathComponent < $1.lastPathComponent }
    }

    func testAllSessionFilesDecode() throws {
        let files = try sessionFiles()
        XCTAssertGreaterThanOrEqual(files.count, 9, "expected the 1 logged session + 8 stub sessions")
        for f in files {
            let data = try Data(contentsOf: f)
            XCTAssertNoThrow(try SessionCoding.decode(data), "decode \(f.lastPathComponent)")
        }
    }

    func testRoundTripIsModelStableAndIdempotent() throws {
        for f in try sessionFiles() {
            let original = try Data(contentsOf: f)
            let s1 = try SessionCoding.decode(original)
            let enc1 = try SessionCoding.encode(s1)
            let s2 = try SessionCoding.decode(enc1)
            XCTAssertEqual(s1, s2, "model round-trip drifted for \(f.lastPathComponent)")
            let enc2 = try SessionCoding.encode(s2)
            XCTAssertEqual(enc1, enc2, "encoding not idempotent for \(f.lastPathComponent)")
        }
    }

    func testCatalogueDecodesAndResolvesAliases() throws {
        let data = try Data(contentsOf: repoRoot.appendingPathComponent("exercises.json"))
        let cat = try SessionCoding.decodeCatalogue(data)
        XCTAssertFalse(cat.exercises.isEmpty)
        XCTAssertEqual(cat.resolve("присід фронтальний")?.pattern, .squat)
        XCTAssertEqual(cat.resolve("фр. присід")?.name, "присід фронтальний")
        XCTAssertNil(cat.resolve("невідома вправа"))
        XCTAssertEqual(cat.resolve("жим лежачи")?.primaryMuscles, ["chest", "triceps"])
        XCTAssertEqual(cat.resolve("гирі")?.name, "махи гирею")
    }
}
