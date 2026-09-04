import Foundation

/// All fields optional so a weightless / bodyweight / timed set round-trips
/// without inventing values. Named `WorkSet` to avoid clashing with Swift's `Set`.
public struct WorkSet: Codable, Equatable {
    public var weightKg: Double?
    public var reps: Int?
    public var durationSec: Double?
    public var cluster: [Int]?
    public var totalReps: Int?
    public var rir: Double?
    public var repBand: RepBand?
    public var barSpeed: BarSpeed?
    public var isBackoff: Bool?
    public var plannedReps: Int?
    public var notes: String?

    public enum CodingKeys: String, CodingKey {
        case weightKg = "weight_kg"
        case reps
        case durationSec = "duration_sec"
        case cluster
        case totalReps = "total_reps"
        case rir
        case repBand = "rep_band"
        case barSpeed = "bar_speed"
        case isBackoff = "is_backoff"
        case plannedReps = "planned_reps"
        case notes
    }

    public init(weightKg: Double? = nil, reps: Int? = nil, durationSec: Double? = nil,
                cluster: [Int]? = nil, totalReps: Int? = nil, rir: Double? = nil,
                repBand: RepBand? = nil, barSpeed: BarSpeed? = nil, isBackoff: Bool? = nil,
                plannedReps: Int? = nil, notes: String? = nil) {
        self.weightKg = weightKg
        self.reps = reps
        self.durationSec = durationSec
        self.cluster = cluster
        self.totalReps = totalReps
        self.rir = rir
        self.repBand = repBand
        self.barSpeed = barSpeed
        self.isBackoff = isBackoff
        self.plannedReps = plannedReps
        self.notes = notes
    }
}

public enum RepBand: String, Codable, Equatable {
    case heavy   // ≤3
    case base    // 4–6
    case volume  // 7+
}

public enum BarSpeed: String, Codable, Equatable {
    case fast, ok, slow, grind
}
