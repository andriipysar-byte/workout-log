//! Warn, never block: only what would corrupt the archive is an error.
//! Everything else the log tolerates (ADR-006) and reports as a warning.

use std::fmt;
use std::sync::LazyLock;

use chrono::NaiveDate;
use regex::Regex;
use serde::{Deserialize, Serialize};

use crate::cycles::cycle_day::CycleDay;
use crate::models::block::Block;
use crate::models::exercise::Catalogue;
use crate::models::session::Session;
use crate::wall_clock;

static ISO_DATE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^\d{4}-\d{2}-\d{2}$").expect("valid pattern"));

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum IssueSeverity {
    Error,
    Warning,
}

impl IssueSeverity {
    pub fn name(self) -> &'static str {
        match self {
            Self::Error => "error",
            Self::Warning => "warning",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ValidationIssue {
    pub severity: IssueSeverity,
    pub path: String,
    pub message: String,
}

impl fmt::Display for ValidationIssue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}: {} — {}",
            self.severity.name(),
            self.path,
            self.message
        )
    }
}

pub fn has_errors(issues: &[ValidationIssue]) -> bool {
    issues.iter().any(|i| i.severity == IssueSeverity::Error)
}

pub fn validate(session: &Session, catalogue: Option<&Catalogue>) -> Vec<ValidationIssue> {
    let mut issues = Issues::default();

    if !ISO_DATE.is_match(&session.date)
        || NaiveDate::parse_from_str(&session.date, "%Y-%m-%d").is_err()
    {
        issues.error(
            "date",
            format!("\"{}\" is not an ISO date (YYYY-MM-DD)", session.date),
        );
    }

    if session.cycle_day.trim().is_empty() {
        issues.error("cycle_day", "is empty");
    } else if CycleDay::try_parse(&session.cycle_day).is_none() {
        issues.warn(
            "cycle_day",
            format!(
                "\"{}\" does not follow the A1…F2 convention",
                session.cycle_day
            ),
        );
    }

    if !wall_clock::is_valid(session.start_time.as_deref()) {
        issues.error(
            "start_time",
            format!(
                "\"{}\" is not a wall-clock time (HH:MM)",
                session.start_time.as_deref().unwrap_or_default()
            ),
        );
    }

    if session.bodyweight_kg.is_some_and(|kg| kg <= 0.0) {
        issues.error("bodyweight_kg", "must be positive");
    }

    if session.blocks.is_empty() {
        issues.warn("blocks", "the session is empty");
    }

    let mut previous_end = wall_clock::minutes(session.start_time.as_deref());
    for (index, block) in session.blocks.iter().enumerate() {
        let path = format!("blocks[{index}]");
        previous_end = check_times(&mut issues, block, &path, previous_end);
        check_block(&mut issues, block, &path, catalogue);
    }

    issues.0
}

fn check_times(
    issues: &mut Issues,
    block: &Block,
    path: &str,
    previous_end: Option<i64>,
) -> Option<i64> {
    let (start, end) = (block.start_time(), block.end_time());
    if !wall_clock::is_valid(start) {
        issues.error(
            format!("{path}.start_time"),
            format!(
                "\"{}\" is not a wall-clock time (HH:MM)",
                start.unwrap_or_default()
            ),
        );
    }
    if !wall_clock::is_valid(end) {
        issues.error(
            format!("{path}.end_time"),
            format!(
                "\"{}\" is not a wall-clock time (HH:MM)",
                end.unwrap_or_default()
            ),
        );
    }

    let start_min = wall_clock::minutes(start);
    let end_min = wall_clock::minutes(end);
    if let (Some(s), Some(e)) = (start_min, end_min)
        && e < s
    {
        issues.warn(format!("{path}.end_time"), "ends before it starts");
    }
    if let (Some(first), Some(previous)) = (start_min.or(end_min), previous_end)
        && first < previous
    {
        issues.warn(
            path,
            format!(
                "starts before the previous block ended ({} after {})",
                wall_clock::format(first),
                wall_clock::format(previous)
            ),
        );
    }
    end_min.or(previous_end)
}

fn check_block(issues: &mut Issues, block: &Block, path: &str, catalogue: Option<&Catalogue>) {
    match block {
        Block::Cardio(cardio) => {
            if cardio.machine.trim().is_empty() {
                issues.warn(format!("{path}.machine"), "no machine recorded");
            }
            if cardio.duration_min.is_some_and(|v| v <= 0.0) {
                issues.error(format!("{path}.duration_min"), "must be positive");
            }
            if cardio.distance_m.is_some_and(|v| v <= 0.0) {
                issues.error(format!("{path}.distance_m"), "must be positive");
            }
        }
        Block::Strength(strength) => {
            if strength.exercise.trim().is_empty() {
                issues.error(format!("{path}.exercise"), "is empty");
            } else {
                check_known(
                    issues,
                    &strength.exercise,
                    format!("{path}.exercise"),
                    catalogue,
                );
            }
            if strength.sets.is_empty() {
                issues.warn(
                    format!("{path}.sets"),
                    format!("no sets recorded for \"{}\"", strength.exercise),
                );
            }
            for (index, set) in strength.sets.iter().enumerate() {
                let set_path = format!("{path}.sets[{index}]");
                if set.weight_kg.is_some_and(|v| v < 0.0) {
                    issues.error(format!("{set_path}.weight_kg"), "is negative");
                }
                if set.reps.is_some_and(|v| v <= 0) {
                    issues.error(format!("{set_path}.reps"), "must be positive");
                }
                if set.duration_sec.is_some_and(|v| v <= 0.0) {
                    issues.error(format!("{set_path}.duration_sec"), "must be positive");
                }
                if let Some(cluster) = &set.cluster {
                    if cluster.iter().any(|&r| r <= 0) {
                        issues.error(
                            format!("{set_path}.cluster"),
                            "every rep in the chain must be positive",
                        );
                    }
                    let sum: i64 = cluster.iter().sum();
                    if let Some(total) = set.total_reps
                        && total != sum
                    {
                        issues.warn(
                            format!("{set_path}.total_reps"),
                            format!("{total} does not match the chain sum {sum}"),
                        );
                    }
                }
            }
        }
        Block::Metcon(metcon) => {
            if metcon.exercises.is_empty() {
                issues.warn(format!("{path}.exercises"), "the metcon has no movements");
            }
            for (index, entry) in metcon.exercises.iter().enumerate() {
                check_known(
                    issues,
                    &entry.name,
                    format!("{path}.exercises[{index}].name"),
                    catalogue,
                );
            }
            if let (Some(scheme), Some(rounds)) = (&metcon.scheme, &metcon.rounds)
                && rounds.len() > scheme.len()
            {
                issues.warn(
                    format!("{path}.rounds"),
                    format!(
                        "{} rounds recorded against a {}-round scheme",
                        rounds.len(),
                        scheme.len()
                    ),
                );
            }
            let mut previous_split: Option<f64> = None;
            for (index, round) in metcon.rounds.iter().flatten().enumerate() {
                if let (Some(split), Some(previous)) = (round.split_cumulative_sec, previous_split)
                    && split < previous
                {
                    issues.warn(
                        format!("{path}.rounds[{index}].split_cumulative_sec"),
                        format!(
                            "goes backwards: {split} after {previous} — splits are cumulative, not per round"
                        ),
                    );
                }
                if round.round != index as i64 + 1 {
                    issues.warn(
                        format!("{path}.rounds[{index}].round"),
                        format!("is {} at position {}", round.round, index + 1),
                    );
                }
                previous_split = round.split_cumulative_sec.or(previous_split);
            }
        }
        Block::Cooldown(_) => {}
    }
}

fn check_known(issues: &mut Issues, name: &str, path: String, catalogue: Option<&Catalogue>) {
    let Some(catalogue) = catalogue else { return };
    if name.trim().is_empty() || catalogue.resolve(name).is_some() {
        return;
    }
    issues.warn(
        path,
        format!(
            "\"{name}\" is in no catalogue entry — add it to exercises.json or it contributes nothing to the muscle map"
        ),
    );
}

#[derive(Default)]
struct Issues(Vec<ValidationIssue>);

impl Issues {
    fn error(&mut self, path: impl Into<String>, message: impl Into<String>) {
        self.push(IssueSeverity::Error, path, message);
    }

    fn warn(&mut self, path: impl Into<String>, message: impl Into<String>) {
        self.push(IssueSeverity::Warning, path, message);
    }

    fn push(
        &mut self,
        severity: IssueSeverity,
        path: impl Into<String>,
        message: impl Into<String>,
    ) {
        self.0.push(ValidationIssue {
            severity,
            path: path.into(),
            message: message.into(),
        });
    }
}
