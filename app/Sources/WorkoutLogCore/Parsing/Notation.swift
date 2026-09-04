import Foundation

/// Warn-never-block: a line we can't fully parse still yields the sets it can.
public struct ParsedSets: Equatable {
    public var sets: [WorkSet]
    public var warnings: [String]
    public init(sets: [WorkSet] = [], warnings: [String] = []) {
        self.sets = sets
        self.warnings = warnings
    }
}

/// Parses the terse paper-log notation into model sets. Pure (no UI, ADR-004);
/// see `docs/03-log-notation.md` for the full grammar.
///
/// Supported strength forms:
///   6 × [70, 80, 90, 100, 110]          fixed reps × ascending weights → 5 sets
///   6 × [70, 80] + 6 × [30]             trailing groups are back-off sets
///   6 × [30, 60, 80(4), 86(2), 90(1)]   per-set rep override in parentheses
///   5+5+4+3+3 (20)                      cluster set: chain + total in parens
///   4 × [54c, 40c, 36c, 42c]            timed holds (c/с = seconds) → duration sets
///   {60c, 70c, 30c, 35c}                bare bracketed list, no count prefix
public enum Notation {

    private static let multipliers: Set<Character> = ["×", "x", "X", "х", "Х", "*"]
    private static let secondsSuffix: Set<Character> = ["c", "C", "с", "С"]

    public static func parseStrengthSets(_ raw: String) -> ParsedSets {
        var warnings: [String] = []
        let line = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !line.isEmpty else { return ParsedSets() }

        let hasMultiplier = line.contains { multipliers.contains($0) }
        let hasBracket = line.contains("[") || line.contains("{")

        if !hasMultiplier && !hasBracket && line.contains("+") {
            return parseCluster(line, &warnings)
        }

        // First group = ramp/straight sets, later top-level '+' groups = back-offs.
        var sets: [WorkSet] = []
        for (idx, group) in splitTopLevel(Array(line), separator: "+").enumerated() {
            sets.append(contentsOf: parseGroup(group, isBackoff: idx > 0, &warnings))
        }
        if sets.isEmpty && warnings.isEmpty {
            warnings.append("could not parse: \(line)")
        }
        return ParsedSets(sets: sets, warnings: warnings)
    }

    private static func parseCluster(_ line: String, _ warnings: inout [String]) -> ParsedSets {
        var body = line
        var total: Int?
        if let r = body.range(of: #"\(\s*\d+\s*\)\s*$"#, options: .regularExpression) {
            total = Int(body[r].filter(\.isNumber))
            body.removeSubrange(r)
        }
        let parts = body.split(separator: "+").map { $0.trimmingCharacters(in: .whitespaces) }
        let reps = parts.compactMap { Int($0) }
        guard !reps.isEmpty, reps.count == parts.count else {
            warnings.append("could not parse cluster: \(line)")
            return ParsedSets(warnings: warnings)
        }
        let set = WorkSet(cluster: reps, totalReps: total ?? reps.reduce(0, +))
        return ParsedSets(sets: [set], warnings: warnings)
    }

    private static func parseGroup(_ group: String, isBackoff: Bool, _ warnings: inout [String]) -> [WorkSet] {
        let chars = Array(group)
        var count: Int?
        var listPart = group

        if let mIdx = chars.firstIndex(where: { multipliers.contains($0) }) {
            let countStr = String(chars[..<mIdx]).trimmingCharacters(in: .whitespaces)
            if let c = Int(countStr) {
                count = c
            } else {
                warnings.append("unrecognised rep count \"\(countStr)\" in \"\(group)\"")
            }
            listPart = String(chars[(mIdx + 1)...])
        }

        listPart = listPart.trimmingCharacters(in: CharacterSet(charactersIn: "[]{} \t"))
        let items = listPart
            .split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }

        guard !items.isEmpty else {
            warnings.append("no values in \"\(group)\"")
            return []
        }

        var out: [WorkSet] = []
        for item in items {
            if let seconds = parseDuration(item) {
                out.append(WorkSet(durationSec: seconds, isBackoff: isBackoff ? true : nil))
            } else if let (weight, repsOverride) = parseWeight(item) {
                out.append(WorkSet(weightKg: weight,
                                   reps: repsOverride ?? count,
                                   isBackoff: isBackoff ? true : nil))
            } else {
                warnings.append("unparseable value \"\(item)\" in \"\(group)\"")
            }
        }
        return out
    }

    /// "54c" / "54с" (Latin or Cyrillic suffix) → 54 seconds; nil if not a duration token.
    private static func parseDuration(_ token: String) -> Double? {
        guard let last = token.last, secondsSuffix.contains(last) else { return nil }
        let num = token.dropLast().trimmingCharacters(in: .whitespaces)
            .replacingOccurrences(of: ",", with: ".")
        return Double(num)
    }

    /// "90" or "90(2)" or "47.5" → (weight, optional rep override).
    private static func parseWeight(_ token: String) -> (Double, Int?)? {
        var t = token
        var reps: Int?
        if let r = t.range(of: #"\(\s*\d+\s*\)\s*$"#, options: .regularExpression) {
            reps = Int(t[r].filter(\.isNumber))
            t.removeSubrange(r)
        }
        t = t.trimmingCharacters(in: .whitespaces).replacingOccurrences(of: ",", with: ".")
        guard let weight = Double(t) else { return nil }
        return (weight, reps)
    }

    /// Split on `separator` at bracket depth 0 (so weights inside [...] stay intact).
    private static func splitTopLevel(_ chars: [Character], separator: Character) -> [String] {
        var result: [String] = []
        var current = ""
        var depth = 0
        for ch in chars {
            switch ch {
            case "[", "{": depth += 1; current.append(ch)
            case "]", "}": depth = max(0, depth - 1); current.append(ch)
            case separator where depth == 0:
                result.append(current); current = ""
            default:
                current.append(ch)
            }
        }
        result.append(current)
        return result
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }
}
