import Foundation

/// Coarse categorical grouping of the fine-grained catalogue muscles, each with a
/// stable colour code. Distinct from the sequential heat-map ramp (`MuscleMapSVG`):
/// this answers "which region" (categorical), not "how hard" (intensity).
public enum MuscleGroup: String, CaseIterable, Equatable {
    case chest, back, shoulders, arms, legs, core

    public var label: String { rawValue.capitalized }

    public var hex: String {
        switch self {
        case .chest:     return "#d1495b"
        case .back:      return "#00798c"
        case .shoulders: return "#edae49"
        case .arms:      return "#8e5ea2"
        case .legs:      return "#30638e"
        case .core:      return "#58a65c"
        }
    }

    public static func of(_ muscle: String) -> MuscleGroup? {
        switch muscle {
        case "chest": return .chest
        case "lats", "rhomboids", "traps", "spinal_erectors": return .back
        case "front_delts", "side_delts", "rear_delts": return .shoulders
        case "biceps", "triceps", "forearms": return .arms
        case "quads", "hamstrings", "glutes", "calves", "adductors", "hip_flexors": return .legs
        case "abs", "obliques": return .core
        default: return nil
        }
    }

    /// The group carrying the most activation across a `[muscle: score]` map.
    public static func dominant(in scores: [String: Double]) -> MuscleGroup? {
        var byGroup: [MuscleGroup: Double] = [:]
        for (muscle, score) in scores {
            guard let g = of(muscle) else { continue }
            byGroup[g, default: 0] += score
        }
        return byGroup.max { $0.value < $1.value }?.key
    }
}
