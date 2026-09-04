import Foundation

/// Flat tagged-union coded manually: peek `type`, then decode the matching struct
/// from the *same* decoder (Swift's default enum coding would nest instead).
public enum Block: Codable, Equatable {
    case cardio(CardioBlock)
    case strength(StrengthBlock)
    case metcon(MetconBlock)
    case cooldown(CooldownBlock)

    private enum TypeKey: String, CodingKey { case type }

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: TypeKey.self)
        let type = try c.decode(String.self, forKey: .type)
        switch type {
        case "cardio": self = .cardio(try CardioBlock(from: decoder))
        case "strength": self = .strength(try StrengthBlock(from: decoder))
        case "metcon": self = .metcon(try MetconBlock(from: decoder))
        case "cooldown": self = .cooldown(try CooldownBlock(from: decoder))
        default:
            throw DecodingError.dataCorrupted(
                .init(codingPath: decoder.codingPath,
                      debugDescription: "Unknown block type \"\(type)\"")
            )
        }
    }

    public func encode(to encoder: Encoder) throws {
        switch self {
        case .cardio(let b): try b.encode(to: encoder)
        case .strength(let b): try b.encode(to: encoder)
        case .metcon(let b): try b.encode(to: encoder)
        case .cooldown(let b): try b.encode(to: encoder)
        }
    }
}

/// `end_time` (on every block) is the bracket timestamp from the paper log — the
/// source of block duration and session density.
public struct CardioBlock: Codable, Equatable {
    public var type = "cardio"
    public var machine: String
    public var durationMin: Double?
    public var distanceM: Double?
    public var endTime: String?

    public enum CodingKeys: String, CodingKey {
        case type, machine
        case durationMin = "duration_min"
        case distanceM = "distance_m"
        case endTime = "end_time"
    }

    public init(machine: String, durationMin: Double? = nil,
                distanceM: Double? = nil, endTime: String? = nil) {
        self.machine = machine
        self.durationMin = durationMin
        self.distanceM = distanceM
        self.endTime = endTime
    }
}

public struct StrengthBlock: Codable, Equatable {
    public var type = "strength"
    public var exercise: String
    public var sets: [WorkSet]
    public var startTime: String?
    public var endTime: String?
    public var notes: String?

    public enum CodingKeys: String, CodingKey {
        case type, exercise, sets
        case startTime = "start_time"
        case endTime = "end_time"
        case notes
    }

    public init(exercise: String, sets: [WorkSet] = [], startTime: String? = nil,
                endTime: String? = nil, notes: String? = nil) {
        self.exercise = exercise
        self.sets = sets
        self.startTime = startTime
        self.endTime = endTime
        self.notes = notes
    }
}

public struct CooldownBlock: Codable, Equatable {
    public var type = "cooldown"
    public var endTime: String?
    public var notes: String?

    public enum CodingKeys: String, CodingKey {
        case type
        case endTime = "end_time"
        case notes
    }

    public init(endTime: String? = nil, notes: String? = nil) {
        self.endTime = endTime
        self.notes = notes
    }
}
