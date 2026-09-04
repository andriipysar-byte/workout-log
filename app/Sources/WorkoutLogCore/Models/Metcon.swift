import Foundation

public struct MetconBlock: Codable, Equatable {
    public var type = "metcon"
    public var format: MetconFormat?
    public var scheme: [Int]?
    public var exercises: [MetconExercise]
    public var rounds: [MetconRound]?
    public var startTime: String?
    public var endTime: String?
    public var notes: String?

    public enum CodingKeys: String, CodingKey {
        case type, format, scheme, exercises, rounds
        case startTime = "start_time"
        case endTime = "end_time"
        case notes
    }

    public init(format: MetconFormat? = nil, scheme: [Int]? = nil,
                exercises: [MetconExercise] = [], rounds: [MetconRound]? = nil,
                startTime: String? = nil, endTime: String? = nil, notes: String? = nil) {
        self.format = format
        self.scheme = scheme
        self.exercises = exercises
        self.rounds = rounds
        self.startTime = startTime
        self.endTime = endTime
        self.notes = notes
    }
}

public enum MetconFormat: String, Codable, Equatable {
    case forTime = "for_time"
    case amrap, emom, intervals, ladder, chipper
}

public struct MetconExercise: Codable, Equatable {
    public var name: String
    public var weightKg: Double?
    public var repsOverride: [Int]?

    public enum CodingKeys: String, CodingKey {
        case name
        case weightKg = "weight_kg"
        case repsOverride = "reps_override"
    }

    public init(name: String, weightKg: Double? = nil, repsOverride: [Int]? = nil) {
        self.name = name
        self.weightKg = weightKg
        self.repsOverride = repsOverride
    }
}

/// Splits are stored cumulative (as on paper); per-round splits are derived at read time, never stored.
public struct MetconRound: Codable, Equatable {
    public var round: Int
    public var reps: Int?
    public var splitCumulativeSec: Double?
    public var heartRate: Int?

    public enum CodingKeys: String, CodingKey {
        case round, reps
        case splitCumulativeSec = "split_cumulative_sec"
        case heartRate = "heart_rate"
    }

    public init(round: Int, reps: Int? = nil,
                splitCumulativeSec: Double? = nil, heartRate: Int? = nil) {
        self.round = round
        self.reps = reps
        self.splitCumulativeSec = splitCumulativeSec
        self.heartRate = heartRate
    }
}
