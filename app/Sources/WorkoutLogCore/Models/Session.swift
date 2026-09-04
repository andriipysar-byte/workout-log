import Foundation

/// Files on disk are the source of truth (ADR-001); this is a lossless projection.
public struct Session: Codable, Equatable {
    public var date: String
    public var cycleDay: String
    public var startTime: String?
    public var kind: Kind
    public var bodyweightKg: Double?
    public var notes: String?
    public var blocks: [Block]

    public enum CodingKeys: String, CodingKey {
        case date
        case cycleDay = "cycle_day"
        case startTime = "start_time"
        case kind
        case bodyweightKg = "bodyweight_kg"
        case notes
        case blocks
    }

    public init(
        date: String,
        cycleDay: String,
        startTime: String? = nil,
        kind: Kind = .training,
        bodyweightKg: Double? = nil,
        notes: String? = nil,
        blocks: [Block] = []
    ) {
        self.date = date
        self.cycleDay = cycleDay
        self.startTime = startTime
        self.kind = kind
        self.bodyweightKg = bodyweightKg
        self.notes = notes
        self.blocks = blocks
    }

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        date = try c.decode(String.self, forKey: .date)
        cycleDay = try c.decode(String.self, forKey: .cycleDay)
        startTime = try c.decodeIfPresent(String.self, forKey: .startTime)
        kind = try c.decodeIfPresent(Kind.self, forKey: .kind) ?? .training
        bodyweightKg = try c.decodeIfPresent(Double.self, forKey: .bodyweightKg)
        notes = try c.decodeIfPresent(String.self, forKey: .notes)
        blocks = try c.decode([Block].self, forKey: .blocks)
    }
}

public enum Kind: String, Codable, Equatable {
    case training, deload, retest
}
