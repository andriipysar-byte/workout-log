import SwiftUI
import AppKit
import WorkoutLogCore

@main
struct WorkoutLogApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup("WorkoutLog") {
            RootView()
                .environmentObject(model)
                .frame(minWidth: 820, minHeight: 560)
        }
    }
}

/// A bare `swift run` executable (no .app bundle) launches unfocused; promoting to
/// a regular foreground app is what makes the window appear and activate.
final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

struct DayInfo: Equatable {
    let url: URL
    let cycleDay: String
    let isMetcon: Bool
    let group: MuscleGroup?
}

/// Which exercises appear on which cycle day. `cells[exercise][day]` is presence only (no load);
/// `groups[exercise]` is that exercise's primary muscle group(s) for colour coding.
struct CycleMatrix: Equatable {
    var days: [String] = []
    var exercises: [String] = []
    var cells: [[Bool]] = []
    var groups: [[MuscleGroup]] = []
}

/// All domain work is delegated to WorkoutLogCore (ADR-004): no parsing/validation here.
@MainActor
final class AppModel: ObservableObject {
    @Published var folder: URL
    @Published var files: [URL] = []
    @Published var selection: URL?
    @Published var session: Session?
    @Published var status: String = ""
    @Published var mode: WeightingMode = .setCount
    @Published var calendar: [String: DayInfo] = [:]   // keyed "yyyy-MM-dd"
    @Published var cycle = CycleMatrix()
    @Published var cycleSessions: [Session] = []

    private var store: SessionStore { SessionStore(folder: folder) }
    private var catalogue: Catalogue?
    private var mapTemplate: String?

    init() {
        self.folder = AppModel.defaultFolder()
        loadAssets()
        refresh()
    }

    /// This file is at app/Sources/WorkoutLogApp/, so walk up 4 levels to the repo root.
    private static func repoRoot() -> URL {
        URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
    }

    private func loadAssets() {
        let root = AppModel.repoRoot()
        catalogue = try? SessionCoding.decodeCatalogue(
            Data(contentsOf: root.appendingPathComponent("exercises.json")))
        mapTemplate = try? String(
            contentsOf: root.appendingPathComponent("app/Resources/muscle-map.svg"), encoding: .utf8)
    }

    func dayMapSVG() -> String? {
        guard let session, let catalogue, let mapTemplate else { return nil }
        let scores = MuscleActivation().forSession(session, catalogue: catalogue, mode: mode)
        return MuscleMapSVG.colorize(template: mapTemplate, scores: scores)
    }

    func cycleMapSVG() -> String? {
        guard let catalogue, let mapTemplate, !cycleSessions.isEmpty else { return nil }
        let scores = MuscleActivation().forSessions(cycleSessions, catalogue: catalogue, mode: mode)
        return MuscleMapSVG.colorize(template: mapTemplate, scores: scores)
    }

    func exerciseMapSVG(_ name: String) -> String? {
        guard let catalogue, let mapTemplate, let ex = catalogue.resolve(name) else { return nil }
        return MuscleMapSVG.colorize(template: mapTemplate, scores: MuscleActivation().forExercise(ex))
    }

    /// $WORKOUTLOG_DATA if set, else ../data relative to the CWD (so `swift run` from app/ finds data/).
    static func defaultFolder() -> URL {
        if let env = ProcessInfo.processInfo.environment["WORKOUTLOG_DATA"], !env.isEmpty {
            return URL(fileURLWithPath: (env as NSString).expandingTildeInPath, isDirectory: true)
        }
        return URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
            .appendingPathComponent("../data")
            .standardizedFileURL
    }

    func refresh() {
        files = (try? store.listURLs()) ?? []
        var cal: [String: DayInfo] = [:]
        var latest: [String: (date: String, session: Session)] = [:]
        for url in files {
            let base = url.deletingPathExtension().lastPathComponent
            guard let sep = base.firstIndex(of: "_") else { continue }
            let date = String(base[..<sep])
            let cycleDay = String(base[base.index(after: sep)...])
            let loaded = try? store.load(url)
            let isMetcon = loaded?.blocks.contains {
                if case .metcon = $0 { return true } else { return false }
            } ?? false
            var group: MuscleGroup?
            if let s = loaded, let cat = catalogue {
                group = MuscleGroup.dominant(in: MuscleActivation().forSession(s, catalogue: cat, mode: .setCount))
            }
            cal[date] = DayInfo(url: url, cycleDay: cycleDay, isMetcon: isMetcon, group: group)
            if let s = loaded, latest[cycleDay].map({ date > $0.date }) ?? true {
                latest[cycleDay] = (date, s)
            }
        }
        calendar = cal
        cycle = AppModel.buildCycle(latest, catalogue: catalogue)
        cycleSessions = latest.keys.sorted().compactMap { latest[$0]?.session }
    }

    /// One column per cycle day (using each day's most recent session), rows = exercises in first-seen order.
    private static func buildCycle(_ latest: [String: (date: String, session: Session)], catalogue: Catalogue?) -> CycleMatrix {
        let days = latest.keys.sorted()
        var exercises: [String] = []
        var byDay: [String: Set<String>] = [:]
        for label in days {
            guard let session = latest[label]?.session else { continue }
            var present: Set<String> = []
            for block in session.blocks {
                let names: [String]
                switch block {
                case .strength(let b): names = [b.exercise]
                case .metcon(let m): names = m.exercises.map(\.name)
                default: names = []
                }
                for name in names {
                    present.insert(name)
                    if !exercises.contains(name) { exercises.append(name) }
                }
            }
            byDay[label] = present
        }
        let cells = exercises.map { ex in days.map { byDay[$0]?.contains(ex) ?? false } }
        let groups = exercises.map { primaryGroups($0, catalogue: catalogue) }
        return CycleMatrix(days: days, exercises: exercises, cells: cells, groups: groups)
    }

    /// Distinct primary muscle groups of an exercise, in listed order (empty if not in catalogue).
    static func primaryGroups(_ name: String, catalogue: Catalogue?) -> [MuscleGroup] {
        guard let ex = catalogue?.resolve(name) else { return [] }
        var seen: [MuscleGroup] = []
        for m in ex.primaryMuscles where MuscleGroup.of(m) != nil {
            let g = MuscleGroup.of(m)!
            if !seen.contains(g) { seen.append(g) }
        }
        return seen
    }

    func open(_ url: URL) {
        do {
            session = try store.load(url)
            selection = url
            status = "Loaded \(url.lastPathComponent)"
        } catch {
            status = "Load failed: \(error.localizedDescription)"
        }
    }

    func save() {
        guard let session else { return }
        do {
            let written = try store.save(session)
            status = "Saved \(written.lastPathComponent)"
            refresh()
            selection = written
        } catch {
            status = "Save failed: \(error.localizedDescription)"
        }
    }

    func chooseFolder() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.allowsMultipleSelection = false
        panel.directoryURL = folder
        if panel.runModal() == .OK, let url = panel.url {
            folder = url
            session = nil
            selection = nil
            refresh()
            status = "Folder: \(url.path)"
        }
    }
}
