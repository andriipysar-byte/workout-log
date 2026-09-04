import XCTest
@testable import WorkoutLogCore

/// Requires full Xcode (XCTest); the same assertions run under Command Line Tools via `swift run wl-verify`.
final class NotationTests: XCTestCase {

    func testRampExpandsToOneSetPerWeight() {
        let r = Notation.parseStrengthSets("6 × [70, 80, 90, 100, 110]")
        XCTAssertEqual(r.sets.map(\.weightKg), [70, 80, 90, 100, 110])
        XCTAssertTrue(r.sets.allSatisfy { $0.reps == 6 })
        XCTAssertTrue(r.warnings.isEmpty)
    }

    func testBackoffGroup() {
        let r = Notation.parseStrengthSets("6 × [70, 80] + 6 × [30]")
        XCTAssertEqual(r.sets.count, 3)
        XCTAssertEqual(r.sets.last?.weightKg, 30)
        XCTAssertEqual(r.sets.last?.isBackoff, true)
        XCTAssertNil(r.sets.first?.isBackoff)
    }

    func testPerSetRepOverride() {
        let r = Notation.parseStrengthSets("6 × [30, 60, 80(4), 86(2), 90(1)]")
        XCTAssertEqual(r.sets.map(\.reps), [6, 6, 4, 2, 1])
    }

    func testClusterWithAndWithoutTotal() {
        XCTAssertEqual(Notation.parseStrengthSets("5+5+4+3+3 (20)").sets.first?.cluster, [5, 5, 4, 3, 3])
        XCTAssertEqual(Notation.parseStrengthSets("5+5+4+3+3 (20)").sets.first?.totalReps, 20)
        XCTAssertEqual(Notation.parseStrengthSets("5+5+4+3+3").sets.first?.totalReps, 20)
    }

    func testTimedHolds() {
        let r = Notation.parseStrengthSets("4 × [54c, 40c, 36c, 42c]")
        XCTAssertEqual(r.sets.map(\.durationSec), [54, 40, 36, 42])
        XCTAssertTrue(r.sets.allSatisfy { $0.reps == nil })
    }

    func testBareBraceListCyrillicSeconds() {
        let r = Notation.parseStrengthSets("{60с, 70с, 30с, 35с}")
        XCTAssertEqual(r.sets.map(\.durationSec), [60, 70, 30, 35])
    }
}
