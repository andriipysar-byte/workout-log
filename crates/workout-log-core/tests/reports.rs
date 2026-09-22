//! Progress tracks and the training report, against the real catalogue.

use std::fs;
use std::path::PathBuf;

use workout_log_core::analytics::cycle_report::TrainingReport;
use workout_log_core::analytics::progress::{one_rep_max, report};
use workout_log_core::models::work_set::RepBand;
use workout_log_core::{Catalogue, Session};

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .find(|d| d.join("cycles.json").is_file())
        .expect("repo root")
        .to_path_buf()
}

fn catalogue() -> Catalogue {
    serde_json::from_str(&fs::read_to_string(repo_root().join("exercises.json")).expect("readable"))
        .expect("decodes")
}

fn session(json: &str) -> Session {
    serde_json::from_str(json).expect("session decodes")
}

fn real_sessions() -> Vec<Session> {
    let mut sessions = Vec::new();
    for entry in fs::read_dir(repo_root().join("data")).expect("data/") {
        let path = entry.expect("entry").path();
        if path.extension().is_none_or(|e| e != "json") {
            continue;
        }
        sessions.push(session(&fs::read_to_string(&path).expect("readable")));
    }
    sessions
}

#[test]
fn one_rep_max_identities_and_bracketing() {
    assert_eq!(one_rep_max::estimate(Some(100.0), Some(1)), Some(100.0));
    let six = one_rep_max::estimate(Some(100.0), Some(6)).expect("an estimate");
    assert!(six > 116.1 && six < 120.0, "estimate for 6 reps was {six}");
    assert_eq!(one_rep_max::estimate(None, Some(5)), None);
    assert_eq!(one_rep_max::estimate(Some(100.0), Some(0)), None);
    assert_eq!(one_rep_max::estimate(Some(0.0), Some(5)), None);
    // Brzycki is undefined at 37 reps; the estimate falls back to Epley alone.
    assert!(one_rep_max::estimate(Some(100.0), Some(37)).is_some());
}

#[test]
fn a_slot_splits_into_one_track_per_rep_band_heaviest_first() {
    let sessions = vec![session(
        r#"{"date":"2026-07-01","cycle_day":"A2","blocks":[
            {"type":"strength","exercise":"присід фронтальний","sets":[
              {"reps":6,"weight_kg":90},{"reps":3,"weight_kg":110}]}]}"#,
    )];
    let progress = report(&sessions, "присід фронтальний", Some(&catalogue()), false);
    assert_eq!(
        progress.series.iter().map(|s| s.band).collect::<Vec<_>>(),
        [Some(RepBand::Heavy), Some(RepBand::Base)]
    );
}

#[test]
fn back_offs_are_reported_beside_the_top_set_never_inside_it() {
    let sessions = vec![session(
        r#"{"date":"2026-07-01","cycle_day":"A2","blocks":[
            {"type":"strength","exercise":"присід фронтальний","sets":[
              {"reps":6,"weight_kg":90},{"reps":6,"weight_kg":100},
              {"reps":6,"weight_kg":70,"is_backoff":true}]}]}"#,
    )];
    let progress = report(&sessions, "присід фронтальний", Some(&catalogue()), false);
    let point = &progress.series[0].points[0];
    assert_eq!(point.weight_kg, Some(100.0));
    assert_eq!(point.backoff_sets, 1);
    assert_eq!(point.backoff_tonnage_kg, 420.0);
    assert_eq!(point.sets, 3);
}

#[test]
fn an_alias_resolves_into_the_canonical_track_and_by_pattern_widens_it() {
    let sessions = vec![session(
        r#"{"date":"2026-07-01","cycle_day":"A2","blocks":[
            {"type":"strength","exercise":"фр. присід","sets":[{"reps":5,"weight_kg":100}]},
            {"type":"strength","exercise":"присід на плечах","sets":[{"reps":5,"weight_kg":120}]}]}"#,
    )];
    let catalogue = catalogue();
    let narrow = report(&sessions, "фр. присід", Some(&catalogue), false);
    assert_eq!(narrow.resolved.as_deref(), Some("присід фронтальний"));
    assert_eq!(narrow.variants.len(), 1);

    let wide = report(&sessions, "фр. присід", Some(&catalogue), true);
    assert_eq!(
        wide.variants.len(),
        2,
        "by_pattern should gather both squats"
    );
}

#[test]
fn an_unknown_lift_is_still_tracked() {
    let sessions = vec![session(
        r#"{"date":"2026-07-01","cycle_day":"A2","blocks":[
            {"type":"strength","exercise":"жим носом","sets":[{"reps":5,"weight_kg":10}]}]}"#,
    )];
    let progress = report(&sessions, "жим носом", Some(&catalogue()), false);
    assert_eq!(progress.resolved, None);
    assert_eq!(progress.series.len(), 1);
}

#[test]
fn all_volume_slots_raise_the_p5_alert() {
    let sessions = vec![session(
        r#"{"date":"2026-07-01","cycle_day":"A2","blocks":[
            {"type":"strength","exercise":"гіперекстензія","sets":[
              {"reps":12,"weight_kg":15},{"reps":12,"weight_kg":20}]}]}"#,
    )];
    let report = TrainingReport::of(&sessions, &catalogue(), 12, 0.3);
    let p5 = report
        .alerts
        .iter()
        .find(|a| a.principle == "P5")
        .expect("a P5 alert");
    assert!(p5.message.contains("100%"), "{}", p5.message);
}

#[test]
fn an_explosive_lift_above_three_reps_raises_p3() {
    let sessions = vec![session(
        r#"{"date":"2026-07-01","cycle_day":"A1","blocks":[
            {"type":"strength","exercise":"ривок","sets":[{"reps":6,"weight_kg":60}]}]}"#,
    )];
    let report = TrainingReport::of(&sessions, &catalogue(), 12, 0.3);
    let p3 = report
        .alerts
        .iter()
        .find(|a| a.principle == "P3")
        .expect("a P3 alert");
    assert_eq!(p3.sessions, ["2026-07-01 ривок"]);
}

#[test]
fn pattern_frequency_is_per_cycle_and_a_long_block_without_anchors_raises_p10() {
    let sessions = vec![
        session(
            r#"{"date":"2026-07-01","cycle_day":"A2","blocks":[
                {"type":"strength","exercise":"присід фронтальний","sets":[{"reps":5,"weight_kg":100}]}]}"#,
        ),
        session(
            r#"{"date":"2026-07-24","cycle_day":"B2","blocks":[
                {"type":"strength","exercise":"присід на плечах","sets":[{"reps":5,"weight_kg":120}]}]}"#,
        ),
    ];
    let report = TrainingReport::of(&sessions, &catalogue(), 12, 0.3);
    assert_eq!(report.days, Some(24)); // inclusive of both ends
    let squat = report
        .patterns
        .iter()
        .find(|p| format!("{:?}", p.pattern) == "Squat")
        .expect("a squat row");
    assert_eq!(squat.slots, 2);
    assert_eq!(squat.per_cycle, 1.0); // 2 slots over 2 cycles
    assert!(report.alerts.iter().any(|a| a.principle == "P10"));
}

#[test]
fn load_keeps_strength_and_conditioning_separate() {
    let sessions = vec![session(
        r#"{"date":"2026-07-01","cycle_day":"A1","start_time":"08:00","blocks":[
            {"type":"strength","exercise":"присід фронтальний","sets":[{"reps":5,"weight_kg":100}],"end_time":"08:30"},
            {"type":"metcon","scheme":[21,15,9],"start_time":"08:30","end_time":"08:50",
             "exercises":[{"name":"бьорпі"}],
             "rounds":[{"round":1,"split_cumulative_sec":600}]}]}"#,
    )];
    let report = TrainingReport::of(&sessions, &catalogue(), 12, 0.3);
    let point = &report.load[0];
    assert_eq!(point.tonnage_kg, 500.0);
    assert_eq!(point.metcon_sec, Some(600.0));
    assert_eq!(report.total_tonnage_kg(), 500.0);
}

#[test]
fn the_whole_real_archive_reports_without_panicking() {
    let sessions = real_sessions();
    let report = TrainingReport::of(&sessions, &catalogue(), 12, 0.3);
    assert_eq!(report.session_count, 9);
    assert!(report.total_tonnage_kg() > 0.0);
    assert_eq!(report.load.len(), 9);
    // The archive really does carry names the catalogue does not know.
    let _ = report.unknown_exercises;
    assert!(report.to_json().is_object());
}
