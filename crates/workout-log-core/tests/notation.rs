//! The grammar table from `docs/03-log-notation.md`, as the Dart suite pins it.

use workout_log_core::parsing::notation::parse_strength_sets;

fn weights(line: &str) -> Vec<Option<f64>> {
    parse_strength_sets(line)
        .sets
        .iter()
        .map(|s| s.weight_kg)
        .collect()
}

fn reps(line: &str) -> Vec<Option<i64>> {
    parse_strength_sets(line)
        .sets
        .iter()
        .map(|s| s.reps)
        .collect()
}

#[test]
fn a_ramp_is_one_set_per_weight_at_a_fixed_rep_count() {
    let parsed = parse_strength_sets("6 × [70, 80, 90, 100, 110]");
    assert!(parsed.warnings.is_empty());
    assert_eq!(
        weights("6 × [70, 80, 90, 100, 110]"),
        [Some(70.0), Some(80.0), Some(90.0), Some(100.0), Some(110.0)]
    );
    assert_eq!(reps("6 × [70, 80, 90, 100, 110]"), [Some(6); 5]);
    assert!(parsed.sets.iter().all(|s| s.is_backoff.is_none()));
}

#[test]
fn a_trailing_group_is_back_offs() {
    let parsed = parse_strength_sets("6 × [70, 80, 90, 100, 110] + 6 × [30]");
    assert_eq!(parsed.sets.len(), 6);
    assert_eq!(
        parsed.sets.iter().map(|s| s.is_backoff).collect::<Vec<_>>(),
        [None, None, None, None, None, Some(true)]
    );
}

#[test]
fn parenthesised_reps_override_the_group_count() {
    assert_eq!(
        reps("6 × [30, 60, 80(4), 86(2), 90(1)]"),
        [Some(6), Some(6), Some(4), Some(2), Some(1)]
    );
}

#[test]
fn clusters_take_an_explicit_or_summed_total() {
    let explicit = parse_strength_sets("5+5+4+3+3 (20)");
    assert_eq!(explicit.sets.len(), 1);
    assert_eq!(
        explicit.sets[0].cluster.as_deref(),
        Some(&[5, 5, 4, 3, 3][..])
    );
    assert_eq!(explicit.sets[0].total_reps, Some(20));

    let summed = parse_strength_sets("4+4+3+2+2+2+2+1");
    assert_eq!(summed.sets[0].total_reps, Some(20));
}

#[test]
fn timed_holds_become_duration_sets_with_no_reps() {
    let parsed = parse_strength_sets("4 × [54c, 40c, 36c, 42c]");
    assert_eq!(
        parsed
            .sets
            .iter()
            .map(|s| s.duration_sec)
            .collect::<Vec<_>>(),
        [Some(54.0), Some(40.0), Some(36.0), Some(42.0)]
    );
    assert!(parsed.sets.iter().all(|s| s.reps.is_none()));
}

#[test]
fn a_bare_brace_list_needs_no_count_prefix() {
    // Cyrillic с, which the paper log uses interchangeably with Latin c.
    let parsed = parse_strength_sets("{60с, 70с, 30с, 35с}");
    assert_eq!(parsed.sets.len(), 4);
    assert_eq!(parsed.sets[0].duration_sec, Some(60.0));
}

#[test]
fn every_multiplier_and_seconds_suffix_is_accepted() {
    for multiplier in ["x", "X", "х", "Х", "*", "×"] {
        assert_eq!(
            weights(&format!("6 {multiplier} [70,80]")),
            [Some(70.0), Some(80.0)],
            "multiplier {multiplier} failed"
        );
    }
    for suffix in ["c", "C", "с", "С"] {
        let parsed = parse_strength_sets(&format!("3 × [40{suffix}]"));
        assert_eq!(
            parsed.sets[0].duration_sec,
            Some(40.0),
            "suffix {suffix} failed"
        );
    }
}

#[test]
fn a_dot_is_a_decimal_point_and_a_comma_is_always_a_separator() {
    assert_eq!(weights("1 × [47.5]"), [Some(47.5)]);
    assert_eq!(weights("1 × [47,5]"), [Some(47.0), Some(5.0)]);
}

#[test]
fn blank_input_is_silent_and_junk_warns_without_losing_the_rest() {
    let blank = parse_strength_sets("   ");
    assert!(blank.sets.is_empty() && blank.warnings.is_empty());

    let partial = parse_strength_sets("6 × [70, банан, 90]");
    assert_eq!(weights("6 × [70, банан, 90]"), [Some(70.0), Some(90.0)]);
    assert_eq!(partial.warnings.len(), 1);
    assert!(partial.warnings[0].contains("банан"));

    let junk = parse_strength_sets("нічого корисного");
    assert!(junk.sets.is_empty());
    assert_eq!(junk.warnings.len(), 1);
}
