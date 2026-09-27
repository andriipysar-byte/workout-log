use dioxus::prelude::*;
use workout_log_core::Exercise;
use workout_log_core::analytics::progress::pattern_name;
use workout_log_core::cycles::cycle_day::{CycleDay, DayKind};
use workout_log_core::models::cycle::{BlockTemplate, Cycle, CycleSession};
use workout_log_core::models::exercise::{ExerciseCategory, MovementPattern};
use workout_log_core::models::metcon::MetconExercise;

use crate::state::AppState;
use crate::ui::{Confirm, Field, GroupLegend, Modal, MuscleMap, UNCLASSIFIED};

#[derive(Clone, Copy, PartialEq)]
enum Dialog {
    None,
    NewCycle,
    CloneCycle,
    EditCycle,
    Retitle(usize),
    AddBlock(usize),
    EditBlock(usize, usize),
    PickExercise(usize, usize),
    RemoveWorkout(usize),
}

#[component]
pub fn PlanView() -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut selected = use_signal(|| 0usize);
    let mut dialog = use_signal(|| Dialog::None);

    let cycles = state.read().cycles.clone();
    let editable = state.read().can_edit_plan();
    let Some(catalogue) = cycles else {
        return rsx! { p { class: "p-5 text-sm text-slate-400", "cycles.json could not be read." } };
    };

    if catalogue.cycles.is_empty() {
        return rsx! {
            div { class: "space-y-3 p-5",
                p { class: "text-sm text-slate-500", "No cycles yet." }
                button {
                    class: "rounded bg-sky-600 px-3 py-1 text-sm text-white disabled:bg-slate-200",
                    disabled: !editable,
                    onclick: move |_| dialog.set(Dialog::NewCycle),
                    "New cycle"
                }
            }
        };
    }

    let index = (*selected.read()).min(catalogue.cycles.len() - 1);
    let cycle = catalogue.cycles[index].clone();

    rsx! {
        div { class: "space-y-4 p-5",
            div { class: "flex flex-wrap items-end gap-2",
                label { class: "flex flex-col gap-1",
                    span { class: "text-[11px] font-medium uppercase tracking-wide text-slate-500", "Cycle" }
                    select {
                        class: "w-64 rounded border border-slate-300 px-2 py-1 text-sm",
                        onchange: move |event| {
                            if let Ok(next) = event.value().parse::<usize>() { selected.set(next); }
                        },
                        for (position, option) in catalogue.cycles.iter().enumerate() {
                            option { key: "{option.id}", value: "{position}", selected: position == index, "{option.name}" }
                        }
                    }
                }
                PlanButton { label: "New cycle", enabled: editable, onclick: move |_| dialog.set(Dialog::NewCycle) }
                PlanButton { label: "Clone", enabled: editable, onclick: move |_| dialog.set(Dialog::CloneCycle) }
                PlanButton { label: "Edit", enabled: editable, onclick: move |_| dialog.set(Dialog::EditCycle) }
                button {
                    class: "rounded bg-sky-600 px-3 py-1 text-sm text-white disabled:bg-slate-200 disabled:text-slate-400",
                    disabled: !editable,
                    onclick: move |_| state.write().save_cycles(),
                    "Save cycles.json"
                }
                if !editable {
                    span {
                        class: "text-xs text-slate-400",
                        title: "cycles.json is read-only here — it ships with the app on this platform (ADR-007).",
                        "🔒 read-only"
                    }
                }
            }

            div { class: "flex items-start justify-between rounded-lg border border-slate-200 bg-white p-3",
                div {
                    h2 { class: "text-sm font-semibold", "{cycle.name}" }
                    p { class: "text-xs text-slate-500",
                        "{cycle.sessions.len()} workouts · starts {cycle.start_date} · {cycle.training_days.join(\", \")}"
                    }
                }
                GroupLegend {}
            }

            div { class: "flex gap-3 overflow-x-auto pb-2",
                section { class: "w-72 shrink-0 rounded-lg border border-slate-200 bg-white p-3",
                    h3 { class: "text-sm font-semibold", "Cycle total" }
                    p { class: "mb-2 text-xs text-slate-500", "{cycle.sessions.len()} workouts · by set count" }
                    MuscleMap {
                        svg: state.read().plan_cycle_map(&cycle),
                        reason: "No exercises chosen yet — nothing to map.".to_string(),
                        height: "20rem".to_string(),
                    }
                }
                for (position, workout) in cycle.sessions.iter().enumerate() {
                    WorkoutCard {
                        key: "{position}-{workout.cycle_day}",
                        cycle_index: index,
                        position,
                        last: position + 1 == cycle.sessions.len(),
                        workout: workout.clone(),
                        planned: state.read().planned_date(&cycle, position),
                        editable,
                        dialog,
                    }
                }
                div { class: "flex w-48 shrink-0 items-start",
                    button {
                        class: "w-full rounded border border-dashed border-slate-300 px-3 py-2 text-sm text-slate-600 hover:bg-slate-50 disabled:text-slate-300",
                        disabled: !editable,
                        onclick: move |_| state.write().add_workout(index),
                        {
                            let codes: Vec<String> = cycle.sessions.iter().map(|s| s.cycle_day.clone()).collect();
                            format!("Add {}", workout_log_core::cycles::cycle_templates::next_day(&codes).code())
                        }
                    }
                }
            }
        }

        PlanDialogs { cycle_index: index, dialog }
    }
}

#[component]
fn PlanButton(label: String, enabled: bool, onclick: EventHandler<MouseEvent>) -> Element {
    rsx! {
        button {
            class: "rounded border border-slate-200 px-3 py-1 text-sm text-slate-700 hover:bg-slate-100 disabled:text-slate-300",
            disabled: !enabled,
            onclick: move |event| onclick.call(event),
            "{label}"
        }
    }
}

#[component]
fn WorkoutCard(
    cycle_index: usize,
    position: usize,
    last: bool,
    workout: CycleSession,
    planned: Option<String>,
    editable: bool,
    dialog: Signal<Dialog>,
) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut dialog = dialog;
    let mut show_map = use_signal(|| true);

    let day = CycleDay::try_parse(&workout.cycle_day);
    let group = state.read().plan_dominant_group(&workout);
    let subtitle = [planned, workout.weekday.clone(), workout.title.clone()]
        .into_iter()
        .flatten()
        .collect::<Vec<_>>()
        .join(" · ");

    rsx! {
        section { class: "flex w-96 shrink-0 flex-col rounded-lg border border-slate-200 bg-white p-3",
            div { class: "flex items-start gap-2",
                span {
                    class: "rounded px-1.5 py-0.5 text-[11px] font-bold text-white",
                    style: "background: {group.map_or(UNCLASSIFIED, |g| g.hex())}",
                    "{workout.cycle_day}"
                }
                div { class: "min-w-0 flex-1",
                    p { class: "truncate text-xs font-medium text-slate-700",
                        {day.map_or("custom".to_string(), |d| d.kind.label().to_string())}
                    }
                    p { class: "truncate text-[11px] text-slate-500", "{subtitle}" }
                }
                button {
                    class: "text-xs text-slate-400 hover:text-slate-600",
                    title: if *show_map.read() { "Hide muscle map" } else { "Show muscle map" },
                    onclick: move |_| { let next = !*show_map.read(); show_map.set(next); },
                    if *show_map.read() { "◉" } else { "○" }
                }
            }

            if editable {
                div { class: "mt-2 flex flex-wrap gap-1",
                    MiniButton { label: "←", enabled: position > 0,
                        onclick: move |_| state.write().move_workout(cycle_index, position, position - 1) }
                    MiniButton { label: "→", enabled: !last,
                        onclick: move |_| state.write().move_workout(cycle_index, position, position + 1) }
                    MiniButton { label: "Rename", enabled: true,
                        onclick: move |_| dialog.set(Dialog::Retitle(position)) }
                    MiniButton { label: "Remove", enabled: true,
                        onclick: move |_| dialog.set(Dialog::RemoveWorkout(position)) }
                }
            }

            if *show_map.read() {
                div { class: "mt-2",
                    MuscleMap {
                        svg: state.read().plan_map(&workout),
                        reason: "Nothing to map yet.".to_string(),
                        height: "13rem".to_string(),
                    }
                }
            }

            ul { class: "mt-2 space-y-1",
                for (block_index, block) in workout.blocks.iter().enumerate() {
                    BlockRow {
                        key: "{block_index}",
                        cycle_index, position, block_index,
                        block: block.clone(), editable, dialog,
                    }
                }
            }
            if editable {
                button {
                    class: "mt-2 self-start text-xs text-sky-700 underline",
                    onclick: move |_| dialog.set(Dialog::AddBlock(position)),
                    "Add block"
                }
            }
        }
    }
}

#[component]
fn MiniButton(label: String, enabled: bool, onclick: EventHandler<MouseEvent>) -> Element {
    rsx! {
        button {
            class: "rounded border border-slate-200 px-1.5 py-0.5 text-[11px] text-slate-600 hover:bg-slate-100 disabled:text-slate-300",
            disabled: !enabled,
            onclick: move |event| onclick.call(event),
            "{label}"
        }
    }
}

#[component]
fn BlockRow(
    cycle_index: usize,
    position: usize,
    block_index: usize,
    block: BlockTemplate,
    editable: bool,
    dialog: Signal<Dialog>,
) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut dialog = dialog;

    let is_strength = block.block_type == "strength";
    let title = match (&block.exercise, is_strength) {
        (Some(name), _) if !name.is_empty() => name.clone(),
        (_, true) => "choose exercise…".to_string(),
        _ => block.block_type.clone(),
    };
    let subtitle = match block.block_type.as_str() {
        "strength" => {
            let reps = block
                .sets_reps()
                .iter()
                .map(ToString::to_string)
                .collect::<Vec<_>>()
                .join("+");
            [block.role.clone().unwrap_or_default(), reps]
                .into_iter()
                .filter(|part| !part.is_empty())
                .collect::<Vec<_>>()
                .join(" · ")
        }
        "cardio" => block
            .duration_min
            .map_or("— min".to_string(), |m| format!("{m} min")),
        _ => block.role.clone().unwrap_or_default(),
    };
    let unresolved = is_strength
        && block.exercise.as_ref().is_some_and(|name| {
            !name.is_empty()
                && state
                    .read()
                    .catalogue
                    .as_ref()
                    .is_some_and(|c| c.resolve(name).is_none())
        });

    rsx! {
        li { class: "flex items-center gap-2 rounded px-1 py-0.5 text-xs hover:bg-slate-50",
            div { class: "min-w-0 flex-1",
                p {
                    class: if title == "choose exercise…" { "truncate italic text-slate-400" } else { "truncate text-slate-700" },
                    "{title}"
                    if unresolved {
                        span {
                            class: "ml-1 text-amber-600",
                            title: "\"{block.exercise.clone().unwrap_or_default()}\" is not in the catalogue, so it cannot reach the muscle map.",
                            "⚠"
                        }
                    }
                }
                if !subtitle.is_empty() {
                    p { class: "truncate text-[11px] text-slate-400", "{subtitle}" }
                }
            }
            if editable {
                if is_strength {
                    MiniButton { label: "Pick", enabled: true,
                        onclick: move |_| dialog.set(Dialog::PickExercise(position, block_index)) }
                }
                MiniButton { label: "Edit", enabled: true,
                    onclick: move |_| dialog.set(Dialog::EditBlock(position, block_index)) }
                MiniButton { label: "✕", enabled: true,
                    onclick: move |_| {
                        if let Some(cycle) = state.write().cycle_mut(cycle_index)
                            && let Some(workout) = cycle.sessions.get_mut(position)
                            && block_index < workout.blocks.len()
                        {
                            workout.blocks.remove(block_index);
                        }
                    },
                }
            }
        }
    }
}

#[component]
fn PlanDialogs(cycle_index: usize, dialog: Signal<Dialog>) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut dialog = dialog;
    let current = *dialog.read();

    match current {
        Dialog::None => rsx! {},
        Dialog::RemoveWorkout(position) => rsx! {
            Confirm {
                title: "Remove workout".to_string(),
                body: "Nothing is written until you save cycles.json.".to_string(),
                action: "Remove".to_string(),
                oncancel: move |_| dialog.set(Dialog::None),
                onconfirm: move |_| {
                    state.write().remove_workout(cycle_index, position);
                    dialog.set(Dialog::None);
                },
            }
        },
        Dialog::Retitle(position) => rsx! { RetitleDialog { cycle_index, position, dialog } },
        Dialog::AddBlock(position) => rsx! { AddBlockDialog { cycle_index, position, dialog } },
        Dialog::EditBlock(position, block_index) => rsx! {
            EditBlockDialog { cycle_index, position, block_index, dialog }
        },
        Dialog::PickExercise(position, block_index) => rsx! {
            ExercisePicker { cycle_index, position, block_index, dialog }
        },
        Dialog::NewCycle | Dialog::CloneCycle | Dialog::EditCycle => rsx! {
            CycleDialog { cycle_index, mode: current, dialog }
        },
    }
}

/// The A1/A2 convention is picked, never typed: the letter groups workouts by
/// emphasis and the number is the day type.
#[component]
fn RetitleDialog(cycle_index: usize, position: usize, dialog: Signal<Dialog>) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut dialog = dialog;
    let existing = state
        .read()
        .cycles
        .as_ref()
        .and_then(|c| c.cycles.get(cycle_index))
        .and_then(|c| c.sessions.get(position))
        .and_then(|s| CycleDay::try_parse(&s.cycle_day));
    let mut letter = use_signal(|| existing.map_or('A', |d| d.letter));
    let mut kind = use_signal(|| existing.map_or(DayKind::Conditioning, |d| d.kind));

    rsx! {
        Modal { width: "24rem".to_string(),
            h2 { class: "mb-3 text-sm font-semibold", "Rename workout" }
            p { class: "text-[11px] font-medium uppercase tracking-wide text-slate-500", "Muscle group" }
            div { class: "mt-1 flex gap-1",
                for option in ['A', 'B', 'C', 'D', 'E', 'F'] {
                    button {
                        key: "{option}",
                        class: if *letter.read() == option {
                            "h-7 w-7 rounded bg-sky-600 text-xs text-white"
                        } else {
                            "h-7 w-7 rounded border border-slate-200 text-xs hover:bg-slate-100"
                        },
                        onclick: move |_| letter.set(option),
                        "{option}"
                    }
                }
            }
            p { class: "mt-3 text-[11px] font-medium uppercase tracking-wide text-slate-500", "Day type" }
            div { class: "mt-1 space-y-1",
                for option in [DayKind::Conditioning, DayKind::Heavy] {
                    label { key: "{option.number()}", class: "flex items-center gap-2 text-sm",
                        input {
                            r#type: "radio",
                            checked: *kind.read() == option,
                            onchange: move |_| kind.set(option),
                        }
                        span { "{option.number()} — {option.label()}" }
                    }
                }
            }
            p { class: "mt-3 text-sm text-slate-600",
                "Becomes {letter.read()}{kind.read().number()}"
            }
            DialogActions {
                confirm: "Apply".to_string(),
                oncancel: move |_| dialog.set(Dialog::None),
                onconfirm: move |_| {
                    let day = CycleDay { letter: *letter.read(), kind: *kind.read() };
                    state.write().retitle_workout(cycle_index, position, day);
                    dialog.set(Dialog::None);
                },
            }
        }
    }
}

#[component]
fn AddBlockDialog(cycle_index: usize, position: usize, dialog: Signal<Dialog>) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut dialog = dialog;
    rsx! {
        Modal { width: "18rem".to_string(),
            h2 { class: "mb-3 text-sm font-semibold", "Add block" }
            div { class: "space-y-1",
                for block_type in ["cardio", "strength", "metcon", "cooldown"] {
                    button {
                        key: "{block_type}",
                        class: "w-full rounded px-2 py-1 text-left text-sm hover:bg-slate-100",
                        onclick: move |_| {
                            if let Some(cycle) = state.write().cycle_mut(cycle_index)
                                && let Some(workout) = cycle.sessions.get_mut(position)
                            {
                                workout.blocks.push(BlockTemplate {
                                    block_type: block_type.to_string(),
                                    role: None, machine: None, duration_min: None, exercise: None,
                                    sets_reps: None, notes: None, format: None, scheme: None,
                                    exercises: None, extras: Default::default(),
                                });
                            }
                            dialog.set(Dialog::None);
                        },
                        "{block_type}"
                    }
                }
            }
        }
    }
}

/// Which fields exist depends on the block type, so nothing renders empty rows.
#[component]
fn EditBlockDialog(
    cycle_index: usize,
    position: usize,
    block_index: usize,
    dialog: Signal<Dialog>,
) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut dialog = dialog;
    let Some(block) = state
        .read()
        .cycles
        .as_ref()
        .and_then(|c| c.cycles.get(cycle_index))
        .and_then(|c| c.sessions.get(position))
        .and_then(|s| s.blocks.get(block_index))
        .cloned()
    else {
        return rsx! {};
    };

    let mut role = use_signal(|| block.role.clone().unwrap_or_default());
    let mut notes = use_signal(|| block.notes.clone().unwrap_or_default());
    let mut machine = use_signal(|| block.machine.clone().unwrap_or_default());
    let mut duration = use_signal(|| {
        block
            .duration_min
            .map(|d| d.to_string())
            .unwrap_or_default()
    });
    let mut sets_reps = use_signal(|| numbers_to_text(block.sets_reps()));
    let mut scheme = use_signal(|| {
        block
            .scheme
            .as_deref()
            .map(numbers_to_text)
            .unwrap_or_default()
    });
    let mut movements = use_signal(|| {
        block
            .exercises()
            .iter()
            .map(|e| e.name.clone())
            .collect::<Vec<_>>()
            .join(", ")
    });
    let block_type = block.block_type.clone();

    rsx! {
        Modal { width: "30rem".to_string(),
            h2 { class: "mb-3 text-sm font-semibold", "Edit {block_type} block" }
            div { class: "flex flex-wrap gap-3",
                Field {
                    label: "Role".to_string(), value: role.read().clone(),
                    placeholder: "warmup, main, accessory, grip…".to_string(), width: "14rem".to_string(),
                    oninput: move |value: String| role.set(value),
                }
                if block_type == "cardio" {
                    Field {
                        label: "Machine".to_string(), value: machine.read().clone(),
                        placeholder: String::new(), width: "10rem".to_string(),
                        oninput: move |value: String| machine.set(value),
                    }
                    Field {
                        label: "Duration (min)".to_string(), value: duration.read().clone(),
                        placeholder: String::new(), width: "7rem".to_string(),
                        oninput: move |value: String| duration.set(value),
                    }
                }
                if block_type == "strength" {
                    Field {
                        label: "Planned reps per set".to_string(), value: sets_reps.read().clone(),
                        placeholder: "6, 6, 6, 6, 6".to_string(), width: "14rem".to_string(),
                        oninput: move |value: String| sets_reps.set(value),
                    }
                }
                if block_type == "metcon" {
                    Field {
                        label: "Scheme".to_string(), value: scheme.read().clone(),
                        placeholder: "21, 15, 9".to_string(), width: "10rem".to_string(),
                        oninput: move |value: String| scheme.set(value),
                    }
                    Field {
                        label: "Movements".to_string(), value: movements.read().clone(),
                        placeholder: "трастери, бьорпі".to_string(), width: "18rem".to_string(),
                        oninput: move |value: String| movements.set(value),
                    }
                }
                Field {
                    label: "Notes".to_string(), value: notes.read().clone(),
                    placeholder: String::new(), width: "26rem".to_string(),
                    oninput: move |value: String| notes.set(value),
                }
            }
            DialogActions {
                confirm: "Apply".to_string(),
                oncancel: move |_| dialog.set(Dialog::None),
                onconfirm: move |_| {
                    if let Some(cycle) = state.write().cycle_mut(cycle_index)
                        && let Some(workout) = cycle.sessions.get_mut(position)
                        && let Some(target) = workout.blocks.get_mut(block_index)
                    {
                        target.role = optional(role.read().clone());
                        target.notes = optional(notes.read().clone());
                        match target.block_type.as_str() {
                            "cardio" => {
                                target.machine = Some(machine.read().clone());
                                target.duration_min = duration.read().parse().ok();
                            }
                            "strength" => {
                                let parsed = text_to_numbers(&sets_reps.read());
                                target.sets_reps = (!parsed.is_empty()).then_some(parsed);
                            }
                            "metcon" => {
                                let parsed = text_to_numbers(&scheme.read());
                                target.scheme = (!parsed.is_empty()).then_some(parsed);
                                // Existing movements keep their load: rebuilding
                                // from names alone would drop the weight tonnage reads.
                                let existing = target.exercises().to_vec();
                                let names: Vec<String> = movements
                                    .read()
                                    .split(',')
                                    .map(|n| n.trim().to_string())
                                    .filter(|n| !n.is_empty())
                                    .collect();
                                target.exercises = Some(names.into_iter().map(|name| {
                                    existing.iter().find(|e| e.name == name).cloned()
                                        .unwrap_or(MetconExercise { name, load: None, weight_kg: None, reps_override: None })
                                }).collect());
                            }
                            _ => {}
                        }
                    }
                    dialog.set(Dialog::None);
                },
            }
        }
    }
}

#[component]
fn ExercisePicker(
    cycle_index: usize,
    position: usize,
    block_index: usize,
    dialog: Signal<Dialog>,
) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut dialog = dialog;
    let mut query = use_signal(String::new);
    let mut creating = use_signal(|| false);
    let needle = query.read().to_lowercase();

    let matches: Vec<(String, String)> = state
        .read()
        .catalogue
        .as_ref()
        .map(|catalogue| {
            catalogue
                .exercises
                .iter()
                .filter(|exercise| {
                    needle.is_empty()
                        || exercise.name.to_lowercase().contains(&needle)
                        || exercise
                            .aliases()
                            .iter()
                            .any(|a| a.to_lowercase().contains(&needle))
                })
                .map(|exercise| (exercise.name.clone(), exercise.primary_muscles.join(", ")))
                .collect()
        })
        .unwrap_or_default();

    rsx! {
        Modal { width: "28rem".to_string(),
            h2 { class: "mb-2 text-sm font-semibold", "Choose exercise" }
            input {
                class: "w-full rounded border border-slate-300 px-2 py-1 text-sm",
                placeholder: "search name or alias",
                value: "{query}",
                oninput: move |event| query.set(event.value()),
            }
            div { class: "mt-2 max-h-72 overflow-auto",
                if matches.is_empty() {
                    p { class: "p-2 text-xs text-slate-400", "Nothing matches." }
                }
                for (name, muscles) in matches {
                    button {
                        key: "{name}",
                        class: "block w-full rounded px-2 py-1 text-left hover:bg-slate-100",
                        onclick: {
                            let chosen = name.clone();
                            move |_| {
                                if let Some(cycle) = state.write().cycle_mut(cycle_index)
                                    && let Some(workout) = cycle.sessions.get_mut(position)
                                    && let Some(block) = workout.blocks.get_mut(block_index)
                                {
                                    block.exercise = Some(chosen.clone());
                                }
                                dialog.set(Dialog::None);
                            }
                        },
                        p { class: "truncate text-sm", "{name}" }
                        p { class: "truncate text-[11px] text-slate-400", "{muscles}" }
                    }
                }
            }
            div { class: "mt-3 flex justify-between",
                button {
                    class: "rounded border border-slate-200 px-3 py-1 text-sm text-slate-700 hover:bg-slate-100 disabled:text-slate-300",
                    disabled: !state.read().can_edit_plan(),
                    onclick: move |_| creating.set(true),
                    "New exercise"
                }
                button {
                    class: "rounded px-3 py-1 text-sm text-slate-600 hover:bg-slate-100",
                    onclick: move |_| dialog.set(Dialog::None),
                    "Cancel"
                }
            }
        }
        if *creating.read() {
            NewExerciseDialog {
                oncancel: move |_| creating.set(false),
                oncreated: move |name: String| {
                    if let Some(cycle) = state.write().cycle_mut(cycle_index)
                        && let Some(workout) = cycle.sessions.get_mut(position)
                        && let Some(block) = workout.blocks.get_mut(block_index)
                    {
                        block.exercise = Some(name);
                    }
                    creating.set(false);
                    dialog.set(Dialog::None);
                },
            }
        }
    }
}

/// A new catalogue entry needs at least one primary muscle, or it would resolve
/// but contribute nothing to any map.
#[component]
fn NewExerciseDialog(
    oncancel: EventHandler<MouseEvent>,
    oncreated: EventHandler<String>,
) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut name = use_signal(String::new);
    let mut aliases = use_signal(String::new);
    let mut category = use_signal(|| ExerciseCategory::Strength);
    let mut pattern = use_signal(|| None::<MovementPattern>);
    let primary = use_signal(Vec::<String>::new);
    let secondary = use_signal(Vec::<String>::new);
    let mut error = use_signal(String::new);
    let vocabulary = state
        .read()
        .catalogue
        .as_ref()
        .map(|c| c.known_muscles())
        .unwrap_or_default();

    rsx! {
        Modal { width: "32rem".to_string(),
            h2 { class: "mb-3 text-sm font-semibold", "New exercise" }
            div { class: "flex flex-wrap gap-3",
                Field {
                    label: "Canonical name".to_string(), value: name.read().clone(),
                    placeholder: "присід фронтальний".to_string(), width: "17rem".to_string(),
                    oninput: move |value: String| name.set(value),
                }
                Field {
                    label: "Aliases".to_string(), value: aliases.read().clone(),
                    placeholder: "comma separated".to_string(), width: "11rem".to_string(),
                    oninput: move |value: String| aliases.set(value),
                }
            }
            div { class: "mt-3 flex gap-4",
                label { class: "flex flex-col gap-1",
                    span { class: "text-[11px] font-medium uppercase tracking-wide text-slate-500", "Category" }
                    select {
                        class: "rounded border border-slate-300 px-2 py-1 text-sm",
                        onchange: move |event| category.set(match event.value().as_str() {
                            "power" => ExerciseCategory::Power,
                            "speed" => ExerciseCategory::Speed,
                            "longevity" => ExerciseCategory::Longevity,
                            _ => ExerciseCategory::Strength,
                        }),
                        for option in ["strength", "power", "speed", "longevity"] {
                            option { key: "{option}", value: "{option}", "{option}" }
                        }
                    }
                }
                label { class: "flex flex-col gap-1",
                    span { class: "text-[11px] font-medium uppercase tracking-wide text-slate-500", "Pattern" }
                    select {
                        class: "rounded border border-slate-300 px-2 py-1 text-sm",
                        onchange: move |event| pattern.set(MovementPattern::ALL.into_iter()
                            .find(|p| pattern_name(*p) == event.value())),
                        option { value: "", "—" }
                        for option in MovementPattern::ALL {
                            option { key: "{pattern_name(option)}", value: "{pattern_name(option)}", "{pattern_name(option)}" }
                        }
                    }
                }
            }
            MusclePicker { title: "Primary muscles".to_string(), vocabulary: vocabulary.clone(), chosen: primary }
            MusclePicker { title: "Secondary muscles".to_string(), vocabulary, chosen: secondary }
            if !error.read().is_empty() {
                p { class: "mt-2 text-xs text-rose-600", "{error}" }
            }
            DialogActions {
                confirm: "Create".to_string(),
                oncancel: move |event| oncancel.call(event),
                onconfirm: move |_| {
                    let canonical = name.read().trim().to_string();
                    if canonical.is_empty() {
                        error.set("A name is required.".into());
                        return;
                    }
                    if state.read().catalogue.as_ref().is_some_and(|c| c.resolve(&canonical).is_some()) {
                        error.set(format!("\"{canonical}\" already resolves to an exercise."));
                        return;
                    }
                    if primary.read().is_empty() {
                        error.set("Pick at least one primary muscle.".into());
                        return;
                    }
                    let mut primaries = primary.read().clone();
                    primaries.sort();
                    let mut secondaries = secondary.read().clone();
                    secondaries.sort();
                    let alias_list: Vec<String> = aliases.read().split(',')
                        .map(|a| a.trim().to_string()).filter(|a| !a.is_empty()).collect();
                    state.write().add_exercise(Exercise {
                        name: canonical.clone(),
                        aliases: (!alias_list.is_empty()).then_some(alias_list),
                        pattern: *pattern.read(),
                        modality: None,
                        category: *category.read(),
                        primary_muscles: primaries,
                        secondary_muscles: (!secondaries.is_empty()).then_some(secondaries),
                        notes: None,
                        extras: Default::default(),
                    });
                    oncreated.call(canonical);
                },
            }
        }
    }
}

#[component]
fn MusclePicker(title: String, vocabulary: Vec<String>, chosen: Signal<Vec<String>>) -> Element {
    let mut chosen = chosen;
    rsx! {
        p { class: "mt-3 text-[11px] font-medium uppercase tracking-wide text-slate-500", "{title}" }
        div { class: "mt-1 flex flex-wrap gap-1",
            for muscle in vocabulary {
                {
                    let selected = chosen.read().contains(&muscle);
                    let token = muscle.clone();
                    rsx! {
                        button {
                            key: "{muscle}",
                            class: if selected {
                                "rounded bg-sky-600 px-1.5 py-0.5 text-[11px] text-white"
                            } else {
                                "rounded border border-slate-200 px-1.5 py-0.5 text-[11px] text-slate-600 hover:bg-slate-100"
                            },
                            onclick: move |_| {
                                let mut next = chosen.read().clone();
                                match next.iter().position(|m| m == &token) {
                                    Some(at) => { next.remove(at); }
                                    None => next.push(token.clone()),
                                }
                                chosen.set(next);
                            },
                            "{muscle}"
                        }
                    }
                }
            }
        }
    }
}

#[component]
fn CycleDialog(cycle_index: usize, mode: Dialog, dialog: Signal<Dialog>) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut dialog = dialog;
    let source = state
        .read()
        .cycles
        .as_ref()
        .and_then(|c| c.cycles.get(cycle_index))
        .cloned();
    let today = chrono::Local::now()
        .date_naive()
        .format("%Y-%m-%d")
        .to_string();

    let (title, confirm) = match mode {
        Dialog::CloneCycle => ("Clone cycle", "Create"),
        Dialog::EditCycle => ("Edit cycle", "Apply"),
        _ => ("New cycle", "Create"),
    };
    let mut id = use_signal(|| match (mode, &source) {
        (Dialog::CloneCycle, Some(cycle)) => format!("{}-copy", cycle.id),
        (Dialog::EditCycle, Some(cycle)) => cycle.id.clone(),
        _ => String::new(),
    });
    let mut name = use_signal(|| match (mode, &source) {
        (Dialog::CloneCycle, Some(cycle)) => format!("{} (copy)", cycle.name),
        (Dialog::EditCycle, Some(cycle)) => cycle.name.clone(),
        _ => String::new(),
    });
    let mut start = use_signal(|| match (mode, &source) {
        (Dialog::NewCycle, _) | (_, None) => today.clone(),
        (_, Some(cycle)) => cycle.start_date.clone(),
    });
    let mut days = use_signal(|| match (mode, &source) {
        (Dialog::NewCycle, _) | (_, None) => vec!["Tue".to_string(), "Thu".into(), "Sun".into()],
        (_, Some(cycle)) => cycle.training_days.clone(),
    });
    let mut error = use_signal(String::new);

    rsx! {
        Modal { width: "30rem".to_string(),
            h2 { class: "mb-3 text-sm font-semibold", "{title}" }
            div { class: "flex flex-wrap gap-3",
                Field {
                    label: "Name".to_string(), value: name.read().clone(),
                    placeholder: "8-session hybrid cycle".to_string(), width: "26rem".to_string(),
                    oninput: move |value: String| name.set(value),
                }
                Field {
                    label: "Id".to_string(), value: id.read().clone(),
                    placeholder: "hybrid-8".to_string(), width: "12rem".to_string(),
                    oninput: move |value: String| id.set(value),
                }
                Field {
                    label: "Start date".to_string(), value: start.read().clone(),
                    placeholder: "YYYY-MM-DD".to_string(), width: "10rem".to_string(),
                    oninput: move |value: String| start.set(value),
                }
            }
            p { class: "mt-3 text-[11px] font-medium uppercase tracking-wide text-slate-500", "Training days" }
            div { class: "mt-1 flex flex-wrap gap-1",
                for abbreviation in workout_log_core::cycles::cycle_generator::WEEKDAY_ABBREVIATIONS {
                    button {
                        key: "{abbreviation}",
                        class: if days.read().iter().any(|d| d == abbreviation) {
                            "rounded bg-sky-600 px-2 py-0.5 text-xs text-white"
                        } else {
                            "rounded border border-slate-200 px-2 py-0.5 text-xs hover:bg-slate-100"
                        },
                        onclick: move |_| {
                            let mut next = days.read().clone();
                            match next.iter().position(|d| d == abbreviation) {
                                Some(at) => { next.remove(at); }
                                None => next.push(abbreviation.to_string()),
                            }
                            // Stored in calendar order, not click order.
                            next.sort_by_key(|day| {
                                workout_log_core::cycles::cycle_generator::WEEKDAY_ABBREVIATIONS
                                    .iter().position(|a| a == day).unwrap_or(7)
                            });
                            days.set(next);
                        },
                        "{abbreviation}"
                    }
                }
            }
            if !error.read().is_empty() {
                p { class: "mt-2 text-xs text-rose-600", "{error}" }
            }
            DialogActions {
                confirm: confirm.to_string(),
                oncancel: move |_| dialog.set(Dialog::None),
                onconfirm: move |_| {
                    let (new_id, new_name) = (id.read().trim().to_string(), name.read().trim().to_string());
                    if new_id.is_empty() || new_name.is_empty() {
                        error.set("An id and a name are required.".into());
                        return;
                    }
                    if chrono::NaiveDate::parse_from_str(&start.read(), "%Y-%m-%d").is_err() {
                        error.set("Start date must be YYYY-MM-DD.".into());
                        return;
                    }
                    if days.read().is_empty() {
                        error.set("Pick at least one training day.".into());
                        return;
                    }
                    let unchanged = matches!(mode, Dialog::EditCycle)
                        && source.as_ref().is_some_and(|c| c.id == new_id);
                    if !unchanged && state.read().cycle_id_taken(&new_id) {
                        error.set(format!("The id \"{new_id}\" is already used."));
                        return;
                    }

                    let mut guard = state.write();
                    let Some(catalogue) = guard.cycles.as_mut() else { return };
                    match mode {
                        Dialog::EditCycle => {
                            if let Some(cycle) = catalogue.cycles.get_mut(cycle_index) {
                                cycle.id = new_id;
                                cycle.name = new_name;
                                cycle.start_date = start.read().clone();
                                cycle.training_days = days.read().clone();
                            }
                        }
                        Dialog::CloneCycle => {
                            if let Some(source) = source.clone() {
                                catalogue.cycles.push(Cycle {
                                    id: new_id, name: new_name,
                                    start_date: start.read().clone(),
                                    training_days: days.read().clone(),
                                    ..source
                                });
                            }
                        }
                        _ => catalogue.cycles.push(Cycle {
                            id: new_id, name: new_name,
                            training_days: days.read().clone(),
                            start_date: start.read().clone(),
                            sessions: Vec::new(),
                            extras: Default::default(),
                        }),
                    }
                    drop(guard);
                    dialog.set(Dialog::None);
                },
            }
        }
    }
}

#[component]
fn DialogActions(
    confirm: String,
    oncancel: EventHandler<MouseEvent>,
    onconfirm: EventHandler<MouseEvent>,
) -> Element {
    rsx! {
        div { class: "mt-4 flex justify-end gap-2",
            button {
                class: "rounded px-3 py-1 text-sm text-slate-600 hover:bg-slate-100",
                onclick: move |event| oncancel.call(event),
                "Cancel"
            }
            button {
                class: "rounded bg-sky-600 px-3 py-1 text-sm text-white hover:bg-sky-700",
                onclick: move |event| onconfirm.call(event),
                "{confirm}"
            }
        }
    }
}

fn optional(value: String) -> Option<String> {
    let trimmed = value.trim().to_string();
    (!trimmed.is_empty()).then_some(trimmed)
}

fn numbers_to_text(values: &[i64]) -> String {
    values
        .iter()
        .map(ToString::to_string)
        .collect::<Vec<_>>()
        .join(", ")
}

fn text_to_numbers(text: &str) -> Vec<i64> {
    text.split([',', ' ', '+'])
        .filter_map(|part| part.trim().parse().ok())
        .collect()
}
