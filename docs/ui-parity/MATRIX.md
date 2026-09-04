# UI parity matrix

Last reviewed against commit `43959f1` (2026-09-04). Update the "Reviewed at"
line below whenever this table is re-checked against source, so staleness is
visible at a glance.

**Reviewed at:** `43959f1`

| Screen / feature | SwiftUI source | ImGui source | Status | Notes |
|---|---|---|---|---|
| App shell (sidebar + detail + status bar) | `WorkoutLogApp.swift`, `Views.swift` (`RootView`) | `root_view.hpp/cpp` | Done | |
| List/Cycle tab switch | `RootView` (segmented `Picker`) | `root_view.cpp` (`widgets::segmented`) | Done | |
| Session file list (sidebar) | `RootView` (`List(model.files...)`) | `root_view.cpp` (`draw_sidebar`) | Done | |
| Choose folder | `AppModel.chooseFolder()` (`NSOpenPanel`) | `platform.hpp/cpp` (`request_folder_dialog`) | Done | **Divergence (platform-forced):** ImGui also has a manual "type a path" fallback text field (`root_view.cpp:38-49`) because Linux has no guaranteed folder-picker portal (no XDG portal / zenity). Not a SwiftUI feature; keep it. |
| Reload folder | `RootView` toolbar button | `root_view.cpp` "Reload" button | Done | |
| Status bar (path or last status message) | `RootView` `.safeAreaInset` | `root_view.cpp` bottom `TextUnformatted` | Done | |
| Cycle day click → jump to List tab | `CalendarView.dayCell` (`model.open`, tab unchanged) | `root_view.cpp` `draw_detail` | **Divergence (intentional improvement)** | SwiftUI leaves the sidebar on the Cycle tab after opening a session from the calendar, so nothing visibly changes until the user flips tabs themselves. ImGui auto-switches to List. Documented at `root_view.cpp:66-73` — don't "fix" ImGui to match the SwiftUI quirk. |
| Session editor — header (date, cycle day, start, kind, bodyweight, notes) | `Views.swift` `SessionEditorView.header` | `session_editor.cpp` `draw_header` | Done | |
| Save session (button + shortcut) | `SessionEditorView` toolbar (⌘S) | `session_editor.cpp` (Ctrl+S) | Done | Icon-only in SwiftUI (SF Symbol), text-only in ImGui — cosmetic, no icon set in ImGui build. |
| Cardio block editor | `CardioEditor` | `session_editor.cpp` `draw_cardio_block` | Done | |
| Cooldown block editor | `CooldownEditor` | `session_editor.cpp` `draw_cooldown_block` | Done | |
| Metcon block (read-only display) | `MetconEditor` | `session_editor.cpp` `draw_metcon_block` | Done | Read-only on **both** sides by design — not a gap. |
| Strength block editor (sets summary, notation input, live preview + warnings, Apply) | `StrengthEditor` | `session_editor.cpp` `draw_strength_block` | Done | |
| Per-exercise muscle map (collapsible) | `StrengthEditor` `DisclosureGroup` | `session_editor.cpp` `CollapsingHeader("Muscle map")` | Done | |
| Day muscle map + weighting-mode picker | `DayMuscleMap` | `session_editor.cpp` `draw_day_muscle_map` | Done | |
| Cycle table (exercise × cycle-day presence grid) | `CycleTable` | `cycle_view.cpp` `draw_cycle_grid` | Done | Both intentionally have no pinned header/name column — matches the SwiftUI original's plain two-axis `ScrollView` (`cycle_view.cpp:15-17`). |
| Cycle muscle map + weighting-mode picker | `CycleMuscleMap` | `cycle_view.cpp` `draw_cycle_muscle_map` | Done | |
| Calendar (month grid, day badges, prev/next) | `CalendarView.swift` | `calendar_view.hpp/cpp` | Done | |
| Calendar first weekday | `Calendar.current.firstWeekday` (locale-dependent: Sun in en_US, Mon in uk_UA) | `calendar_view.cpp` (`kFirstWeekday = 2`, fixed Monday) | **Divergence (platform-forced)** | C++ side has no locale plumbing; fixed to Monday to match the Ukrainian training data rather than reader's system locale. Documented at `calendar_view.cpp:18-22`. |
| Calendar → open session on click | `CalendarView.dayCell.onTapGesture` | `calendar_view.cpp` `draw_day_cell` | Done | |
| Muscle group legend | `MuscleGroupLegend` | `widgets.hpp/cpp` `muscle_group_legend` | Done | |
| Muscle map rendering | `MuscleMapView.swift` (`WKWebView`, raw SVG string) | `svg_texture.hpp/cpp` (rasterized to GPU texture, presumably via lunasvg) | Done | Different rendering technology (embedded browser vs. rasterizer) but same visual output and same SVG source (`MuscleMapSVG.colorize` / `workoutlog::` equivalent) — not a gap, just worth knowing if a map ever renders differently between the two. |
| `DayInfo.isMetcon` flag | `WorkoutLogApp.swift` (computed, stored on `DayInfo`) | — | N/A | Appears unused by any SwiftUI view today (not rendered anywhere) — not a UI feature to port, just tracked domain state. Re-check if a future SwiftUI change starts using it. |

## Screens with no counterpart yet

None currently — every SwiftUI screen has an ImGui counterpart as of the
reviewed commit. If a new SwiftUI screen is added, add a row here immediately
(status **Missing**) even before the ImGui side exists, so the gap is visible.
