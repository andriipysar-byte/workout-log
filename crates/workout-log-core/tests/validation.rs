//! The real archive must be error-free; the rest pins the rule set.

use std::fs;
use std::path::PathBuf;

use workout_log_core::validation::{IssueSeverity, has_errors, validate};
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
        .expect("decodes")
}

#[test]
fn every_real_session_is_free_of_errors() {
    let catalogue = catalogue();
    let mut checked = 0;
    for entry in fs::read_dir(repo_root().join("data")).expect("data/ exists") {
        let path = entry.expect("entry").path();
        if path.extension().is_none_or(|e| e != "json") {
            continue;
        }
        let session: Session =
            serde_json::from_str(&fs::read_to_string(&path).expect("readable")).expect("decodes");
        let issues = validate(&session, Some(&catalogue));
        let errors: Vec<_> = issues
            .iter()
            .filter(|i| i.severity == IssueSeverity::Error)
            .map(ToString::to_string)
            .collect();
        assert!(
            errors.is_empty(),
            "{} has errors: {errors:?}",
            path.display()
        );
        checked += 1;
    }
    assert_eq!(checked, 9);
}

#[test]
fn a_malformed_header_reports_date_then_start_time() {
    let session: Session = serde_json::from_str(
        r#"{"date": "22.09.2026", "cycle_day": "A1", "start_time": "9am", "blocks": []}"#,
    )
    .expect("decodes");
    let issues = validate(&session, None);
    let error_paths: Vec<&str> = issues
        .iter()
        .filter(|i| i.severity == IssueSeverity::Error)
        .map(|i| i.path.as_str())
        .collect();
    assert_eq!(error_paths, ["date", "start_time"]);
    assert!(has_errors(&issues));
}

#[test]
fn an_off_convention_cycle_day_is_only_a_warning() {
    let session: Session =
        serde_json::from_str(r#"{"date": "2026-09-22", "cycle_day": "Z9", "blocks": []}"#)
            .expect("decodes");
    let issues = validate(&session, None);
    assert!(!has_errors(&issues));
    assert!(issues.iter().any(
        |i| i.path == "cycle_day" && i.message.contains("does not follow the A1…F2 convention")
    ));
}

#[test]
fn a_blank_exercise_errors_but_an_unknown_one_warns() {
    let blank: Session = serde_json::from_str(
        r#"{"date":"2026-09-22","cycle_day":"A1","blocks":[{"type":"strength","exercise":"  ","sets":[]}]}"#,
    )
    .expect("decodes");
    assert!(has_errors(&validate(&blank, None)));

    let unknown: Session = serde_json::from_str(
        r#"{"date":"2026-09-22","cycle_day":"A1","blocks":[{"type":"strength","exercise":"жим носом","sets":[{"reps":5}]}]}"#,
    )
    .expect("decodes");
    let issues = validate(&unknown, Some(&catalogue()));
    assert!(!has_errors(&issues));
    assert!(issues.iter().any(|i| i.message.contains("жим носом")));
}

#[test]
fn a_cluster_total_mismatch_and_backwards_splits_warn() {
    let session: Session = serde_json::from_str(
        r#"{"date":"2026-09-22","cycle_day":"A1","blocks":[
            {"type":"strength","exercise":"вис","sets":[{"cluster":[5,5],"total_reps":99}]},
            {"type":"metcon","exercises":[{"name":"бьорпі"}],"rounds":[
                {"round":1,"split_cumulative_sec":100},
                {"round":2,"split_cumulative_sec":50}]}]}"#,
    )
    .expect("decodes");
    let issues = validate(&session, None);
    assert!(!has_errors(&issues));
    assert!(
        issues
            .iter()
            .any(|i| i.message.contains("does not match the chain sum 10"))
    );
    assert!(issues.iter().any(|i| i.message.contains("goes backwards")));
}

#[test]
fn overlapping_blocks_warn_on_the_later_block() {
    let session: Session = serde_json::from_str(
        r#"{"date":"2026-09-22","cycle_day":"A1","start_time":"08:00","blocks":[
            {"type":"strength","exercise":"вис","sets":[],"start_time":"08:00","end_time":"09:00"},
            {"type":"strength","exercise":"вис","sets":[],"start_time":"08:30","end_time":"09:30"}]}"#,
    )
    .expect("decodes");
    let issues = validate(&session, None);
    assert!(!has_errors(&issues));
    assert!(issues.iter().any(|i| {
        i.path == "blocks[1]"
            && i.message
                .contains("starts before the previous block ended (08:30 after 09:00)")
    }));
}
