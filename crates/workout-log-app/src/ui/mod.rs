pub mod cycle;
pub mod plan;
pub mod session;

use dioxus::prelude::*;
use workout_log_core::analytics::muscle_activation::WeightingMode;
use workout_log_core::analytics::muscle_group::MuscleGroup;

use crate::state::{AppState, Tab};

/// The badge colour for a day whose dominant muscle group is unknown, and the
/// accent the rest of the palette is built around.
pub const UNCLASSIFIED: &str = "#2a78d6";

#[component]
pub fn App() -> Element {
    use_context_provider(|| Signal::new(AppState::start()));
    let tab = use_signal(|| Tab::List);

    rsx! {
        document::Link { rel: "stylesheet", href: asset!("/assets/tailwind.css") }
        div { class: "flex h-screen flex-col bg-slate-50 text-slate-900",
            div { class: "flex min-h-0 flex-1",
                Sidebar { tab }
                main { class: "min-w-0 flex-1 overflow-auto",
                    match *tab.read() {
                        Tab::List => rsx! { session::SessionEditor {} },
                        Tab::Cycle => rsx! { cycle::CycleView {} },
                        Tab::Plan => rsx! { plan::PlanView {} },
                    }
                }
            }
            StatusBar {}
        }
    }
}

#[component]
fn Sidebar(tab: Signal<Tab>) -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let mut confirming = use_signal(|| None::<String>);
    let mut creating = use_signal(|| false);

    let files = state.read().files.clone();
    let selection = state.read().selection.clone();
    let can_choose = state.read().can_choose_folder;
    let has_session = state.read().session.is_some();

    rsx! {
        aside { class: "flex w-60 shrink-0 flex-col border-r border-slate-200 bg-white",
            div { class: "flex gap-1 border-b border-slate-200 p-2",
                for (label, value) in [("List", Tab::List), ("Cycle", Tab::Cycle), ("Plan", Tab::Plan)] {
                    button {
                        key: "{label}",
                        class: if *tab.read() == value {
                            "flex-1 rounded px-2 py-1 text-xs font-medium bg-sky-600 text-white"
                        } else {
                            "flex-1 rounded px-2 py-1 text-xs font-medium text-slate-600 hover:bg-slate-100"
                        },
                        onclick: move |_| tab.set(value),
                        "{label}"
                    }
                }
            }

            div { class: "flex flex-wrap gap-1 border-b border-slate-200 p-2",
                ToolButton { label: "New session", onclick: move |_| creating.set(true) }
                if can_choose {
                    ToolButton {
                        label: "Folder…",
                        onclick: move |_| {
                            if let Some(folder) = crate::platform::choose_folder() {
                                state.write().adopt(folder);
                            }
                        },
                    }
                }
                ToolButton {
                    label: "Import",
                    onclick: move |_| {
                        let files = crate::platform::import_files();
                        state.write().import(files);
                    },
                }
                ToolButton {
                    label: "Export",
                    onclick: move |_| state.write().export(),
                }
                ToolButton { label: "Reload", onclick: move |_| state.write().refresh() }
                if has_session {
                    ToolButton { label: "Save", onclick: move |_| state.write().save() }
                }
            }

            nav { class: "min-h-0 flex-1 overflow-auto p-1",
                if files.is_empty() {
                    p { class: "p-3 text-xs text-slate-400", "No sessions here yet." }
                }
                for id in files {
                    {
                        let selected = selection.as_deref() == Some(id.as_str());
                        let label = id.trim_end_matches(".json").to_string();
                        let open_id = id.clone();
                        let delete_id = id.clone();
                        rsx! {
                            div {
                                key: "{id}",
                                class: if selected { "group flex items-center rounded bg-sky-50" } else { "group flex items-center rounded hover:bg-slate-50" },
                                button {
                                    class: "flex-1 truncate px-2 py-1 text-left text-xs",
                                    onclick: move |_| {
                                        state.write().open(&open_id);
                                        tab.set(Tab::List);
                                    },
                                    "{label}"
                                }
                                button {
                                    class: "invisible px-2 text-xs text-slate-400 group-hover:visible hover:text-rose-600",
                                    onclick: move |_| confirming.set(Some(delete_id.clone())),
                                    "✕"
                                }
                            }
                        }
                    }
                }
            }
        }

        if let Some(id) = confirming.read().clone() {
            Confirm {
                title: "Delete session".to_string(),
                body: format!("Delete {id}? This removes the file."),
                action: "Delete".to_string(),
                oncancel: move |_| confirming.set(None),
                onconfirm: move |_| {
                    state.write().delete(&id);
                    confirming.set(None);
                },
            }
        }
        if *creating.read() {
            session::NewSessionDialog { onclose: move |_| creating.set(false) }
        }
    }
}

#[component]
pub fn ToolButton(label: String, onclick: EventHandler<MouseEvent>) -> Element {
    rsx! {
        button {
            class: "rounded border border-slate-200 px-2 py-1 text-xs text-slate-700 hover:bg-slate-100",
            onclick: move |event| onclick.call(event),
            "{label}"
        }
    }
}

#[component]
fn StatusBar() -> Element {
    let state = use_context::<Signal<AppState>>();
    let status = state.read().status.clone();
    let folder = state.read().folder_label();
    let working_copy = state.read().is_working_copy;

    rsx! {
        footer { class: "flex items-center gap-2 border-t border-slate-200 bg-slate-100 px-3 py-1 text-xs text-slate-600",
            span { class: "truncate",
                if status.is_empty() { "{folder}" } else { "{status}" }
            }
            if working_copy {
                span {
                    class: "rounded bg-amber-100 px-1.5 py-0.5 text-amber-800",
                    title: "The browser holds a working copy (ADR-007); export to write back to your folder.",
                    "working copy"
                }
            }
        }
    }
}

/// The map arrives already coloured from the core, so this only has to put the
/// markup on the page — which is also what makes per-muscle hover possible.
#[component]
pub fn MuscleMap(svg: Option<String>, reason: String, height: String) -> Element {
    match svg {
        Some(svg) => rsx! {
            div {
                class: "flex justify-center rounded-lg bg-white p-2 [&_svg]:h-full [&_svg]:w-auto",
                style: "height: {height}",
                dangerous_inner_html: "{svg}",
            }
        },
        None => rsx! {
            p { class: "rounded-lg bg-slate-100 p-3 text-xs text-slate-500", "{reason}" }
        },
    }
}

#[component]
pub fn WeightingPicker() -> Element {
    let mut state = use_context::<Signal<AppState>>();
    let current = state.read().mode;
    rsx! {
        div {
            class: "flex gap-1",
            title: "How each exercise is weighted into the map",
            for mode in WeightingMode::ALL {
                button {
                    key: "{mode.wire()}",
                    class: if mode == current {
                        "rounded border border-sky-600 bg-sky-600 px-2 py-0.5 text-xs text-white"
                    } else {
                        "rounded border border-slate-200 px-2 py-0.5 text-xs text-slate-600 hover:bg-slate-100"
                    },
                    onclick: move |_| state.write().set_mode(mode),
                    "{mode.label()}"
                }
            }
        }
    }
}

#[component]
pub fn GroupLegend() -> Element {
    rsx! {
        div { class: "flex flex-wrap gap-2",
            for group in MuscleGroup::ALL {
                div { key: "{group.name()}", class: "flex items-center gap-1",
                    span {
                        class: "inline-block h-2.5 w-2.5 rounded-sm",
                        style: "background: {group.hex()}",
                    }
                    span { class: "text-xs text-slate-600", "{group.label()}" }
                }
            }
        }
    }
}

/// A wash of every group an exercise touches, so a row reads as its emphasis
/// before you read the name.
pub fn group_gradient(groups: &[MuscleGroup], alpha: f32) -> String {
    match groups.len() {
        0 => "transparent".into(),
        1 => format!(
            "linear-gradient(135deg, {}, {})",
            rgba(groups[0], alpha * 1.4),
            rgba(groups[0], alpha * 0.5)
        ),
        _ => format!(
            "linear-gradient(135deg, {})",
            groups
                .iter()
                .map(|g| rgba(*g, alpha))
                .collect::<Vec<_>>()
                .join(", ")
        ),
    }
}

fn rgba(group: MuscleGroup, alpha: f32) -> String {
    let hex = group.hex().trim_start_matches('#');
    let channel = |range: std::ops::Range<usize>| u8::from_str_radix(&hex[range], 16).unwrap_or(0);
    format!(
        "rgba({}, {}, {}, {alpha})",
        channel(0..2),
        channel(2..4),
        channel(4..6)
    )
}

#[component]
pub fn Confirm(
    title: String,
    body: String,
    action: String,
    oncancel: EventHandler<MouseEvent>,
    onconfirm: EventHandler<MouseEvent>,
) -> Element {
    rsx! {
        Modal { width: "24rem".to_string(),
            h2 { class: "text-sm font-semibold", "{title}" }
            p { class: "mt-2 text-sm text-slate-600", "{body}" }
            div { class: "mt-4 flex justify-end gap-2",
                button {
                    class: "rounded px-3 py-1 text-sm text-slate-600 hover:bg-slate-100",
                    onclick: move |event| oncancel.call(event),
                    "Cancel"
                }
                button {
                    class: "rounded bg-rose-600 px-3 py-1 text-sm text-white hover:bg-rose-700",
                    onclick: move |event| onconfirm.call(event),
                    "{action}"
                }
            }
        }
    }
}

#[component]
pub fn Modal(width: String, children: Element) -> Element {
    rsx! {
        div { class: "fixed inset-0 z-50 flex items-center justify-center bg-slate-900/40 p-4",
            div {
                class: "max-h-[85vh] w-full overflow-auto rounded-lg bg-white p-4 shadow-xl",
                style: "max-width: {width}",
                {children}
            }
        }
    }
}

#[component]
pub fn Field(
    label: String,
    value: String,
    placeholder: String,
    width: String,
    oninput: EventHandler<String>,
) -> Element {
    rsx! {
        label { class: "flex flex-col gap-1", style: "width: {width}",
            span { class: "text-[11px] font-medium uppercase tracking-wide text-slate-500", "{label}" }
            input {
                class: "rounded border border-slate-300 px-2 py-1 text-sm focus:border-sky-500 focus:outline-none",
                value: "{value}",
                placeholder: "{placeholder}",
                oninput: move |event| oninput.call(event.value()),
            }
        }
    }
}
