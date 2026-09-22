//! The real files are the spec: `data/*.json` is kept canonical on disk, so a
//! decode/encode cycle must reproduce each file byte for byte.

use std::fs;
use std::path::PathBuf;

use workout_log_core::coding::{canonical_json, decode_json, encode_json};

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .find(|dir| dir.join("cycles.json").is_file())
        .expect("a repo root holding cycles.json")
        .to_path_buf()
}

#[test]
fn every_session_file_round_trips_byte_for_byte() {
    let data = repo_root().join("data");
    let mut checked = 0;
    for entry in fs::read_dir(&data).expect("data/ exists") {
        let path = entry.expect("readable entry").path();
        if path.extension().is_none_or(|ext| ext != "json") {
            continue;
        }
        let original = fs::read_to_string(&path).expect("readable session");
        let decoded = decode_json(&original).expect("valid JSON");
        assert_eq!(
            encode_json(&decoded),
            original,
            "{} is not canonical after a round trip",
            path.display()
        );
        checked += 1;
    }
    assert!(checked > 0, "no session files found in {}", data.display());
}

#[test]
fn reference_files_survive_canonicalisation() {
    for name in ["cycles.json", "exercises.json"] {
        let path = repo_root().join(name);
        let text = fs::read_to_string(&path).expect("readable reference file");
        let decoded = decode_json(&text).expect("valid JSON");
        assert_eq!(
            canonical_json(&decoded),
            canonical_json(&decode_json(&encode_json(&decoded)).expect("re-readable")),
            "{name} loses data through an encode/decode cycle"
        );
    }
}

#[test]
fn every_session_file_round_trips_through_the_typed_model() {
    let data = repo_root().join("data");
    let mut checked = 0;
    for entry in fs::read_dir(&data).expect("data/ exists") {
        let path = entry.expect("readable entry").path();
        if path.extension().is_none_or(|ext| ext != "json") {
            continue;
        }
        let original = fs::read_to_string(&path).expect("readable session");
        let session: workout_log_core::Session = serde_json::from_str(&original)
            .unwrap_or_else(|e| panic!("{} did not decode: {e}", path.display()));
        let encoded = encode_json(&serde_json::to_value(&session).expect("encodable"));
        assert_eq!(
            encoded,
            original,
            "{} changed through the model",
            path.display()
        );
        checked += 1;
    }
    assert_eq!(checked, 9, "expected the 9 real session files");
}
