import Foundation

/// `pattern` groups variants of one movement so conjugate rotation reads as
/// progress rather than scatter (P8).
public struct Exercise: Codable, Equatable {
    public var name: String            // canonical, Ukrainian
    public var aliases: [String]
    public var pattern: Pattern?
    public var modality: Modality?
    public var category: ExerciseCategory
    public var primaryMuscles: [String]
    public var secondaryMuscles: [String]
    public var notes: String?

    public enum CodingKeys: String, CodingKey {
        case name, aliases, pattern, modality, category
        case primaryMuscles = "primary_muscles"
        case secondaryMuscles = "secondary_muscles"
        case notes
    }

    public init(name: String, aliases: [String] = [], pattern: Pattern? = nil,
                modality: Modality? = nil, category: ExerciseCategory = .strength,
                primaryMuscles: [String] = [], secondaryMuscles: [String] = [],
                notes: String? = nil) {
        self.name = name
        self.aliases = aliases
        self.pattern = pattern
        self.modality = modality
        self.category = category
        self.primaryMuscles = primaryMuscles
        self.secondaryMuscles = secondaryMuscles
        self.notes = notes
    }

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        name = try c.decode(String.self, forKey: .name)
        aliases = try c.decodeIfPresent([String].self, forKey: .aliases) ?? []
        pattern = try c.decodeIfPresent(Pattern.self, forKey: .pattern)
        modality = try c.decodeIfPresent(Modality.self, forKey: .modality)
        category = try c.decodeIfPresent(ExerciseCategory.self, forKey: .category) ?? .strength
        primaryMuscles = try c.decodeIfPresent([String].self, forKey: .primaryMuscles) ?? []
        secondaryMuscles = try c.decodeIfPresent([String].self, forKey: .secondaryMuscles) ?? []
        notes = try c.decodeIfPresent(String.self, forKey: .notes)
    }

    /// The P3 bar-speed rule applies to explosive movements — both power and speed work.
    public var isExplosive: Bool { category == .power || category == .speed }
}

/// Force–velocity / training emphasis of a movement.
/// - `power`: olympic lifts, force under heavy load (drives the P3 bar-speed rule)
/// - `speed`: light ballistic work (also drives P3)
/// - `strength`: max-force / loaded compound work
/// - `longevity`: durability work (grip, core, delts, calves, mobility, conditioning)
public enum ExerciseCategory: String, Codable, Equatable {
    case strength, power, speed, longevity
}

public enum Pattern: String, Codable, Equatable {
    case squat, hinge, press, pull, olympic, carry, core, grip
}

public enum Modality: String, Codable, Equatable {
    case barbell, dumbbell, kettlebell, bodyweight, machine
}

public struct Catalogue: Codable, Equatable {
    public var exercises: [Exercise]

    public init(exercises: [Exercise]) { self.exercises = exercises }

    public init(from decoder: Decoder) throws {
        // The file carries a top-level "$comment"; decode only "exercises".
        let c = try decoder.container(keyedBy: CodingKeys.self)
        exercises = try c.decode([Exercise].self, forKey: .exercises)
    }

    private enum CodingKeys: String, CodingKey { case exercises }

    /// Matches canonical names and aliases (case-insensitive); nil when unknown (caller warns, never blocks).
    public func resolve(_ typed: String) -> Exercise? {
        let key = typed.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        return exercises.first { ex in
            ex.name.lowercased() == key || ex.aliases.contains { $0.lowercased() == key }
        }
    }
}
