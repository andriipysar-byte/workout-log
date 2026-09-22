//! The real catalogue and the real map template are the spec — a fixture copy
//! would only test the copy.

use std::collections::BTreeMap;
use std::fs;
use std::path::PathBuf;

use workout_log_core::analytics::muscle_activation::{MuscleActivation, WeightingMode};
use workout_log_core::analytics::muscle_group::MuscleGroup;
use workout_log_core::analytics::muscle_map_svg::{HIGH_COLOR, ZERO_COLOR, colorize};
use workout_log_core::{Catalogue, Session};

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .find(|dir| dir.join("cycles.json").is_file())
        .expect("a repo root holding cycles.json")
        .to_path_buf()
}

fn catalogue() -> Catalogue {
    serde_json::from_str(&fs::read_to_string(repo_root().join("exercises.json")).expect("readable"))
        .expect("exercises.json decodes")
}

fn session(name: &str) -> Session {
    serde_json::from_str(
        &fs::read_to_string(repo_root().join("data").join(name)).expect("readable"),
    )
    .expect("session decodes")
}

fn peak(scores: &BTreeMap<String, f64>) -> Option<(&str, f64)> {
    scores.iter().fold(
        None,
        |best: Option<(&str, f64)>, (name, &score)| match best {
            Some((_, high)) if high >= score => best,
            _ => Some((name.as_str(), score)),
        },
    )
}

#[test]
fn every_catalogue_muscle_maps_to_a_group() {
    for muscle in catalogue().known_muscles() {
        assert!(
            MuscleGroup::of(&muscle).is_some(),
            "{muscle} maps to no group"
        );
    }
}

#[test]
fn every_mode_normalises_to_a_peak_of_one() {
    let (catalogue, session) = (catalogue(), session("2026-06-23_C2.json"));
    let activation = MuscleActivation::default();
    for mode in WeightingMode::ALL {
        let scores = activation.for_session(&session, &catalogue, mode);
        assert!(!scores.is_empty(), "{} produced no scores", mode.wire());
        let (_, high) = peak(&scores).expect("a peak");
        assert!(
            (high - 1.0).abs() < 1e-9,
            "{} peaked at {high}, not 1.0",
            mode.wire()
        );
    }
}

#[test]
fn the_three_weightings_deliberately_disagree() {
    let (catalogue, session) = (catalogue(), session("2026-06-23_C2.json"));
    let activation = MuscleActivation::default();
    let hottest = |mode| {
        peak(&activation.for_session(&session, &catalogue, mode))
            .expect("a peak")
            .0
            .to_string()
    };
    assert_eq!(hottest(WeightingMode::SetCount), "forearms");
    let by_reps = hottest(WeightingMode::RepVolume);
    assert!(
        by_reps == "quads" || by_reps == "glutes",
        "rep volume peaked at {by_reps}"
    );
    assert_ne!(hottest(WeightingMode::SetCount), by_reps);
}

#[test]
fn an_exercise_scores_primaries_at_one_and_secondaries_at_a_half() {
    let catalogue = catalogue();
    let squat = catalogue.resolve("присід фронтальний").expect("known");
    let scores = MuscleActivation::default().for_exercise(squat);
    assert_eq!(scores.get("quads"), Some(&1.0));
    assert_eq!(scores.get("abs"), Some(&0.5));
}

#[test]
fn the_template_covers_every_catalogue_muscle_and_colourises_cleanly() {
    let template = fs::read_to_string(repo_root().join("assets/muscle-map.svg")).expect("readable");
    for muscle in catalogue().known_muscles() {
        assert!(
            template.contains(&format!("data-muscle=\"{muscle}\"")),
            "the map has no element for {muscle}"
        );
    }
    let worked = colorize(&template, &BTreeMap::from([("quads".to_string(), 1.0)]));
    assert!(worked.contains(&format!(
        "data-muscle=\"quads\" style=\"fill:{HIGH_COLOR}\""
    )));
    let unworked = colorize(&template, &BTreeMap::new());
    assert!(!unworked.contains(HIGH_COLOR));
    assert!(unworked.contains(ZERO_COLOR));
}
