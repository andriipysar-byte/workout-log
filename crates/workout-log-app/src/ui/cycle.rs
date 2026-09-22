use chrono::{Datelike, NaiveDate};
use dioxus::prelude::*;

use crate::state::AppState;
use crate::ui::{GroupLegend, MuscleMap, UNCLASSIFIED, WeightingPicker, group_gradient};

#[component]
pub fn CycleView() -> Element {
    let state = use_context::<Signal<AppState>>();
    let svg = state.read().cycle_map();

    rsx! {
        div { class: "space-y-5 p-5",
            CycleTable {}
            hr { class: "border-slate-200" }
            div { class: "flex flex-wrap gap-6",
                div { class: "min-w-[20rem] flex-1",
                    Calendar {}
                }
                section { class: "w-80 space-y-2",
                    div { class: "flex items-center justify-between",
                        h2 { class: "text-sm font-semibold", "Cycle muscle map" }
                    }
                    WeightingPicker {}
                    MuscleMap {
                        svg,
                        reason: "Muscle map unavailable (no sessions or catalogue missing).".to_string(),
                        height: "16rem".to_string(),
                    }
                }
            }
        }
    }
}

/// Exercises down the side, cycle days across the top. Ukrainian names overflow
/// a sane column, so the cell truncates and carries the full name as a tooltip.
#[component]
fn CycleTable() -> Element {
    let state = use_context::<Signal<AppState>>();
    let matrix = state.read().cycle.clone();

    if matrix.exercises.is_empty() {
        return rsx! {
            p { class: "text-sm text-slate-400", "No sessions in this folder." }
        };
    }

    rsx! {
        div { class: "max-h-[45vh] overflow-auto rounded-lg border border-slate-200 bg-white",
            table { class: "w-full border-collapse text-xs",
                thead { class: "sticky top-0 bg-slate-100",
                    tr {
                        th { class: "w-60 px-2 py-1 text-left font-semibold", "Exercise" }
                        for day in matrix.days.iter() {
                            th { key: "{day}", class: "w-12 px-1 py-1 font-semibold", "{day}" }
                        }
                    }
                }
                tbody {
                    for (row, exercise) in matrix.exercises.iter().enumerate() {
                        tr { key: "{exercise}", class: "border-t border-slate-100",
                            td {
                                class: "max-w-60 truncate px-2 py-1",
                                title: "{exercise}",
                                style: "background: {group_gradient(&matrix.groups[row], 0.22)}",
                                "{exercise}"
                            }
                            for (column, present) in matrix.cells[row].iter().enumerate() {
                                td {
                                    key: "{column}",
                                    class: "px-1 py-1 text-center",
                                    style: "background: {group_gradient(&matrix.groups[row], 0.16)}",
                                    if *present {
                                        span {
                                            class: "inline-block h-2 w-2 rounded-full",
                                            style: "background: {matrix.groups[row].first().map_or(UNCLASSIFIED, |g| g.hex())}",
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/// Opens on the month of the most recent session, which is the one you are
/// almost always looking for.
#[component]
fn Calendar() -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let calendar = state.read().calendar.clone();
    let anchor_default = calendar
        .keys()
        .max()
        .and_then(|date| NaiveDate::parse_from_str(date, "%Y-%m-%d").ok())
        .unwrap_or_else(|| chrono::Local::now().date_naive());
    let mut anchor = use_signal(|| anchor_default.with_day(1).unwrap_or(anchor_default));

    let month = *anchor.read();
    let first = month.with_day(1).unwrap_or(month);
    let leading = first.weekday().num_days_from_monday() as usize;
    let days_in_month = days_in(month);
    let selection = state.read().selection.clone();

    rsx! {
        section { class: "space-y-2",
            div { class: "flex items-center justify-between",
                button {
                    class: "rounded px-2 py-0.5 text-sm text-slate-500 hover:bg-slate-100",
                    onclick: move |_| { let current = *anchor.read(); anchor.set(shift(current, -1)); },
                    "‹"
                }
                h2 { class: "text-sm font-semibold", "{first.format(\"%B %Y\")}" }
                button {
                    class: "rounded px-2 py-0.5 text-sm text-slate-500 hover:bg-slate-100",
                    onclick: move |_| { let current = *anchor.read(); anchor.set(shift(current, 1)); },
                    "›"
                }
            }
            div { class: "grid grid-cols-7 gap-1",
                for label in ["M", "T", "W", "T", "F", "S", "S"] {
                    div { class: "py-1 text-center text-[11px] font-medium text-slate-400", "{label}" }
                }
                for _ in 0..leading {
                    div {}
                }
                for day in 1..=days_in_month {
                    {
                        let date = first.with_day(day).expect("a valid day");
                        let key = date.format("%Y-%m-%d").to_string();
                        let info = calendar.get(&key).cloned();
                        let logged = info.is_some();
                        let target = info.as_ref().map(|i| i.id.clone());
                        let badge = info.as_ref().map(|i| (i.cycle_day.clone(), i.group.map_or(UNCLASSIFIED, |g| g.hex())));
                        let selected = target.as_deref() == selection.as_deref() && logged;
                        rsx! {
                            button {
                                key: "{key}",
                                class: if selected {
                                    "flex aspect-square flex-col items-center justify-center rounded border-2 border-sky-500 bg-sky-50 p-1"
                                } else {
                                    "flex aspect-square flex-col items-center justify-center rounded border border-transparent p-1 hover:bg-slate-100"
                                },
                                disabled: !logged,
                                onclick: move |_| {
                                    if let Some(id) = target.clone() {
                                        state.write().open(&id);
                                    }
                                },
                                span {
                                    class: if logged { "text-[11px] text-slate-700" } else { "text-[11px] text-slate-300" },
                                    "{day}"
                                }
                                if let Some((code, colour)) = badge {
                                    span {
                                        class: "mt-0.5 rounded px-1 text-[9px] font-bold text-white",
                                        style: "background: {colour}",
                                        "{code}"
                                    }
                                }
                            }
                        }
                    }
                }
            }
            GroupLegend {}
        }
    }
}

fn days_in(month: NaiveDate) -> u32 {
    let (year, number) = (month.year(), month.month());
    let next = if number == 12 {
        NaiveDate::from_ymd_opt(year + 1, 1, 1)
    } else {
        NaiveDate::from_ymd_opt(year, number + 1, 1)
    };
    next.and_then(|n| n.pred_opt())
        .map_or(28, |last| last.day())
}

fn shift(month: NaiveDate, by: i32) -> NaiveDate {
    let total = month.year() * 12 + month.month0() as i32 + by;
    NaiveDate::from_ymd_opt(total.div_euclid(12), total.rem_euclid(12) as u32 + 1, 1)
        .unwrap_or(month)
}
