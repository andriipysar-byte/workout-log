use dioxus::prelude::*;
use workout_log_core::Session;
use workout_log_core::cycles::cycle_generator;
use workout_log_core::io::session_store::SessionStore;
use workout_log_core::models::block::Block;
use workout_log_core::models::metcon::MetconBlock;
use workout_log_core::models::session::Kind;
use workout_log_core::models::work_set::WorkSet;
use workout_log_core::parsing::notation::parse_strength_sets;

use crate::state::AppState;
use crate::ui::{Field, Modal, MuscleMap, WeightingPicker};

/// The canonical one-line rendering of a set, the same in the editor, the
/// preview and the summary line.
pub fn set_summary(set: &WorkSet) -> String {
    if let Some(cluster) = &set.cluster {
        let chain = cluster
            .iter()
            .map(ToString::to_string)
            .collect::<Vec<_>>()
            .join("+");
        return match set.total_reps {
            Some(total) => format!("{chain} ({total})"),
            None => chain,
        };
    }
    if let Some(seconds) = set.duration_sec {
        return format!("{}c", trim(seconds));
    }
    let reps = set.reps.map(|r| format!("{r}×")).unwrap_or_default();
    let weight = set.weight_kg.map_or_else(|| "bw".to_string(), trim);
    let backoff = if set.is_backoff.unwrap_or(false) {
        "*"
    } else {
        ""
    };
    format!("{reps}{weight}{backoff}")
}

fn trim(value: f64) -> String {
    if value.fract() == 0.0 {
        format!("{}", value as i64)
    } else {
        format!("{value}")
    }
}

#[component]
pub fn SessionEditor() -> Element {
    let state = use_context::<Signal<AppState>>();
    let Some(session) = state.read().session.clone() else {
        return rsx! {
            div { class: "flex h-full items-center justify-center text-sm text-slate-400",
                "Select a session to edit"
            }
        };
    };

    rsx! {
        div { class: "space-y-5 p-5",
            Header { session: session.clone() }
            hr { class: "border-slate-200" }
            div { class: "space-y-3",
                for (index, block) in session.blocks.iter().enumerate() {
                    BlockCard { key: "{index}", index, block: block.clone() }
                }
            }
            hr { class: "border-slate-200" }
            DayMuscleMap {}
        }
    }
}

#[component]
fn Header(session: Session) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    rsx! {
        div { class: "flex flex-wrap gap-4",
            Field {
                label: "Date".to_string(), value: session.date.clone(),
                placeholder: "YYYY-MM-DD".to_string(), width: "9rem".to_string(),
                oninput: move |value: String| {
                    if let Some(s) = state.write().session.as_mut() { s.date = value; }
                },
            }
            Field {
                label: "Cycle day".to_string(), value: session.cycle_day.clone(),
                placeholder: "A1".to_string(), width: "6rem".to_string(),
                oninput: move |value: String| {
                    if let Some(s) = state.write().session.as_mut() { s.cycle_day = value; }
                },
            }
            Field {
                label: "Start".to_string(),
                value: session.start_time.clone().unwrap_or_default(),
                placeholder: "H:MM".to_string(), width: "6rem".to_string(),
                oninput: move |value: String| {
                    if let Some(s) = state.write().session.as_mut() {
                        s.start_time = (!value.is_empty()).then_some(value);
                    }
                },
            }
            label { class: "flex w-32 flex-col gap-1",
                span { class: "text-[11px] font-medium uppercase tracking-wide text-slate-500", "Kind" }
                select {
                    class: "rounded border border-slate-300 px-2 py-1 text-sm",
                    onchange: move |event| {
                        let kind = match event.value().as_str() {
                            "deload" => Kind::Deload,
                            "retest" => Kind::Retest,
                            _ => Kind::Training,
                        };
                        if let Some(s) = state.write().session.as_mut() { s.kind = kind; }
                    },
                    for (value, label) in [("training", "training"), ("deload", "deload"), ("retest", "retest")] {
                        option {
                            key: "{value}", value: "{value}",
                            selected: workout_log_core::analytics::session_metrics::kind_name(session.kind) == value,
                            "{label}"
                        }
                    }
                }
            }
            Field {
                label: "Bodyweight".to_string(),
                value: session.bodyweight_kg.map(trim).unwrap_or_default(),
                placeholder: "kg".to_string(), width: "6rem".to_string(),
                oninput: move |value: String| {
                    let parsed = value.parse::<f64>().ok();
                    if let Some(s) = state.write().session.as_mut() { s.bodyweight_kg = parsed; }
                },
            }
            Field {
                label: "Notes".to_string(),
                value: session.notes.clone().unwrap_or_default(),
                placeholder: String::new(), width: "18rem".to_string(),
                oninput: move |value: String| {
                    if let Some(s) = state.write().session.as_mut() {
                        s.notes = (!value.is_empty()).then_some(value);
                    }
                },
            }
        }
    }
}

#[component]
fn DayMuscleMap() -> Element {
    let state = use_context::<Signal<AppState>>();
    let svg = state.read().day_map();
    rsx! {
        section { class: "space-y-2",
            div { class: "flex items-center justify-between",
                h2 { class: "text-sm font-semibold", "Muscle map" }
                WeightingPicker {}
            }
            MuscleMap {
                svg,
                reason: "Muscle map unavailable (catalogue or template not found).".to_string(),
                height: "18rem".to_string(),
            }
        }
    }
}

#[component]
fn BlockCard(index: usize, block: Block) -> Element {
    rsx! {
        div { class: "rounded-lg border border-slate-200 bg-white p-3",
            match block {
                Block::Cardio(cardio) => rsx! { CardioEditor { index, machine: cardio.machine.clone(), duration: cardio.duration_min, distance: cardio.distance_m, end: cardio.end_time.clone() } },
                Block::Strength(strength) => rsx! { StrengthEditor { index, exercise: strength.exercise.clone(), sets: strength.sets.clone(), notes: strength.notes.clone() } },
                Block::Metcon(metcon) => rsx! { MetconCard { metcon: metcon.clone() } },
                Block::Cooldown(cooldown) => rsx! { CooldownEditor { index, end: cooldown.end_time.clone(), notes: cooldown.notes.clone() } },
            }
        }
    }
}

fn with_block<F: FnOnce(&mut Block)>(state: &mut Signal<AppState>, index: usize, edit: F) {
    let mut guard = state.write();
    if let Some(session) = guard.session.as_mut()
        && let Some(block) = session.blocks.get_mut(index)
    {
        edit(block);
    }
}

#[component]
fn CardioEditor(
    index: usize,
    machine: String,
    duration: Option<f64>,
    distance: Option<f64>,
    end: Option<String>,
) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    rsx! {
        h3 { class: "mb-2 text-sm font-semibold text-sky-700", "Cardio" }
        div { class: "flex flex-wrap gap-3",
            Field {
                label: "Machine".to_string(), value: machine, placeholder: String::new(), width: "12rem".to_string(),
                oninput: move |value: String| with_block(&mut state, index, |b| {
                    if let Block::Cardio(c) = b { c.machine = value; }
                }),
            }
            Field {
                label: "Min".to_string(), value: duration.map(trim).unwrap_or_default(),
                placeholder: String::new(), width: "5rem".to_string(),
                oninput: move |value: String| with_block(&mut state, index, |b| {
                    if let Block::Cardio(c) = b { c.duration_min = value.parse().ok(); }
                }),
            }
            Field {
                label: "Distance m".to_string(), value: distance.map(trim).unwrap_or_default(),
                placeholder: String::new(), width: "7rem".to_string(),
                oninput: move |value: String| with_block(&mut state, index, |b| {
                    if let Block::Cardio(c) = b { c.distance_m = value.parse().ok(); }
                }),
            }
            Field {
                label: "End".to_string(), value: end.unwrap_or_default(),
                placeholder: String::new(), width: "5rem".to_string(),
                oninput: move |value: String| with_block(&mut state, index, |b| {
                    if let Block::Cardio(c) = b { c.end_time = (!value.is_empty()).then_some(value); }
                }),
            }
        }
    }
}

#[component]
fn CooldownEditor(index: usize, end: Option<String>, notes: Option<String>) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    rsx! {
        h3 { class: "mb-2 text-sm font-semibold text-sky-700", "Cooldown" }
        div { class: "flex flex-wrap gap-3",
            Field {
                label: "End".to_string(), value: end.unwrap_or_default(),
                placeholder: String::new(), width: "6rem".to_string(),
                oninput: move |value: String| with_block(&mut state, index, |b| {
                    if let Block::Cooldown(c) = b { c.end_time = (!value.is_empty()).then_some(value); }
                }),
            }
            Field {
                label: "Notes".to_string(), value: notes.unwrap_or_default(),
                placeholder: String::new(), width: "24rem".to_string(),
                oninput: move |value: String| with_block(&mut state, index, |b| {
                    if let Block::Cooldown(c) = b { c.notes = (!value.is_empty()).then_some(value); }
                }),
            }
        }
    }
}

#[component]
fn StrengthEditor(
    index: usize,
    exercise: String,
    sets: Vec<WorkSet>,
    notes: Option<String>,
) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut draft = use_signal(String::new);
    let mut show_map = use_signal(|| false);

    let typed = draft.read().clone();
    let parsed = parse_strength_sets(&typed);
    let summary = if sets.is_empty() {
        "no sets yet".to_string()
    } else {
        sets.iter().map(set_summary).collect::<Vec<_>>().join("   ")
    };
    let preview = if parsed.sets.is_empty() {
        "→ —".to_string()
    } else {
        format!(
            "→ {}",
            parsed
                .sets
                .iter()
                .map(set_summary)
                .collect::<Vec<_>>()
                .join("  ")
        )
    };
    let can_apply = !parsed.sets.is_empty();
    let apply_sets = parsed.sets.clone();
    let name = exercise.clone();

    rsx! {
        h3 { class: "text-sm font-semibold text-sky-700", "{exercise}" }
        if let Some(notes) = notes {
            p { class: "mt-1 text-xs text-slate-500", "{notes}" }
        }
        p { class: "mt-2 font-mono text-sm text-slate-700", "{summary}" }

        div { class: "mt-2 flex gap-2",
            input {
                class: "flex-1 rounded border border-slate-300 px-2 py-1 font-mono text-sm focus:border-sky-500 focus:outline-none",
                placeholder: "6 × [70, 80, 90, 100, 110]",
                value: "{typed}",
                oninput: move |event| draft.set(event.value()),
            }
            button {
                class: if can_apply {
                    "rounded bg-sky-600 px-3 py-1 text-sm text-white hover:bg-sky-700"
                } else {
                    "rounded bg-slate-200 px-3 py-1 text-sm text-slate-400"
                },
                disabled: !can_apply,
                onclick: move |_| {
                    let replacement = apply_sets.clone();
                    with_block(&mut state, index, |block| {
                        if let Block::Strength(s) = block { s.sets = replacement; }
                    });
                    draft.set(String::new());
                },
                "Apply"
            }
        }

        if !typed.is_empty() {
            div { class: "mt-1 space-y-0.5",
                p {
                    class: if can_apply { "font-mono text-xs text-emerald-700" } else { "font-mono text-xs text-rose-600" },
                    "{preview}"
                }
                for warning in parsed.warnings.iter() {
                    p { key: "{warning}", class: "text-xs text-amber-700", "⚠ {warning}" }
                }
            }
        }

        button {
            class: "mt-2 text-xs text-slate-500 underline",
            onclick: move |_| {
                let next = !*show_map.read();
                show_map.set(next);
            },
            if *show_map.read() { "Hide muscle map" } else { "Muscle map" }
        }
        if *show_map.read() {
            div { class: "mt-2",
                MuscleMap {
                    svg: state.read().exercise_map(&name),
                    reason: format!("\"{name}\" not in catalogue — no map."),
                    height: "14rem".to_string(),
                }
            }
        }
    }
}

/// Read-only by design: rounds and splits are transcribed from paper, and an
/// editor for them is out of scope.
#[component]
fn MetconCard(metcon: MetconBlock) -> Element {
    rsx! {
        h3 { class: "mb-2 text-sm font-semibold text-orange-600", "Metcon" }
        MetconTable { metcon: metcon.clone() }
        if let Some(rounds) = &metcon.rounds {
            p { class: "mt-2 text-xs text-slate-500", "rounds: {rounds.len()}" }
        }
        if let Some(notes) = &metcon.notes {
            p { class: "mt-1 text-xs text-slate-500", "{notes}" }
        }
    }
}

/// The whiteboard view: one row per movement, one column per round — a
/// movement's own `reps_override` beats the workout scheme, which is why rounds
/// are columns rather than rows.
#[component]
pub fn MetconTable(metcon: MetconBlock) -> Element {
    let scheme = metcon.scheme.clone();
    let rounds = metcon
        .exercises
        .iter()
        .map(|e| e.scheme_within(scheme.as_deref()).len())
        .max()
        .unwrap_or(0);
    let format = metcon
        .format
        .map(|f| workout_log_core::analytics::session_metrics::format_wire(f).to_string())
        .unwrap_or_else(|| "—".into());

    if metcon.exercises.is_empty() {
        return rsx! { p { class: "text-xs italic text-slate-400", "no movements yet" } };
    }

    rsx! {
        p { class: "mb-1 text-xs text-slate-500", "wod: {format}" }
        table { class: "text-sm",
            tbody {
                for exercise in metcon.exercises.iter() {
                    tr { key: "{exercise.name}",
                        td { class: "py-0.5 pr-3",
                            span { "{exercise.name}" }
                            if let Some(prescription) = exercise.prescription() {
                                span { class: "text-slate-400", "  ({prescription})" }
                            }
                        }
                        for round in 0..rounds {
                            td { key: "{round}", class: "w-8 py-0.5 text-right tabular-nums text-slate-600",
                                {exercise.scheme_within(scheme.as_deref()).get(round).map(|r| r.to_string()).unwrap_or_default()}
                            }
                        }
                    }
                }
            }
        }
    }
}

#[component]
pub fn NewSessionDialog(onclose: EventHandler<MouseEvent>) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let today = chrono::Local::now().date_naive();
    let mut date = use_signal(|| cycle_generator::iso_date(today));
    let mut cycle_day = use_signal(String::new);
    let mut template = use_signal(|| None::<usize>);
    let mut error = use_signal(String::new);

    let cycles = state.read().cycles.clone();
    let templates: Vec<(usize, String)> = cycles
        .as_ref()
        .and_then(|c| c.cycles.first())
        .map(|cycle| {
            cycle
                .sessions
                .iter()
                .enumerate()
                .map(|(index, session)| {
                    (
                        index,
                        format!(
                            "{} · {}",
                            session.cycle_day,
                            session.title.clone().unwrap_or_else(|| "session".into())
                        ),
                    )
                })
                .collect()
        })
        .unwrap_or_default();

    rsx! {
        Modal { width: "26rem".to_string(),
            h2 { class: "mb-3 text-sm font-semibold", "New session" }
            div { class: "flex flex-wrap gap-3",
                Field {
                    label: "Date".to_string(), value: date.read().clone(),
                    placeholder: "YYYY-MM-DD".to_string(), width: "10rem".to_string(),
                    oninput: move |value: String| date.set(value),
                }
                Field {
                    label: "Cycle day".to_string(), value: cycle_day.read().clone(),
                    placeholder: "A1".to_string(), width: "7rem".to_string(),
                    oninput: move |value: String| cycle_day.set(value),
                }
            }
            if !templates.is_empty() {
                label { class: "mt-3 flex flex-col gap-1",
                    span { class: "text-[11px] font-medium uppercase tracking-wide text-slate-500", "Template" }
                    select {
                        class: "rounded border border-slate-300 px-2 py-1 text-sm",
                        onchange: move |event| {
                            let picked = event.value().parse::<usize>().ok();
                            template.set(picked);
                            // Picking a template also moves the header to the slot it plans.
                            if let (Some(index), Some(cycles)) = (picked, &cycles)
                                && let Some(cycle) = cycles.cycles.first()
                                && let Some(session) = cycle.sessions.get(index)
                            {
                                cycle_day.set(session.cycle_day.clone());
                            }
                        },
                        option { value: "", "Blank session" }
                        for (index, label) in templates.iter() {
                            option { key: "{index}", value: "{index}", "{label}" }
                        }
                    }
                }
            }
            if !error.read().is_empty() {
                p { class: "mt-2 text-xs text-rose-600", "{error}" }
            }
            div { class: "mt-4 flex justify-end gap-2",
                button {
                    class: "rounded px-3 py-1 text-sm text-slate-600 hover:bg-slate-100",
                    onclick: move |event| onclose.call(event),
                    "Cancel"
                }
                button {
                    class: "rounded bg-sky-600 px-3 py-1 text-sm text-white hover:bg-sky-700",
                    onclick: move |event| {
                        let on = date.read().clone();
                        let day = cycle_day.read().trim().to_string();
                        let Ok(parsed) = chrono::NaiveDate::parse_from_str(&on, "%Y-%m-%d") else {
                            error.set("Date must be YYYY-MM-DD.".into());
                            return;
                        };
                        if day.is_empty() {
                            error.set("Cycle day is required.".into());
                            return;
                        }
                        let built = match *template.read() {
                            Some(index) => {
                                let guard = state.read();
                                let found = guard.cycles.as_ref()
                                    .and_then(|c| c.cycles.first())
                                    .and_then(|c| c.sessions.get(index))
                                    .cloned();
                                drop(guard);
                                match found {
                                    Some(source) => match cycle_generator::session_from_template(&source, parsed, false) {
                                        Ok(mut session) => { session.cycle_day = day.clone(); Ok(session) }
                                        Err(failure) => Err(failure.to_string()),
                                    },
                                    None => Err("that template is gone".to_string()),
                                }
                            }
                            None => Ok(Session {
                                date: on.clone(), cycle_day: day.clone(), start_time: None,
                                kind: Kind::Training, bodyweight_kg: None, notes: None, blocks: Vec::new(),
                            }),
                        };
                        match built {
                            Ok(session) => {
                                let id = SessionStore::id_for(&session);
                                if state.read().exists(&id) {
                                    error.set(format!("{id} is already in this folder."));
                                    return;
                                }
                                state.write().create(session);
                                onclose.call(event);
                            }
                            Err(message) => error.set(message),
                        }
                    },
                    "Create"
                }
            }
        }
    }
}
