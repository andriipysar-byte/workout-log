//! The session from `docs/02-data-model.md`, which is a real transcription: if
//! the metrics cannot read that shape they cannot read the archive.

use workout_log_core::Session;
use workout_log_core::analytics::session_metrics::{SessionMetrics, TimeDomain};

fn example() -> Session {
    serde_json::from_str(
        r#"{
        "date": "2026-08-10", "cycle_day": "F1", "start_time": "08:02", "kind": "training",
        "blocks": [
          {"type":"cardio","machine":"велотренажер","duration_min":15,"distance_m":6440,"end_time":"08:18"},
          {"type":"strength","exercise":"гіперекстензія","end_time":"08:28",
           "sets":[{"reps":12,"weight_kg":15},{"reps":12,"weight_kg":30}]},
          {"type":"metcon","scheme":[21,15,9],"format":"for_time","start_time":"08:30","end_time":"08:54",
           "exercises":[{"name":"трастери","weight_kg":50},{"name":"бьорпі"}],
           "rounds":[{"round":1,"reps":21,"split_cumulative_sec":463,"heart_rate":156},
                     {"round":2,"reps":15,"split_cumulative_sec":1155,"heart_rate":178},
                     {"round":3,"reps":9,"split_cumulative_sec":1450,"heart_rate":189}]},
          {"type":"strength","exercise":"присід фронтальний","end_time":"09:12",
           "sets":[{"reps":6,"weight_kg":90},{"reps":3,"weight_kg":110},
                   {"reps":6,"weight_kg":70,"is_backoff":true}]},
          {"type":"cooldown","end_time":"09:20"}
        ]}"#,
    )
    .expect("the example decodes")
}

#[test]
fn sums_tonnage_and_separates_back_off_sets() {
    let metrics = SessionMetrics::of(&example());
    assert_eq!(
        metrics.tonnage_kg,
        (12 * 15 + 12 * 30 + 6 * 90 + 3 * 110 + 6 * 70) as f64
    );
    assert_eq!(metrics.sets, 5);
    assert_eq!(metrics.backoff_sets, 1);
    assert_eq!(metrics.top_sets(), 4);
}

#[test]
fn counts_rep_bands_by_set_and_by_slot() {
    let metrics = SessionMetrics::of(&example());
    // [heavy, base, volume]
    assert_eq!(metrics.band_sets, [1, 2, 2]);
    // The front squat slot is two sixes to one triple: a base slot.
    assert_eq!(metrics.band_slots, [0, 1, 1]);
}

#[test]
fn derives_density_from_the_bracket_timestamps() {
    let density = SessionMetrics::of(&example()).density;
    assert_eq!(density.total_min, Some(78.0)); // 08:02 → 09:20
    // 16 + 10 + 24 + 18 + 8, with the two minutes before the metcon a transition.
    assert_eq!(density.work_min, 76.0);
    assert_eq!(density.transition_min, 2.0);
    assert_eq!(density.work_share(), Some(0.974));
}

#[test]
fn differences_the_cumulative_metcon_splits() {
    let metrics = SessionMetrics::of(&example());
    let metcon = metrics.metcons.first().expect("one metcon");
    assert_eq!(
        metcon
            .rounds
            .iter()
            .map(|r| r.split_sec)
            .collect::<Vec<_>>(),
        [Some(463.0), Some(692.0), Some(295.0)]
    );
    assert_eq!(metcon.rounds[0].seconds_per_rep(), Some(22.05));
    assert_eq!(metcon.total_sec, Some(1450.0));
    assert_eq!(metcon.domain(), Some(TimeDomain::Aerobic)); // 24:10 on the clock
    assert_eq!(metcon.peak_heart_rate(), Some(189));
    // 32.78 s/rep on the last round against 22.05 on the first.
    assert_eq!(metcon.pace_decay_percent(), Some(48.7));
}

#[test]
fn a_planned_session_with_no_times_and_no_weights_reads_as_empty() {
    let session: Session = serde_json::from_str(
        r#"{"date":"2026-08-04","cycle_day":"D1",
            "blocks":[{"type":"strength","exercise":"ривок","sets":[]}]}"#,
    )
    .expect("decodes");
    let metrics = SessionMetrics::of(&session);
    assert_eq!(metrics.tonnage_kg, 0.0);
    assert_eq!(metrics.band_slots, [0, 0, 0]);
    assert_eq!(metrics.density.total_min, None);
    assert!(metrics.to_json()["density"].get("total_min").is_none());
}

#[test]
fn a_cardio_block_with_no_timestamps_still_contributes_its_minutes() {
    let session: Session = serde_json::from_str(
        r#"{"date":"2026-08-04","cycle_day":"D1",
            "blocks":[{"type":"cardio","machine":"","duration_min":10}]}"#,
    )
    .expect("decodes");
    assert_eq!(SessionMetrics::of(&session).density.work_min, 10.0);
}

#[test]
fn every_real_session_produces_metrics() {
    let root = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .find(|d| d.join("cycles.json").is_file())
        .expect("repo root")
        .to_path_buf();
    for entry in std::fs::read_dir(root.join("data")).expect("data/") {
        let path = entry.expect("entry").path();
        if path.extension().is_none_or(|e| e != "json") {
            continue;
        }
        let session: Session =
            serde_json::from_str(&std::fs::read_to_string(&path).expect("readable"))
                .expect("decodes");
        let metrics = SessionMetrics::of(&session);
        assert_eq!(metrics.blocks.len(), session.blocks.len());
        assert!(metrics.tonnage_kg >= 0.0);
    }
}
