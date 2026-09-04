import Foundation

/// Pretty-printed with sorted keys so on-disk files produce clean, stable git diffs.
public enum SessionCoding {
    public static func makeEncoder() -> JSONEncoder {
        let e = JSONEncoder()
        e.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
        return e
    }

    public static func makeDecoder() -> JSONDecoder { JSONDecoder() }

    public static func decode(_ data: Data) throws -> Session {
        try makeDecoder().decode(Session.self, from: data)
    }

    public static func encode(_ session: Session) throws -> Data {
        try makeEncoder().encode(session)
    }

    public static func decodeCatalogue(_ data: Data) throws -> Catalogue {
        try makeDecoder().decode(Catalogue.self, from: data)
    }
}
