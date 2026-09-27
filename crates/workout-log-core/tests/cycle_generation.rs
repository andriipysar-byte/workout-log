//! The real `cycles.json` generated the real `data/` — so expanding it again
//! must land on exactly the same filenames.

use std::collections::BTreeSet;
use std::fs;
use std::path::PathBuf;

use chrono::NaiveDate;
use workout_log_core::coding::encode_json;
use workout_log_core::cycles::cycle_generator::{
    self, CycleGeneratorError, WEEKDAY_ABBREVIATIONS, generate, training_dates,
};
use workout_log_core::cycles::cycle_templates;
use workout_log_core::io::session_store::SessionStore;
use workout_log_core::{CycleCatalogue, Kind};

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .find(|dir| dir.join("cycles.json").is_file())
        .expect("a repo root holding cycles.json")
        .to_path_buf()
}

fn cycles() -> CycleCatalogue {
    serde_json::from_str(&fs::read_to_string(repo_root().join("cycles.json")).expect("readable"))
        .expect("cycles.json decodes")
}

fn date(text: &str) -> NaiveDate {
    NaiveDate::parse_from_str(text, "%Y-%m-%d").expect("valid date")
}

#[test]
fn hybrid_8_expands_onto_the_files_that_exist() {
    let catalogue = cycles();
    let cycle = catalogue.by_id("hybrid-8").expect("the real cycle");
    let generated: BTreeSet<String> = generate(cycle)
        .expect("generates")
        .iter()
        .map(SessionStore::id_for)
        .collect();

    let on_disk: BTreeSet<String> = fs::read_dir(repo_root().join("data"))
        .expect("data/ exists")
        .filter_map(|e| e.ok())
        .map(|e| e.file_name().to_string_lossy().into_owned())
        .filter(|name| name.ends_with(".json"))
        .collect();

    assert_eq!(generated.len(), 8);
    assert!(
        generated.is_subset(&on_disk),
        "generated files missing from data/: {:?}",
        generated.difference(&on_disk).collect::<Vec<_>>()
    );
}

#[test]
fn planning_only_fields_never_reach_a_session_file() {
    let catalogue = cycles();
    let cycle = catalogue.by_id("hybrid-8").expect("the real cycle");
    for session in generate(cycle).expect("generates") {
        let text = SessionStore::encode(&session);
        for planning_only in ["sets_reps", "\"role\"", "weekday", "\"title\"", "\"week\""] {
            assert!(
                !text.contains(planning_only),
                "{planning_only} leaked into {}",
                SessionStore::id_for(&session)
            );
        }
        assert_eq!(session.kind, Kind::Training);
    }
}

#[test]
fn a_generated_stub_is_already_canonical() {
    let catalogue = cycles();
    let cycle = catalogue.by_id("hybrid-8").expect("the real cycle");
    for session in generate(cycle).expect("generates") {
        let once = SessionStore::encode(&session);
        let reparsed = encode_json(&serde_json::from_str(&once).expect("valid JSON"));
        assert_eq!(once, reparsed, "the encoder is not idempotent");
    }
}

#[test]
fn generation_is_deterministic() {
    let catalogue = cycles();
    let cycle = catalogue.by_id("hybrid-8").expect("the real cycle");
    assert_eq!(generate(cycle).expect("a"), generate(cycle).expect("b"));
}

#[test]
fn training_dates_start_inclusive_and_reject_nonsense_days() {
    let days = ["Tue".to_string(), "Thu".to_string(), "Sun".to_string()];
    let dates = training_dates(date("2026-07-21"), &days, 4).expect("walks");
    assert_eq!(
        dates
            .iter()
            .map(|d| cycle_generator::iso_date(*d))
            .collect::<Vec<_>>(),
        ["2026-07-21", "2026-07-23", "2026-07-26", "2026-07-28"]
    );

    let bad = training_dates(date("2026-07-21"), &["Blursday".to_string()], 1);
    assert_eq!(
        bad,
        Err(CycleGeneratorError(format!(
            "no recognised training days in Blursday (expected any of {})",
            WEEKDAY_ABBREVIATIONS.join(", ")
        )))
    );
}

#[test]
fn the_next_planned_day_continues_the_convention() {
    let catalogue = cycles();
    let codes: Vec<String> = catalogue
        .by_id("hybrid-8")
        .expect("the real cycle")
        .sessions
        .iter()
        .map(|s| s.cycle_day.clone())
        .collect();
    assert_eq!(cycle_templates::next_day(&codes).code(), "E1");
    assert_eq!(cycle_templates::next_day(&[]).code(), "A1");
    assert_eq!(
        cycle_templates::next_day(&["деload".into(), "A1".into()]).code(),
        "A2"
    );
}

#[test]
fn preview_skips_the_slots_generate_refuses() {
    // A fresh conditioning skeleton: its `explosive` and `accessory` slots have
    // no exercise chosen yet, which is exactly what the planner starts from.
    let day = workout_log_core::cycles::cycle_day::CycleDay::try_parse("A1").expect("parses");
    let template = cycle_templates::session(day, Some(1), None);

    assert!(
        cycle_generator::session_from_template(&template, date("2026-07-21"), false).is_err(),
        "generate must refuse a strength slot with no exercise"
    );

    let preview = cycle_generator::preview(&template, None);
    assert_eq!(preview.date, "2000-01-01");
    assert!(
        !preview.blocks.is_empty(),
        "preview must still yield the blocks it can build"
    );
}

#[test]
fn a_previewed_lift_with_no_sets_still_scores_one_slot() {
    let catalogue = cycles();
    let heavy = catalogue
        .by_id("hybrid-8")
        .expect("the real cycle")
        .sessions
        .iter()
        .find(|s| s.cycle_day == "A2")
        .expect("a heavy day")
        .clone();
    let preview = cycle_generator::preview(&heavy, None);
    let setless_lifts = preview
        .blocks
        .iter()
        .filter_map(|b| match b {
            workout_log_core::Block::Strength(s) => Some(s),
            _ => None,
        })
        .filter(|s| s.sets.len() == 1 && s.sets[0] == Default::default())
        .count();
    assert!(
        setless_lifts > 0,
        "a named lift with no planned reps must still reach the muscle map"
    );
}
