import Foundation

/// Files are the source of truth (ADR-001). Filename: `YYYY-MM-DD_<cycleDay>.json`.
public struct SessionStore {
    public let folder: URL

    public init(folder: URL) {
        self.folder = folder
    }

    public func listURLs() throws -> [URL] {
        try FileManager.default
            .contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)
            .filter { $0.pathExtension == "json" }
            .sorted { $0.lastPathComponent < $1.lastPathComponent }
    }

    public func load(_ url: URL) throws -> Session {
        try SessionCoding.decode(try Data(contentsOf: url))
    }

    /// A corrupted file costs one session, never the archive: parse failures are collected, not thrown.
    public func loadAll() throws -> (sessions: [Session], failed: [(URL, Error)]) {
        var ok: [Session] = []
        var failed: [(URL, Error)] = []
        for url in try listURLs() {
            do { ok.append(try load(url)) }
            catch { failed.append((url, error)) }
        }
        return (ok, failed)
    }

    public func filename(for session: Session) -> String {
        "\(session.date)_\(session.cycleDay).json"
    }

    public func url(for session: Session) -> URL {
        folder.appendingPathComponent(filename(for: session))
    }

    @discardableResult
    public func save(_ session: Session) throws -> URL {
        let target = url(for: session)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        try SessionCoding.encode(session).write(to: target, options: .atomic)
        return target
    }
}
