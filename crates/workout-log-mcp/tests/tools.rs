mod harness;

use harness::{TestRepo, session, strength};
use serde_json::json;
use workout_log_mcp::requests::*;
use workout_log_mcp::tools;

#[test]
fn create_starts_from_the_cycle_template() {
    let repo = TestRepo::new();
    let mut workspace = repo.workspace();
    let result = tools::create_session(
        &mut workspace,
        &CreateSession {
            date: "2026-09-22".into(),
            cycle_day: "A1".into(),
            from_cycle: None,
            kind: None,
            start_time: Some("08:00".into()),
            bodyweight_kg: None,
            notes: None,
            overwrite: None,
        },
    )
    .expect("creates");

    assert_eq!(result["id"], "2026-09-22_A1.json");
    assert_eq!(result["from_cycle"], "hybrid-8");
    assert_eq!(result["session"]["blocks"][0]["type"], "cardio");
    assert!(
        !result["session"]["blocks"]
            .as_array()
            .expect("blocks")
            .is_empty()
    );
    assert_eq!(repo.session_ids(), ["2026-09-22_A1.json"]);
    assert!(
        repo.read_session("2026-09-22_A1.json")
            .contains("\"start_time\": \"08:00\"")
    );
}

#[test]
fn create_refuses_to_replace_silently() {
    let repo = TestRepo::new();
    let mut workspace = repo.workspace();
    let request = |overwrite| CreateSession {
        date: "2026-09-22".into(),
        cycle_day: "A1".into(),
        from_cycle: None,
        kind: None,
        start_time: None,
        bodyweight_kg: None,
        notes: None,
        overwrite,
    };
    tools::create_session(&mut workspace, &request(None)).expect("first create");
    let refused = tools::create_session(&mut workspace, &request(None)).expect_err("refuses");
    assert!(refused.0.contains("already exists"), "{}", refused.0);
    tools::create_session(&mut workspace, &request(Some(true))).expect("overwrites");
}

#[test]
fn a_malformed_date_writes_nothing() {
    let repo = TestRepo::new();
    let mut workspace = repo.workspace();
    let error = tools::create_session(
        &mut workspace,
        &CreateSession {
            date: "22.09.2026".into(),
            cycle_day: "A1".into(),
            from_cycle: None,
            kind: None,
            start_time: None,
            bodyweight_kg: None,
            notes: None,
            overwrite: None,
        },
    )
    .expect_err("rejects");
    assert!(error.0.contains("ISO date"), "{}", error.0);
    assert!(repo.session_ids().is_empty());
}

#[test]
fn log_sets_fills_a_slot_and_writes_canonical_json() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-09-22_A2.json",
        &session(
            "2026-09-22",
            "A2",
            json!([strength("присід фронтальний", json!([]))]),
        ),
    );
    let mut workspace = repo.workspace();
    let result = tools::log_sets(
        &mut workspace,
        &LogSets {
            id: Some("2026-09-22_A2.json".into()),
            date: None,
            cycle_day: None,
            notation: "6 × [70, 80, 90, 100] + 6 × [70]".into(),
            exercise: Some("присід фронтальний".into()),
            block_index: None,
            mode: None,
        },
    )
    .expect("logs");

    let sets = result["sets"].as_array().expect("sets");
    assert_eq!(sets.len(), 5);
    assert_eq!(sets[0]["reps"], 6);
    assert_eq!(sets[0]["weight_kg"], 70.0);
    assert_eq!(sets[4]["is_backoff"], true);
    // Canonical on disk: sorted keys and no `.0` on an integral weight.
    assert!(
        repo.read_session("2026-09-22_A2.json")
            .contains("\"weight_kg\": 70\n")
    );
}

#[test]
fn a_duplicate_exercise_needs_a_block_index() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-09-22_A2.json",
        &session(
            "2026-09-22",
            "A2",
            json!([strength("вис", json!([])), strength("вис", json!([]))]),
        ),
    );
    let mut workspace = repo.workspace();
    let ambiguous = tools::log_sets(
        &mut workspace,
        &LogSets {
            id: Some("2026-09-22_A2.json".into()),
            date: None,
            cycle_day: None,
            notation: "4 × [54c]".into(),
            exercise: Some("вис".into()),
            block_index: None,
            mode: None,
        },
    )
    .expect_err("ambiguous");
    assert!(ambiguous.0.contains("blocks 0, 1"), "{}", ambiguous.0);

    let result = tools::log_sets(
        &mut workspace,
        &LogSets {
            id: Some("2026-09-22_A2.json".into()),
            date: None,
            cycle_day: None,
            notation: "4 × [54c]".into(),
            exercise: None,
            block_index: Some(1),
            mode: None,
        },
    )
    .expect("logs by index");
    assert_eq!(result["sets"][0]["duration_sec"], 54.0);
}

#[test]
fn an_alias_selects_the_slot() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-09-22_A2.json",
        &session(
            "2026-09-22",
            "A2",
            json!([strength("присід фронтальний", json!([]))]),
        ),
    );
    let mut workspace = repo.workspace();
    let result = tools::log_sets(
        &mut workspace,
        &LogSets {
            id: None,
            date: Some("2026-09-22".into()),
            cycle_day: None,
            notation: "5 × [100]".into(),
            exercise: Some("фр. присід".into()),
            block_index: None,
            mode: None,
        },
    )
    .expect("logs via alias");
    assert_eq!(result["exercise"], "присід фронтальний");
}

#[test]
fn moving_the_header_renames_the_file() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-09-22_A2.json",
        &session("2026-09-22", "A2", json!([strength("вис", json!([]))])),
    );
    let mut workspace = repo.workspace();
    let mut document = session("2026-09-23", "A2", json!([strength("вис", json!([]))]));
    document["kind"] = json!("training");

    let result = tools::write_session(
        &mut workspace,
        &WriteSession {
            session: document.as_object().expect("an object").clone(),
            id: Some("2026-09-22_A2.json".into()),
            overwrite: None,
        },
    )
    .expect("writes");

    assert_eq!(result["id"], "2026-09-23_A2.json");
    assert_eq!(result["renamed_from"], "2026-09-22_A2.json");
    assert_eq!(repo.session_ids(), ["2026-09-23_A2.json"]);
}

#[test]
fn a_corrupting_document_is_refused_and_the_file_survives() {
    let repo = TestRepo::new();
    let original = session("2026-09-22", "A2", json!([strength("вис", json!([]))]));
    repo.write_session("2026-09-22_A2.json", &original);
    let before = repo.read_session("2026-09-22_A2.json");

    let mut workspace = repo.workspace();
    let broken = session("2026-09-22", "A2", json!([strength("   ", json!([]))]));
    let error = tools::write_session(
        &mut workspace,
        &WriteSession {
            session: broken.as_object().expect("an object").clone(),
            id: Some("2026-09-22_A2.json".into()),
            overwrite: None,
        },
    )
    .expect_err("refuses");

    assert!(error.0.contains("blocks[0].exercise"), "{}", error.0);
    assert_eq!(repo.read_session("2026-09-22_A2.json"), before);
}

#[test]
fn delete_requires_confirmation() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-09-22_A2.json",
        &session("2026-09-22", "A2", json!([strength("вис", json!([]))])),
    );
    let mut workspace = repo.workspace();
    let selector = |confirm| DeleteSession {
        id: Some("2026-09-22_A2.json".into()),
        date: None,
        cycle_day: None,
        confirm,
    };

    let refused = tools::delete_session(&mut workspace, &selector(false)).expect_err("refuses");
    assert!(refused.0.contains("confirm"), "{}", refused.0);
    assert_eq!(repo.session_ids().len(), 1);

    let deleted = tools::delete_session(&mut workspace, &selector(true)).expect("deletes");
    assert_eq!(deleted["deleted"], "2026-09-22_A2.json");
    assert!(repo.session_ids().is_empty());
}

#[test]
fn parse_notation_touches_no_files() {
    let repo = TestRepo::new();
    let result = tools::parse_notation(&ParseNotation {
        notation: "5+5+4+3+3 (20)".into(),
    })
    .expect("parses");
    assert_eq!(result["sets"][0]["cluster"], json!([5, 5, 4, 3, 3]));
    assert_eq!(result["sets"][0]["total_reps"], 20);
    assert!(repo.session_ids().is_empty());
}

#[test]
fn analyze_session_reports_tonnage_bands_and_muscles() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-09-22_A2.json",
        &json!({
            "date": "2026-09-22", "cycle_day": "A2", "kind": "training",
            "start_time": "08:00",
            "blocks": [{
                "type": "strength", "exercise": "присід фронтальний", "end_time": "08:40",
                "sets": [{"reps": 6, "weight_kg": 90}, {"reps": 3, "weight_kg": 110},
                         {"reps": 6, "weight_kg": 70, "is_backoff": true}]}]
        }),
    );
    let workspace = repo.workspace();
    let result = tools::analyze_session(
        &workspace,
        &AnalyzeSession {
            id: Some("2026-09-22_A2.json".into()),
            ..Default::default()
        },
    )
    .expect("analyses");

    assert_eq!(result["metrics"]["strength"]["tonnage_kg"], 1290.0);
    assert_eq!(result["metrics"]["strength"]["backoff_sets"], 1);
    assert_eq!(
        result["metrics"]["strength"]["rep_band_sets"],
        json!({"heavy": 1, "base": 2})
    );
    assert_eq!(result["metrics"]["density"]["work_min"], 40.0);
    assert_eq!(result["muscles"]["dominant_group"], "legs");
}

#[test]
fn analyze_training_narrows_and_can_drop_the_load_series() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-07-01_A1.json",
        &session(
            "2026-07-01",
            "A1",
            json!([strength("ривок", json!([{"reps": 6, "weight_kg": 60}]))]),
        ),
    );
    repo.write_session(
        "2026-07-20_A2.json",
        &session(
            "2026-07-20",
            "A2",
            json!([strength(
                "присід фронтальний",
                json!([{"reps": 5, "weight_kg": 100}])
            )]),
        ),
    );
    let workspace = repo.workspace();

    let all = tools::analyze_training(&workspace, &AnalyzeTraining::default()).expect("reports");
    assert_eq!(all["sessions"], 2);
    assert_eq!(all["load"].as_array().expect("load").len(), 2);
    assert!(
        all["alerts"]
            .as_array()
            .expect("alerts")
            .iter()
            .any(|a| a["principle"] == "P3")
    );

    let narrowed = tools::analyze_training(
        &workspace,
        &AnalyzeTraining {
            from: Some("2026-07-10".into()),
            include_load: Some(false),
            ..Default::default()
        },
    )
    .expect("reports");
    assert_eq!(narrowed["sessions"], 1);
    assert!(narrowed.get("load").is_none());

    let empty = tools::analyze_training(
        &workspace,
        &AnalyzeTraining {
            from: Some("2027-01-01".into()),
            ..Default::default()
        },
    )
    .expect_err("empty range");
    assert!(empty.0.contains("no sessions"), "{}", empty.0);
}

#[test]
fn exercise_progress_resolves_aliases_and_notes_an_empty_track() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-07-01_A2.json",
        &session(
            "2026-07-01",
            "A2",
            json!([strength(
                "фр. присід",
                json!([{"reps": 5, "weight_kg": 90}])
            )]),
        ),
    );
    repo.write_session(
        "2026-07-08_A2.json",
        &session(
            "2026-07-08",
            "A2",
            json!([strength(
                "фр. присід",
                json!([{"reps": 5, "weight_kg": 95}])
            )]),
        ),
    );
    let workspace = repo.workspace();

    let result = tools::exercise_progress(
        &workspace,
        &ExerciseProgressArgs {
            exercise: "фр. присід".into(),
            by_pattern: None,
            from: None,
            to: None,
        },
    )
    .expect("reports");
    assert_eq!(result["resolved"], "присід фронтальний");
    let trend = result["series"][0]["trend_1rm_kg"]
        .as_f64()
        .expect("a trend");
    assert!(trend > 0.0);

    let unlogged = tools::exercise_progress(
        &workspace,
        &ExerciseProgressArgs {
            exercise: "жим лежачи".into(),
            by_pattern: None,
            from: None,
            to: None,
        },
    )
    .expect("reports");
    assert!(
        unlogged["note"]
            .as_str()
            .expect("a note")
            .contains("list_exercises")
    );
}

#[test]
fn muscle_activation_has_a_session_mode_and_a_range_mode() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-07-01_A2.json",
        &session(
            "2026-07-01",
            "A2",
            json!([strength(
                "присід фронтальний",
                json!([{"reps": 5, "weight_kg": 100}])
            )]),
        ),
    );
    repo.write_session(
        "2026-07-08_A2.json",
        &session(
            "2026-07-08",
            "A2",
            json!([strength(
                "присід на плечах",
                json!([{"reps": 5, "weight_kg": 120}])
            )]),
        ),
    );
    let workspace = repo.workspace();

    let single = tools::muscle_activation(
        &workspace,
        &MuscleActivationArgs {
            id: Some("2026-07-01_A2.json".into()),
            mode: Some(WeightingArg::Tonnage),
            ..Default::default()
        },
    )
    .expect("session mode");
    assert_eq!(single["weighting"], "tonnage");
    assert_eq!(single["muscles"]["quads"], 1.0);

    let range =
        tools::muscle_activation(&workspace, &MuscleActivationArgs::default()).expect("range mode");
    assert_eq!(range["sessions"], 2);
    assert!(!range["groups"].as_object().expect("groups").is_empty());
}

#[test]
fn validate_archive_surfaces_an_unreadable_file() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-07-01_A2.json",
        &session(
            "2026-07-01",
            "A2",
            json!([strength(
                "присід фронтальний",
                json!([{"reps": 5, "weight_kg": 100}])
            )]),
        ),
    );
    repo.write_raw("broken.json", "{ not json");
    let workspace = repo.workspace();

    let result = tools::validate_archive(&workspace, &ValidateArchive::default()).expect("reports");
    assert_eq!(result["checked"], 1);
    assert_eq!(result["unreadable"][0]["id"], "broken.json");
}

#[test]
fn list_sessions_summarises_and_filters_by_exercise() {
    let repo = TestRepo::new();
    repo.write_session(
        "2026-07-01_A1.json",
        &session(
            "2026-07-01",
            "A1",
            json!([strength("ривок", json!([{"reps": 3, "weight_kg": 60}]))]),
        ),
    );
    repo.write_session(
        "2026-07-08_A2.json",
        &session(
            "2026-07-08",
            "A2",
            json!([strength(
                "присід фронтальний",
                json!([{"reps": 5, "weight_kg": 100}])
            )]),
        ),
    );
    let workspace = repo.workspace();

    let all = tools::list_sessions(&workspace, &ListSessions::default()).expect("lists");
    assert_eq!(all["count"], 2);
    assert_eq!(all["sessions"][0]["date"], "2026-07-01");

    let filtered = tools::list_sessions(
        &workspace,
        &ListSessions {
            exercise: Some("ривок".into()),
            ..Default::default()
        },
    )
    .expect("filters");
    assert_eq!(filtered["count"], 1);
    assert_eq!(filtered["sessions"][0]["id"], "2026-07-01_A1.json");
}

#[test]
fn list_exercises_filters_and_always_returns_the_vocabulary() {
    let repo = TestRepo::new();
    let workspace = repo.workspace();
    let result = tools::list_exercises(
        &workspace,
        &ListExercises {
            pattern: Some(PatternArg::Squat),
            ..Default::default()
        },
    )
    .expect("lists");
    let names: Vec<&str> = result["exercises"]
        .as_array()
        .expect("exercises")
        .iter()
        .map(|e| e["name"].as_str().expect("a name"))
        .collect();
    assert!(names.contains(&"присід фронтальний"));
    assert!(
        result["known_muscles"]
            .as_array()
            .expect("muscles")
            .contains(&json!("quads"))
    );
}

#[test]
fn add_exercise_appends_and_keeps_the_file_comment() {
    let repo = TestRepo::new();
    let workspace = repo.workspace();
    let before = workspace.load_catalogue().expect("loads").exercises.len();

    let result = tools::add_exercise(
        &workspace,
        &AddExercise {
            name: "жим носом".into(),
            aliases: Some(vec!["носовий жим".into()]),
            pattern: Some(PatternArg::Press),
            modality: None,
            category: None,
            primary_muscles: vec!["chest".into()],
            secondary_muscles: None,
            notes: None,
            overwrite: None,
        },
    )
    .expect("adds");

    assert_eq!(result["replaced"], false);
    let after = workspace.load_catalogue().expect("loads");
    assert_eq!(after.exercises.len(), before + 1);
    assert!(after.comment.is_some(), "the file's $comment must survive");
    assert_eq!(
        after.resolve("носовий жим").map(|e| e.name.as_str()),
        Some("жим носом")
    );

    let duplicate = tools::add_exercise(
        &workspace,
        &AddExercise {
            name: "жим носом".into(),
            aliases: None,
            pattern: None,
            modality: None,
            category: None,
            primary_muscles: vec!["chest".into()],
            secondary_muscles: None,
            notes: None,
            overwrite: None,
        },
    )
    .expect_err("refuses");
    assert!(
        duplicate.0.contains("already in the catalogue"),
        "{}",
        duplicate.0
    );
}

#[test]
fn add_exercise_flags_muscle_names_outside_the_vocabulary() {
    let repo = TestRepo::new();
    let workspace = repo.workspace();
    let result = tools::add_exercise(
        &workspace,
        &AddExercise {
            name: "махи в сторони".into(),
            aliases: None,
            pattern: None,
            modality: None,
            category: None,
            primary_muscles: vec!["delts".into()],
            secondary_muscles: None,
            notes: None,
            overwrite: None,
        },
    )
    .expect("adds");
    assert_eq!(result["new_muscle_names"]["names"], json!(["delts"]));
}

#[test]
fn list_cycles_summarises_then_expands_one() {
    let repo = TestRepo::new();
    let workspace = repo.workspace();

    let summary = tools::list_cycles(&workspace, &ListCycles::default()).expect("lists");
    let cycles = summary["cycles"].as_array().expect("cycles");
    assert_eq!(cycles.len(), 1);
    assert_eq!(cycles[0]["id"], "hybrid-8");
    assert_eq!(cycles[0]["days"].as_array().expect("days").len(), 8);

    let full = tools::list_cycles(
        &workspace,
        &ListCycles {
            id: Some("hybrid-8".into()),
        },
    )
    .expect("expands");
    assert_eq!(
        full["cycle"]["sessions"]
            .as_array()
            .expect("sessions")
            .len(),
        8
    );

    let unknown = tools::list_cycles(
        &workspace,
        &ListCycles {
            id: Some("nope".into()),
        },
    )
    .expect_err("refuses");
    assert!(unknown.0.contains("hybrid-8"), "{}", unknown.0);
}

#[test]
fn generate_cycle_dry_runs_then_writes_then_skips() {
    let repo = TestRepo::new();
    let mut workspace = repo.workspace();
    let request = |dry_run, force| GenerateCycle {
        cycle_id: "hybrid-8".into(),
        start_date: None,
        force,
        dry_run,
    };

    let dry = tools::generate_cycle(&mut workspace, &request(Some(true), None)).expect("dry run");
    assert_eq!(dry["sessions"][0]["status"], "would write");
    assert!(repo.session_ids().is_empty());

    let written = tools::generate_cycle(&mut workspace, &request(None, None)).expect("writes");
    assert_eq!(written["sessions"].as_array().expect("sessions").len(), 8);
    assert_eq!(repo.session_ids().len(), 8);

    let again = tools::generate_cycle(&mut workspace, &request(None, None)).expect("skips");
    assert!(
        again["sessions"][0]["status"]
            .as_str()
            .expect("a status")
            .starts_with("skipped")
    );
}
