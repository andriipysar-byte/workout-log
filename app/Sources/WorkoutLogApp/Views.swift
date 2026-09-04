import SwiftUI
import WorkoutLogCore

enum SidebarTab: Hashable { case list, cycle }

struct RootView: View {
    @EnvironmentObject var model: AppModel
    @State private var tab: SidebarTab = .list

    var body: some View {
        NavigationSplitView {
            VStack(spacing: 0) {
                Picker("", selection: $tab) {
                    Text("List").tag(SidebarTab.list)
                    Text("Cycle").tag(SidebarTab.cycle)
                }
                .pickerStyle(.segmented).labelsHidden().padding(8)
                Divider()
                List(model.files, id: \.self, selection: sidebarSelection) { url in
                    Text(url.deletingPathExtension().lastPathComponent).tag(url)
                }
            }
            .frame(minWidth: 240)
            .navigationTitle("Sessions")
            .toolbar {
                Button { model.chooseFolder() } label: { Image(systemName: "folder") }
                    .help("Choose session folder")
                Button { model.refresh() } label: { Image(systemName: "arrow.clockwise") }
                    .help("Reload folder")
            }
        } detail: {
            switch tab {
            case .cycle:
                CycleView()
            case .list:
                if model.session != nil {
                    SessionEditorView(session: sessionBinding, onSave: model.save)
                } else {
                    ContentUnavailableFallback()
                }
            }
        }
        .safeAreaInset(edge: .bottom) {
            HStack {
                Text(model.status.isEmpty ? model.folder.path : model.status)
                    .font(.caption).foregroundStyle(.secondary)
                Spacer()
            }
            .padding(.horizontal, 12).padding(.vertical, 6)
            .background(.bar)
        }
    }

    private var sidebarSelection: Binding<URL?> {
        Binding(get: { model.selection },
                set: { if let u = $0 { model.open(u) } })
    }

    private var sessionBinding: Binding<Session> {
        Binding(get: { model.session ?? Session(date: "", cycleDay: "") },
                set: { model.session = $0 })
    }
}

private struct ContentUnavailableFallback: View {
    var body: some View {
        VStack(spacing: 8) {
            Image(systemName: "square.and.pencil").font(.largeTitle)
            Text("Select a session to edit").foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

private struct DayMuscleMap: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Label("Muscle map", systemImage: "figure.strengthtraining.traditional")
                    .font(.headline)
                Spacer()
                Picker("", selection: $model.mode) {
                    ForEach(WeightingMode.allCases, id: \.self) { Text($0.label).tag($0) }
                }
                .pickerStyle(.segmented).labelsHidden().frame(width: 220)
                .help("How each exercise is weighted into the day's map")
            }
            if let svg = model.dayMapSVG() {
                MuscleMapView(svg: svg)
                    .frame(height: 280)
                    .background(RoundedRectangle(cornerRadius: 10).fill(Color(nsColor: .controlBackgroundColor)))
            } else {
                Text("Muscle map unavailable (catalogue or template not found).")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
    }
}

struct SessionEditorView: View {
    @Binding var session: Session
    var onSave: () -> Void

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                header
                Divider()
                ForEach(session.blocks.indices, id: \.self) { i in
                    BlockCard(block: $session.blocks[i])
                }
                Divider()
                DayMuscleMap()
            }
            .padding(20)
        }
        .toolbar {
            Button { onSave() } label: { Label("Save", systemImage: "square.and.arrow.down") }
                .keyboardShortcut("s", modifiers: .command)
        }
    }

    private var header: some View {
        Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 8) {
            GridRow {
                labeled("Date") { TextField("YYYY-MM-DD", text: $session.date) }
                labeled("Cycle day") { TextField("A1", text: $session.cycleDay) }
            }
            GridRow {
                labeled("Start") { TextField("H:MM", text: optional($session.startTime)) }
                labeled("Kind") {
                    Picker("", selection: $session.kind) {
                        Text("training").tag(Kind.training)
                        Text("deload").tag(Kind.deload)
                        Text("retest").tag(Kind.retest)
                    }.labelsHidden()
                }
            }
            GridRow {
                labeled("Bodyweight") {
                    TextField("kg", value: $session.bodyweightKg, format: .number)
                }
                labeled("Notes") { TextField("", text: optional($session.notes)) }
            }
        }
    }

    private func labeled<Content: View>(_ title: String, @ViewBuilder _ content: () -> Content) -> some View {
        HStack(spacing: 6) {
            Text(title).font(.caption).foregroundStyle(.secondary).frame(width: 78, alignment: .trailing)
            content().textFieldStyle(.roundedBorder).frame(maxWidth: 220)
        }
    }
}

struct BlockCard: View {
    @Binding var block: Block

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            switch block {
            case .cardio:   if let b = cardio   { CardioEditor(block: b) }
            case .strength: if let b = strength { StrengthEditor(block: b) }
            case .metcon:   if let b = metcon   { MetconEditor(block: b) }
            case .cooldown: if let b = cooldown { CooldownEditor(block: b) }
            }
        }
        .padding(12)
        .background(RoundedRectangle(cornerRadius: 10).fill(Color(nsColor: .controlBackgroundColor)))
    }

    private var cardio: Binding<CardioBlock>? {
        guard case .cardio(let c) = block else { return nil }
        return Binding(get: { if case .cardio(let x) = block { return x } else { return c } },
                       set: { block = .cardio($0) })
    }
    private var strength: Binding<StrengthBlock>? {
        guard case .strength(let s) = block else { return nil }
        return Binding(get: { if case .strength(let x) = block { return x } else { return s } },
                       set: { block = .strength($0) })
    }
    private var metcon: Binding<MetconBlock>? {
        guard case .metcon(let m) = block else { return nil }
        return Binding(get: { if case .metcon(let x) = block { return x } else { return m } },
                       set: { block = .metcon($0) })
    }
    private var cooldown: Binding<CooldownBlock>? {
        guard case .cooldown(let d) = block else { return nil }
        return Binding(get: { if case .cooldown(let x) = block { return x } else { return d } },
                       set: { block = .cooldown($0) })
    }
}

private struct BlockHeader: View {
    let symbol: String
    let title: String
    var body: some View {
        HStack(spacing: 6) {
            Image(systemName: symbol).foregroundStyle(.tint)
            Text(title).font(.headline)
        }
    }
}

struct CardioEditor: View {
    @Binding var block: CardioBlock
    var body: some View {
        BlockHeader(symbol: "figure.run", title: "Cardio")
        HStack {
            TextField("machine", text: $block.machine).textFieldStyle(.roundedBorder).frame(maxWidth: 200)
            TextField("min", value: $block.durationMin, format: .number).textFieldStyle(.roundedBorder).frame(width: 70)
            TextField("distance m", value: $block.distanceM, format: .number).textFieldStyle(.roundedBorder).frame(width: 100)
            TextField("end", text: optional($block.endTime)).textFieldStyle(.roundedBorder).frame(width: 70)
        }
    }
}

struct CooldownEditor: View {
    @Binding var block: CooldownBlock
    var body: some View {
        BlockHeader(symbol: "wind", title: "Cooldown")
        HStack {
            TextField("end", text: optional($block.endTime)).textFieldStyle(.roundedBorder).frame(width: 80)
            TextField("notes", text: optional($block.notes)).textFieldStyle(.roundedBorder)
        }
    }
}

struct MetconEditor: View {
    @Binding var block: MetconBlock
    var body: some View {
        BlockHeader(symbol: "flame", title: "Metcon")
        Text(block.format.map { "format: \($0.rawValue)" } ?? "format: —")
            .font(.caption).foregroundStyle(.secondary)
        if let scheme = block.scheme {
            Text("scheme: " + scheme.map(String.init).joined(separator: "-")).font(.caption)
        }
        ForEach(block.exercises.indices, id: \.self) { i in
            HStack {
                Text("•").foregroundStyle(.secondary)
                Text(block.exercises[i].name)
                if let w = block.exercises[i].weightKg { Text("(\(Int(w)) kg)").foregroundStyle(.secondary) }
            }.font(.callout)
        }
        if let notes = block.notes { Text(notes).font(.caption).foregroundStyle(.secondary) }
    }
}

struct StrengthEditor: View {
    @Binding var block: StrengthBlock
    @EnvironmentObject var model: AppModel
    @State private var notation: String = ""
    @State private var showMap = false

    private var parsed: ParsedSets { Notation.parseStrengthSets(notation) }

    var body: some View {
        BlockHeader(symbol: "dumbbell", title: block.exercise)
        if let notes = block.notes {
            Text(notes).font(.caption).foregroundStyle(.secondary)
        }

        if block.sets.isEmpty {
            Text("no sets yet").font(.caption).foregroundStyle(.secondary)
        } else {
            Text(block.sets.map(summary).joined(separator: "   "))
                .font(.system(.callout, design: .monospaced))
        }

        VStack(alignment: .leading, spacing: 4) {
            HStack {
                TextField("6 × [70, 80, 90, 100, 110]", text: $notation)
                    .textFieldStyle(.roundedBorder)
                    .font(.system(.body, design: .monospaced))
                    .onSubmit(apply)
                Button("Apply", action: apply)
                    .disabled(parsed.sets.isEmpty)
            }
            if !notation.isEmpty {
                Text("→ " + (parsed.sets.isEmpty ? "—" : parsed.sets.map(summary).joined(separator: "  ")))
                    .font(.caption).foregroundStyle(parsed.sets.isEmpty ? .red : .green)
                ForEach(parsed.warnings, id: \.self) { w in
                    Label(w, systemImage: "exclamationmark.triangle").font(.caption2).foregroundStyle(.orange)
                }
            }
        }

        // Built only when expanded, to avoid spinning up a WebView unnecessarily.
        DisclosureGroup("Muscle map", isExpanded: $showMap) {
            if showMap {
                if let svg = model.exerciseMapSVG(block.exercise) {
                    MuscleMapView(svg: svg).frame(height: 200)
                } else {
                    Text("\"\(block.exercise)\" not in catalogue — no map.")
                        .font(.caption).foregroundStyle(.secondary)
                }
            }
        }
        .font(.caption)
    }

    private func apply() {
        guard !parsed.sets.isEmpty else { return }
        block.sets = parsed.sets
        notation = ""
    }

    private func summary(_ s: WorkSet) -> String {
        if let cluster = s.cluster {
            return cluster.map(String.init).joined(separator: "+") + (s.totalReps.map { " (\($0))" } ?? "")
        }
        if let sec = s.durationSec { return "\(Int(sec))c" }
        let reps = s.reps.map { "\($0)×" } ?? ""
        let weight = s.weightKg.map { "\(Int($0))" } ?? "bw"
        let bo = (s.isBackoff == true) ? "*" : ""
        return reps + weight + bo
    }
}

struct CycleView: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            CycleTable(matrix: model.cycle)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            Divider()
            HStack(alignment: .top, spacing: 16) {
                VStack(alignment: .leading, spacing: 12) {
                    CycleMuscleMap()
                    CalendarView().frame(width: 300, height: 320)
                }
                .frame(width: 320)
                Spacer()
            }
            .padding(8)
        }
    }
}

private struct CycleMuscleMap: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Label("Cycle muscle map", systemImage: "figure.strengthtraining.traditional")
                    .font(.headline)
                Spacer()
            }
            Picker("", selection: $model.mode) {
                ForEach(WeightingMode.allCases, id: \.self) { Text($0.label).tag($0) }
            }
            .pickerStyle(.segmented).labelsHidden()
            .help("How each exercise is weighted into the cycle map")
            if let svg = model.cycleMapSVG() {
                MuscleMapView(svg: svg)
                    .frame(height: 260)
                    .background(RoundedRectangle(cornerRadius: 10).fill(Color(nsColor: .controlBackgroundColor)))
            } else {
                Text("Muscle map unavailable (no sessions or catalogue missing).")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
    }
}

struct CycleTable: View {
    let matrix: CycleMatrix

    private let nameWidth: CGFloat = 240
    private let dayWidth: CGFloat = 46

    private var separator: some View {
        Rectangle().frame(height: 0.5).foregroundStyle(Color(nsColor: .separatorColor))
    }

    var body: some View {
        if matrix.days.isEmpty {
            Text("No sessions in this folder.")
                .foregroundStyle(.secondary)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        } else {
            ScrollView([.vertical, .horizontal]) {
                Grid(alignment: .leading, horizontalSpacing: 0, verticalSpacing: 0) {
                    GridRow {
                        headerCell("Exercise", width: nameWidth, align: .leading)
                        ForEach(matrix.days, id: \.self) { day in
                            headerCell(day, width: dayWidth, align: .center)
                        }
                    }
                    ForEach(Array(matrix.exercises.enumerated()), id: \.offset) { i, ex in
                        let groups = matrix.groups.indices.contains(i) ? matrix.groups[i] : []
                        GridRow {
                            nameCell(ex, groups: groups)
                            ForEach(Array(matrix.days.enumerated()), id: \.offset) { j, _ in
                                markCell(present: matrix.cells[i][j], groups: groups)
                            }
                        }
                    }
                }
                .padding(1)
            }
        }
    }

    private func headerCell(_ text: String, width: CGFloat, align: Alignment) -> some View {
        Text(text)
            .font(.caption.bold()).lineLimit(1)
            .frame(width: width, alignment: align)
            .padding(.vertical, 6).padding(.horizontal, 8)
            .frame(maxHeight: .infinity)
            .background(Color(nsColor: .controlBackgroundColor))
            .overlay(separator, alignment: .bottom)
    }

    private func nameCell(_ text: String, groups: [MuscleGroup]) -> some View {
        Text(text)
            .font(.callout).lineLimit(1)
            .frame(width: nameWidth, alignment: .leading)
            .padding(.vertical, 6).padding(.horizontal, 8)
            .frame(maxHeight: .infinity)
            .background(groupGradient(groups, base: 0.22))
            .overlay(separator, alignment: .bottom)
    }

    private func markCell(present: Bool, groups: [MuscleGroup]) -> some View {
        ZStack {
            if present {
                Circle().fill(groups.first?.color ?? Color.accentColor).frame(width: 9, height: 9)
            }
        }
        .frame(width: dayWidth)
        .frame(maxHeight: .infinity)
        .background { if present { groupGradient(groups, base: 0.16) } else { Color.clear } }
        .overlay(separator, alignment: .bottom)
    }
}

/// Bind an optional String to a non-optional TextField (empty string ⇄ nil).
func optional(_ source: Binding<String?>) -> Binding<String> {
    Binding(get: { source.wrappedValue ?? "" },
            set: { source.wrappedValue = $0.isEmpty ? nil : $0 })
}

extension Color {
    init(hex: String) {
        var h = hex
        if h.hasPrefix("#") { h.removeFirst() }
        let v = UInt64(h, radix: 16) ?? 0
        self.init(.sRGB, red: Double((v >> 16) & 0xff) / 255,
                  green: Double((v >> 8) & 0xff) / 255, blue: Double(v & 0xff) / 255)
    }
}

extension MuscleGroup {
    var color: Color { Color(hex: hex) }
}

/// A light left-to-right wash of an exercise's primary muscle-group colour(s); clear when uncatalogued.
func groupGradient(_ groups: [MuscleGroup], base: Double) -> LinearGradient {
    let colors: [Color]
    switch groups.count {
    case 0:  colors = [.clear, .clear]
    case 1:  colors = [groups[0].color.opacity(base * 1.4), groups[0].color.opacity(base * 0.5)]
    default: colors = groups.map { $0.color.opacity(base) }
    }
    return LinearGradient(colors: colors, startPoint: .topLeading, endPoint: .bottomTrailing)
}

struct MuscleGroupLegend: View {
    var body: some View {
        LazyVGrid(columns: [GridItem(.adaptive(minimum: 84), spacing: 8)], alignment: .leading, spacing: 4) {
            ForEach(MuscleGroup.allCases, id: \.self) { g in
                HStack(spacing: 5) {
                    RoundedRectangle(cornerRadius: 3).fill(g.color).frame(width: 11, height: 11)
                    Text(g.label).font(.caption2).foregroundStyle(.secondary)
                }
            }
        }
    }
}
