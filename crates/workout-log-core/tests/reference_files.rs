//! `cycles.json` and `exercises.json` are hand-maintained and the app writes
//! them back, so the models must keep every key they do not themselves model.
//! Drop a key and a save would silently delete it from the user's file.

use std::fs;
use std::path::PathBuf;

use workout_log_core::coding::{canonical_json, decode_json, encode_json};
use workout_log_core::{Catalogue, CycleCatalogue};

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .find(|dir| dir.join("cycles.json").is_file())
        .expect("a repo root holding cycles.json")
        .to_path_buf()
}

fn read(name: &str) -> String {
    fs::read_to_string(repo_root().join(name)).expect("readable reference file")
}

#[test]
fn cycles_survive_the_model() {
    let text = read("cycles.json");
    let cycles: CycleCatalogue = serde_json::from_str(&text).expect("cycles.json decodes");
    assert_eq!(
        canonical_json(&serde_json::to_value(&cycles).expect("encodable")),
        canonical_json(&decode_json(&text).expect("valid JSON")),
        "cycles.json lost data through the model"
    );
}

#[test]
fn every_block_keeps_its_planning_only_role() {
    let cycles: CycleCatalogue = serde_json::from_str(&read("cycles.json")).expect("decodes");
    let roles: Vec<&str> = cycles
        .cycles
        .iter()
        .flat_map(|c| &c.sessions)
        .flat_map(|s| &s.blocks)
        .filter_map(|b| b.role.as_deref())
        .collect();
    for expected in [
        "warmup",
        "explosive",
        "metcon",
        "accessory",
        "grip",
        "cooldown",
    ] {
        assert!(roles.contains(&expected), "role {expected} did not survive");
    }
}

#[test]
fn unmodelled_keys_survive_in_extras() {
    let cycles: CycleCatalogue = serde_json::from_str(&read("cycles.json")).expect("decodes");
    assert!(
        cycles.comment.is_some(),
        "the file's own $comment must survive a save"
    );
    let text = encode_json(&serde_json::to_value(&cycles).expect("encodable"));
    assert!(text.contains("\"$comment\""));
}

#[test]
fn catalogue_survives_the_model() {
    let text = read("exercises.json");
    let catalogue: Catalogue = serde_json::from_str(&text).expect("exercises.json decodes");
    assert_eq!(
        canonical_json(&serde_json::to_value(&catalogue).expect("encodable")),
        canonical_json(&decode_json(&text).expect("valid JSON")),
        "exercises.json lost data through the model"
    );
    assert!(catalogue.comment.is_some());
}

#[test]
fn known_muscles_are_sorted_and_non_empty() {
    let catalogue: Catalogue = serde_json::from_str(&read("exercises.json")).expect("decodes");
    let muscles = catalogue.known_muscles();
    assert!(muscles.contains(&"quads".to_string()));
    let mut sorted = muscles.clone();
    sorted.sort();
    assert_eq!(muscles, sorted);
}

#[test]
fn aliases_resolve_case_insensitively() {
    let catalogue: Catalogue = serde_json::from_str(&read("exercises.json")).expect("decodes");
    let resolved = catalogue.resolve("фр. присід").expect("alias resolves");
    assert_eq!(resolved.name, "присід фронтальний");
    assert_eq!(
        catalogue.resolve("  РИВОК ").map(|e| e.name.as_str()),
        Some("ривок")
    );
}

#[test]
fn adding_an_existing_name_replaces_rather_than_duplicates() {
    let catalogue: Catalogue = serde_json::from_str(&read("exercises.json")).expect("decodes");
    let before = catalogue.exercises.len();
    let mut replacement = catalogue.resolve("ривок").expect("known").clone();
    replacement.notes = Some("touched".into());
    let after = catalogue.with_exercise(replacement);
    assert_eq!(after.exercises.len(), before);
    assert_eq!(
        after.resolve("ривок").and_then(|e| e.notes.as_deref()),
        Some("touched")
    );
}
