import Foundation

/// The three modes deliberately surface different hottest muscles; the UI offers all three.
public enum WeightingMode: String, Codable, CaseIterable, Equatable {
    case setCount = "set_count"     // one point per working set — robust across modalities
    case repVolume = "rep_volume"   // sets × reps — favours high-rep accessory work
    case tonnage                    // sets × reps × weight — 0 for bodyweight/timed work

    public var label: String {
        switch self {
        case .setCount: return "Sets"
        case .repVolume: return "Reps"
        case .tonnage: return "Tonnage"
        }
    }
}

/// Pure domain logic (ADR-004): produces `[muscle: score]` maps a renderer colours,
/// normalized so the most-worked muscle is 1.0.
public struct MuscleActivation {
    public let primaryWeight: Double
    public let secondaryWeight: Double

    public init(primaryWeight: Double = 1.0, secondaryWeight: Double = 0.5) {
        self.primaryWeight = primaryWeight
        self.secondaryWeight = secondaryWeight
    }

    /// Single exercise: primary muscles at 1.0, secondary at 0.5.
    public func forExercise(_ exercise: Exercise) -> [String: Double] {
        var raw: [String: Double] = [:]
        for m in exercise.primaryMuscles { raw[m] = max(raw[m] ?? 0, primaryWeight) }
        for m in exercise.secondaryMuscles { raw[m] = max(raw[m] ?? 0, secondaryWeight) }
        return normalize(raw)
    }

    /// Unknown exercise names are skipped (warn-not-block: they contribute nothing).
    public func forSession(_ session: Session, catalogue: Catalogue, mode: WeightingMode) -> [String: Double] {
        var raw: [String: Double] = [:]
        accumulate(session, into: &raw, catalogue: catalogue, mode: mode)
        return normalize(raw)
    }

    /// Whole-cycle map: raw volumes sum across sessions, normalized once so a busy day
    /// can't drown a light one the way summing per-session (already-normalized) maps would.
    public func forSessions(_ sessions: [Session], catalogue: Catalogue, mode: WeightingMode) -> [String: Double] {
        var raw: [String: Double] = [:]
        for session in sessions { accumulate(session, into: &raw, catalogue: catalogue, mode: mode) }
        return normalize(raw)
    }

    private func accumulate(_ session: Session, into raw: inout [String: Double],
                            catalogue: Catalogue, mode: WeightingMode) {
        func add(_ exercise: Exercise, volume: Double) {
            guard volume > 0 else { return }
            for m in exercise.primaryMuscles { raw[m, default: 0] += volume * primaryWeight }
            for m in exercise.secondaryMuscles { raw[m, default: 0] += volume * secondaryWeight }
        }

        for block in session.blocks {
            switch block {
            case .strength(let s):
                guard let ex = catalogue.resolve(s.exercise) else { continue }
                add(ex, volume: strengthVolume(s.sets, mode: mode))
            case .metcon(let m):
                for me in m.exercises {
                    guard let ex = catalogue.resolve(me.name) else { continue }
                    add(ex, volume: metconVolume(m, exercise: me, mode: mode))
                }
            case .cardio, .cooldown:
                continue
            }
        }
    }

    private func strengthVolume(_ sets: [WorkSet], mode: WeightingMode) -> Double {
        switch mode {
        case .setCount:
            return Double(sets.count)
        case .repVolume:
            return sets.reduce(0) { $0 + Double(reps(of: $1)) }
        case .tonnage:
            return sets.reduce(0) { $0 + Double(reps(of: $1)) * ($1.weightKg ?? 0) }
        }
    }

    private func metconVolume(_ block: MetconBlock, exercise: MetconExercise, mode: WeightingMode) -> Double {
        let scheme = exercise.repsOverride ?? block.scheme ?? []
        let rounds = block.rounds?.count ?? scheme.count
        switch mode {
        case .setCount:
            return Double(max(rounds, scheme.isEmpty ? 1 : scheme.count))
        case .repVolume:
            return Double(scheme.reduce(0, +))
        case .tonnage:
            return Double(scheme.reduce(0, +)) * (exercise.weightKg ?? 0)
        }
    }

    private func reps(of set: WorkSet) -> Int {
        set.reps ?? set.totalReps ?? set.cluster?.reduce(0, +) ?? 0
    }

    private func normalize(_ raw: [String: Double]) -> [String: Double] {
        guard let peak = raw.values.max(), peak > 0 else { return raw }
        return raw.mapValues { $0 / peak }
    }
}
