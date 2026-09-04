import SwiftUI

struct CalendarView: View {
    @EnvironmentObject var model: AppModel
    @State private var anchor = Date()

    private let cal = Calendar.current
    private static let heavy = Color(red: 0.16, green: 0.47, blue: 0.84)   // #2a78d6

    private var keyFormatter: DateFormatter {
        let f = DateFormatter()
        f.calendar = cal; f.timeZone = cal.timeZone; f.dateFormat = "yyyy-MM-dd"
        return f
    }

    var body: some View {
        VStack(spacing: 10) {
            monthHeader
            weekdayRow
            grid
            legend
            Spacer()
        }
        .padding(10)
        .frame(minWidth: 260)
        .onAppear(perform: anchorToLatestSession)
    }

    private var monthTitle: String {
        let f = DateFormatter(); f.calendar = cal; f.dateFormat = "LLLL yyyy"
        return f.string(from: anchor)
    }

    private var monthHeader: some View {
        HStack {
            Button { shiftMonth(-1) } label: { Image(systemName: "chevron.left") }
            Spacer()
            Text(monthTitle).font(.headline)
            Spacer()
            Button { shiftMonth(1) } label: { Image(systemName: "chevron.right") }
        }
        .buttonStyle(.borderless)
    }

    private var weekdayRow: some View {
        HStack(spacing: 4) {
            ForEach(orderedWeekdaySymbols, id: \.self) { s in
                Text(s).font(.caption2).foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity)
            }
        }
    }

    private var grid: some View {
        LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 4), count: 7), spacing: 4) {
            ForEach(Array(monthCells.enumerated()), id: \.offset) { _, date in
                if let date { dayCell(date) } else { Color.clear.frame(height: 34) }
            }
        }
    }

    @ViewBuilder
    private func dayCell(_ date: Date) -> some View {
        let key = keyFormatter.string(from: date)
        let info = model.calendar[key]
        let selected = info != nil && model.selection == info!.url
        VStack(spacing: 1) {
            Text("\(cal.component(.day, from: date))").font(.caption2)
                .foregroundStyle(info == nil ? .secondary : .primary)
            if let info {
                Text(info.cycleDay)
                    .font(.system(size: 10, weight: .bold))
                    .foregroundStyle(.white)
                    .padding(.horizontal, 5).padding(.vertical, 1)
                    .background(Capsule().fill(info.group?.color ?? Self.heavy))
            }
        }
        .frame(maxWidth: .infinity, minHeight: 34)
        .background(RoundedRectangle(cornerRadius: 6)
            .fill(selected ? Color.accentColor.opacity(0.18) : Color.clear))
        .overlay(RoundedRectangle(cornerRadius: 6)
            .stroke(selected ? Color.accentColor : .clear, lineWidth: 1))
        .contentShape(Rectangle())
        .onTapGesture { if let info { model.open(info.url) } }
    }

    private var legend: some View { MuscleGroupLegend() }

    private var orderedWeekdaySymbols: [String] {
        let syms = cal.shortWeekdaySymbols                 // index 0 = Sunday
        return (0..<7).map { syms[(cal.firstWeekday - 1 + $0) % 7] }
    }

    /// Cells for the month: leading nils for alignment, then each day.
    private var monthCells: [Date?] {
        guard let monthStart = cal.date(from: cal.dateComponents([.year, .month], from: anchor)),
              let range = cal.range(of: .day, in: .month, for: monthStart) else { return [] }
        let weekdayOfFirst = cal.component(.weekday, from: monthStart)
        let leading = (weekdayOfFirst - cal.firstWeekday + 7) % 7
        var cells: [Date?] = Array(repeating: nil, count: leading)
        for day in range {
            cells.append(cal.date(byAdding: .day, value: day - 1, to: monthStart))
        }
        return cells
    }

    private func shiftMonth(_ delta: Int) {
        if let d = cal.date(byAdding: .month, value: delta, to: anchor) { anchor = d }
    }

    /// Open on the month of the most recent session so data is visible immediately.
    private func anchorToLatestSession() {
        if let latest = model.calendar.keys.max(), let d = keyFormatter.date(from: latest) {
            anchor = d
        }
    }
}
