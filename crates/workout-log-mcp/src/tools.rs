//! The tool handlers. Each returns the JSON body the model reads, or a
//! [`ToolFailure`] whose message is the correction the model acts on.

use std::collections::BTreeMap;

use chrono::NaiveDate;
use serde_json::{Map, Value, json};
use workout_log_core::analytics::cycle_report::TrainingReport;
use workout_log_core::analytics::muscle_activation::{MuscleActivation, Scores, WeightingMode};
use workout_log_core::analytics::muscle_group::MuscleGroup;
use workout_log_core::analytics::progress;
use workout_log_core::analytics::session_metrics::SessionMetrics;
use workout_log_core::coding::canonical_json;
use workout_log_core::cycles::cycle_generator;
use workout_log_core::io::session_store::SessionStore;
use workout_log_core::models::block::Block;
use workout_log_core::models::cycle::CycleSession;
use workout_log_core::parsing::notation::parse_strength_sets;
use workout_log_core::validation::{IssueSeverity, has_errors, validate};
use workout_log_core::{Catalogue, Exercise, Session};

use crate::arguments::{ToolFailure, ToolResult, date, require_date, require_text, text};
use crate::requests::*;
use crate::workspace::Workspace;

/// Pretty JSON with integral floats narrowed, so the model echoes back `70`
/// rather than `70.0` — the same shape the files on disk carry.
pub fn tool_json(value: &Value) -> String {
    serde_json::to_string_pretty(&canonical_json(value)).expect("Value is always encodable")
}

fn fail(message: impl Into<String>) -> ToolFailure {
    ToolFailure(message.into())
}

// ---------------------------------------------------------------- resolution

fn catalogue_or_none(workspace: &Workspace) -> Option<Catalogue> {
    workspace.load_catalogue().ok()
}

fn require_catalogue(workspace: &Workspace) -> ToolResult<Catalogue> {
    workspace.load_catalogue().map_err(|e| fail(e.to_string()))
}

/// `id` wins; otherwise a date, and a cycle day when the date names more than
/// one file.
fn resolve_id(
    workspace: &Workspace,
    id: Option<&str>,
    on: Option<&str>,
    cycle_day: Option<&str>,
) -> ToolResult<String> {
    let ids = workspace
        .sessions
        .list_ids()
        .map_err(|e| fail(e.to_string()))?;

    if let Some(id) = text(id) {
        let id = if id.ends_with(".json") {
            id
        } else {
            format!("{id}.json")
        };
        return ids
            .contains(&id)
            .then_some(id.clone())
            .ok_or_else(|| fail(format!("no such session: {id}")));
    }

    let Some(on) = date(on, "date")? else {
        return Err(fail("pass \"id\", or \"date\" and \"cycle_day\""));
    };
    let day = text(cycle_day);
    let matches: Vec<String> = ids
        .into_iter()
        .filter(|candidate| match &day {
            Some(day) => candidate.to_lowercase() == format!("{on}_{}.json", day.to_lowercase()),
            None => candidate.starts_with(&format!("{on}_")),
        })
        .collect();

    match matches.len() {
        0 => Err(fail(format!("no session on {on}"))),
        1 => Ok(matches.into_iter().next().expect("one match")),
        _ => Err(fail(format!(
            "{on} has several sessions ({}) — pass \"id\"",
            matches.join(", ")
        ))),
    }
}

fn issues_json(session: &Session, catalogue: Option<&Catalogue>) -> Value {
    Value::Array(
        validate(session, catalogue)
            .into_iter()
            .map(|issue| {
                json!({
                    "severity": issue.severity,
                    "path": issue.path,
                    "message": issue.message,
                })
            })
            .collect(),
    )
}

/// Warnings ride along with the result; errors refuse the write.
fn require_writable(session: &Session, catalogue: Option<&Catalogue>) -> ToolResult<Value> {
    let issues = validate(session, catalogue);
    if has_errors(&issues) {
        let errors: Vec<String> = issues
            .iter()
            .filter(|i| i.severity == IssueSeverity::Error)
            .map(ToString::to_string)
            .collect();
        return Err(fail(format!(
            "refusing to write an invalid session: {}",
            errors.join("; ")
        )));
    }
    Ok(issues_json(session, catalogue))
}

fn session_value(session: &Session) -> Value {
    serde_json::to_value(session).expect("a session is always encodable")
}

// -------------------------------------------------------------- session tools

fn exercise_names(session: &Session) -> Vec<String> {
    session
        .blocks
        .iter()
        .flat_map(|block| match block {
            Block::Strength(s) => vec![s.exercise.clone()],
            Block::Metcon(m) => m.exercises.iter().map(|e| e.name.clone()).collect(),
            _ => Vec::new(),
        })
        .collect()
}

fn canonical(name: &str, catalogue: Option<&Catalogue>) -> String {
    catalogue
        .and_then(|c| c.resolve(name))
        .map_or_else(|| name.to_string(), |e| e.name.clone())
        .to_lowercase()
}

fn mentions(session: &Session, query: &str, catalogue: Option<&Catalogue>) -> bool {
    let wanted = canonical(query, catalogue);
    exercise_names(session)
        .iter()
        .any(|name| canonical(name, catalogue) == wanted)
}

fn summary(session: &Session) -> Value {
    let metrics = SessionMetrics::of(session);
    let mut json = Map::new();
    json.insert("id".into(), json!(SessionStore::id_for(session)));
    json.insert("date".into(), json!(session.date));
    json.insert("cycle_day".into(), json!(session.cycle_day));
    json.insert(
        "kind".into(),
        json!(workout_log_core::analytics::session_metrics::kind_name(
            session.kind
        )),
    );
    json.insert("exercises".into(), json!(exercise_names(session)));
    if metrics.sets > 0 {
        json.insert("sets".into(), json!(metrics.sets));
    }
    if metrics.tonnage_kg > 0.0 {
        json.insert("tonnage_kg".into(), json!(metrics.tonnage_kg));
    }
    if let Some(total) = metrics.density.total_min {
        json.insert("duration_min".into(), json!(total));
    }
    if let Some(notes) = &session.notes {
        json.insert("notes".into(), json!(notes));
    }
    Value::Object(json)
}

fn unreadable(failures: &[(String, String)]) -> Value {
    Value::Array(
        failures
            .iter()
            .map(|(id, error)| json!({"id": id, "error": error}))
            .collect(),
    )
}

pub fn list_sessions(workspace: &Workspace, args: &ListSessions) -> ToolResult<Value> {
    let from = date(args.from.as_deref(), "from")?;
    let to = date(args.to.as_deref(), "to")?;
    let limit = args.limit.unwrap_or(50);
    if limit < 0 {
        return Err(fail("\"limit\" must not be negative"));
    }
    let catalogue = catalogue_or_none(workspace);
    let loaded = workspace.load_all(from.as_deref(), to.as_deref());

    let day = text(args.cycle_day.as_deref()).map(|d| d.to_lowercase());
    let matched: Vec<&Session> = loaded
        .sessions
        .iter()
        .filter(|s| {
            day.as_ref()
                .is_none_or(|d| s.cycle_day.to_lowercase() == *d)
        })
        .filter(|s| args.kind.is_none_or(|k| s.kind == k.into()))
        .filter(|s| {
            args.exercise
                .as_deref()
                .is_none_or(|e| mentions(s, e, catalogue.as_ref()))
        })
        .collect();

    let count = matched.len();
    let mut json = Map::new();
    json.insert("folder".into(), json!(workspace.label()));
    json.insert("count".into(), json!(count));
    json.insert(
        "sessions".into(),
        Value::Array(
            matched
                .iter()
                .take(limit as usize)
                .map(|s| summary(s))
                .collect(),
        ),
    );
    if count > limit as usize {
        json.insert(
            "truncated".into(),
            json!(format!(
                "showing {limit} of {count} — narrow the range or raise \"limit\""
            )),
        );
    }
    if !loaded.failures.is_empty() {
        json.insert("unreadable".into(), unreadable(&loaded.failures));
    }
    Ok(Value::Object(json))
}

pub fn read_session(workspace: &Workspace, args: &Selector) -> ToolResult<Value> {
    let id = resolve_id(
        workspace,
        args.id.as_deref(),
        args.date.as_deref(),
        args.cycle_day.as_deref(),
    )?;
    let session = workspace
        .sessions
        .load(&id)
        .map_err(|e| fail(e.to_string()))?;
    let catalogue = catalogue_or_none(workspace);
    Ok(json!({
        "id": id,
        "session": session_value(&session),
        "issues": issues_json(&session, catalogue.as_ref()),
    }))
}

/// The cycle template that defines `cycle_day`, and the id of the cycle it came
/// from. Missing `cycles.json` is only fatal when a cycle was named.
fn template_for(
    workspace: &Workspace,
    cycle_day: &str,
    from_cycle: Option<&str>,
) -> ToolResult<(Option<String>, Option<CycleSession>)> {
    let cycles = match workspace.load_cycles() {
        Ok(cycles) => cycles,
        Err(error) => {
            return if from_cycle.is_some() {
                Err(fail(error.to_string()))
            } else {
                Ok((None, None))
            };
        }
    };
    let ids: Vec<String> = cycles.cycles.iter().map(|c| c.id.clone()).collect();

    if let Some(wanted) = from_cycle {
        let cycle = cycles
            .by_id(wanted)
            .ok_or_else(|| fail(format!("no cycle \"{wanted}\" — have {}", ids.join(", "))))?;
        let session = session_for(cycle.sessions.as_slice(), cycle_day)
            .ok_or_else(|| fail(format!("cycle \"{wanted}\" has no {cycle_day}")))?;
        return Ok((Some(cycle.id.clone()), Some(session.clone())));
    }

    let defining: Vec<&workout_log_core::Cycle> = cycles
        .cycles
        .iter()
        .filter(|c| session_for(&c.sessions, cycle_day).is_some())
        .collect();
    match defining.len() {
        0 => Ok((None, None)),
        1 => {
            let cycle = defining[0];
            Ok((
                Some(cycle.id.clone()),
                session_for(&cycle.sessions, cycle_day).cloned(),
            ))
        }
        _ => Err(fail(format!(
            "{cycle_day} is defined in {} — pass \"from_cycle\"",
            defining
                .iter()
                .map(|c| c.id.clone())
                .collect::<Vec<_>>()
                .join(", ")
        ))),
    }
}

fn session_for<'a>(sessions: &'a [CycleSession], cycle_day: &str) -> Option<&'a CycleSession> {
    sessions
        .iter()
        .find(|s| s.cycle_day.to_lowercase() == cycle_day.to_lowercase())
}

pub fn create_session(workspace: &mut Workspace, args: &CreateSession) -> ToolResult<Value> {
    let on = require_date(Some(&args.date), "date")?;
    let cycle_day = require_text(Some(&args.cycle_day), "cycle_day")?;
    let (from_cycle, template) = template_for(workspace, &cycle_day, args.from_cycle.as_deref())?;

    let parsed = NaiveDate::parse_from_str(&on, "%Y-%m-%d").expect("validated above");
    let mut session = match &template {
        Some(template) => cycle_generator::session_from_template(template, parsed, false)
            .map_err(|e| fail(e.to_string()))?,
        None => Session {
            date: on.clone(),
            cycle_day: cycle_day.clone(),
            start_time: None,
            kind: Default::default(),
            bodyweight_kg: None,
            notes: None,
            blocks: Vec::new(),
        },
    };
    session.cycle_day = cycle_day;
    session.kind = args.kind.map_or(Default::default(), Into::into);
    session.start_time = text(args.start_time.as_deref()).or(session.start_time);
    session.bodyweight_kg = args.bodyweight_kg;
    session.notes = text(args.notes.as_deref()).or(session.notes);

    let id = SessionStore::id_for(&session);
    if !args.overwrite.unwrap_or(false)
        && workspace
            .sessions
            .exists(&id)
            .map_err(|e| fail(e.to_string()))?
    {
        return Err(fail(format!(
            "{id} already exists — pass \"overwrite\": true to replace it"
        )));
    }

    let catalogue = catalogue_or_none(workspace);
    let issues = require_writable(&session, catalogue.as_ref())?;
    workspace
        .sessions
        .save(&session, None)
        .map_err(|e| fail(e.to_string()))?;
    Ok(json!({
        "id": id,
        "from_cycle": from_cycle,
        "session": session_value(&session),
        "issues": issues,
    }))
}

pub fn write_session(workspace: &mut Workspace, args: &WriteSession) -> ToolResult<Value> {
    let session: Session = serde_json::from_value(Value::Object(args.session.clone()))
        .map_err(|e| fail(format!("\"session\" is not a session document: {e}")))?;
    let previous = text(args.id.as_deref());
    let id = SessionStore::id_for(&session);

    match &previous {
        Some(previous) => {
            if !workspace
                .sessions
                .exists(previous)
                .map_err(|e| fail(e.to_string()))?
            {
                return Err(fail(format!("no such session: {previous}")));
            }
        }
        None => {
            if !args.overwrite.unwrap_or(false)
                && !workspace
                    .sessions
                    .exists(&id)
                    .map_err(|e| fail(e.to_string()))?
            {
                return Err(fail(format!(
                    "{id} does not exist yet — use create_session, or pass \"overwrite\": true"
                )));
            }
        }
    }

    let catalogue = catalogue_or_none(workspace);
    let issues = require_writable(&session, catalogue.as_ref())?;
    workspace
        .sessions
        .save(&session, previous.as_deref())
        .map_err(|e| fail(e.to_string()))?;

    let mut json = Map::new();
    json.insert("id".into(), json!(id));
    if let Some(previous) = previous.filter(|p| *p != id) {
        json.insert("renamed_from".into(), json!(previous));
    }
    json.insert("session".into(), session_value(&session));
    json.insert("issues".into(), issues);
    Ok(Value::Object(json))
}

fn strength_block(
    session: &Session,
    block_index: Option<i64>,
    exercise: Option<&str>,
    catalogue: Option<&Catalogue>,
) -> ToolResult<usize> {
    if let Some(index) = block_index {
        let count = session.blocks.len();
        if index < 0 || index as usize >= count {
            return Err(fail(format!(
                "block_index {index} is outside 0..{}",
                count.saturating_sub(1)
            )));
        }
        let index = index as usize;
        return match &session.blocks[index] {
            Block::Strength(_) => Ok(index),
            other => Err(fail(format!(
                "block {index} is a {} block, not strength",
                other.type_name()
            ))),
        };
    }

    let exercise = text(exercise).ok_or_else(|| fail("pass \"block_index\" or \"exercise\""))?;
    let wanted = canonical(&exercise, catalogue);
    let found: Vec<usize> = session
        .blocks
        .iter()
        .enumerate()
        .filter_map(|(index, block)| match block {
            Block::Strength(s) if canonical(&s.exercise, catalogue) == wanted => Some(index),
            _ => None,
        })
        .collect();
    match found.len() {
        0 => Err(fail(format!(
            "no strength block for \"{exercise}\" in this session"
        ))),
        1 => Ok(found[0]),
        _ => Err(fail(format!(
            "\"{exercise}\" appears in blocks {} — pass \"block_index\"",
            found
                .iter()
                .map(ToString::to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ))),
    }
}

pub fn log_sets(workspace: &mut Workspace, args: &LogSets) -> ToolResult<Value> {
    let id = resolve_id(
        workspace,
        args.id.as_deref(),
        args.date.as_deref(),
        args.cycle_day.as_deref(),
    )?;
    let notation = require_text(Some(&args.notation), "notation")?;
    let mut session = workspace
        .sessions
        .load(&id)
        .map_err(|e| fail(e.to_string()))?;
    let catalogue = catalogue_or_none(workspace);
    let index = strength_block(
        &session,
        args.block_index,
        args.exercise.as_deref(),
        catalogue.as_ref(),
    )?;

    let parsed = parse_strength_sets(&notation);
    if parsed.sets.is_empty() {
        let detail = if parsed.warnings.is_empty() {
            String::new()
        } else {
            format!(": {}", parsed.warnings.join("; "))
        };
        return Err(fail(format!(
            "nothing parsed out of \"{notation}\"{detail}"
        )));
    }

    let Block::Strength(strength) = &mut session.blocks[index] else {
        unreachable!("strength_block only returns strength blocks");
    };
    match args.mode {
        Some(LogMode::Append) => strength.sets.extend(parsed.sets.clone()),
        _ => strength.sets = parsed.sets.clone(),
    }
    let exercise = strength.exercise.clone();
    let sets = strength.sets.clone();

    let issues = require_writable(&session, catalogue.as_ref())?;
    workspace
        .sessions
        .save(&session, None)
        .map_err(|e| fail(e.to_string()))?;
    Ok(json!({
        "id": id,
        "block_index": index,
        "exercise": exercise,
        "sets": sets,
        "parse_warnings": parsed.warnings,
        "issues": issues,
    }))
}

pub fn delete_session(workspace: &mut Workspace, args: &DeleteSession) -> ToolResult<Value> {
    let id = resolve_id(
        workspace,
        args.id.as_deref(),
        args.date.as_deref(),
        args.cycle_day.as_deref(),
    )?;
    if !args.confirm {
        return Err(fail(format!(
            "refusing to delete {id} without \"confirm\": true"
        )));
    }
    let session = workspace
        .sessions
        .load(&id)
        .map_err(|e| fail(e.to_string()))?;
    workspace
        .sessions
        .delete(&id)
        .map_err(|e| fail(e.to_string()))?;
    Ok(json!({"deleted": id, "was": session_value(&session)}))
}

pub fn parse_notation(args: &ParseNotation) -> ToolResult<Value> {
    let notation = require_text(Some(&args.notation), "notation")?;
    let parsed = parse_strength_sets(&notation);
    Ok(json!({"sets": parsed.sets, "warnings": parsed.warnings}))
}

// ----------------------------------------------------------------- plan tools

pub fn list_exercises(workspace: &Workspace, args: &ListExercises) -> ToolResult<Value> {
    let catalogue = require_catalogue(workspace)?;
    let query = text(args.query.as_deref()).map(|q| q.to_lowercase());
    let limit = args.limit.unwrap_or(40).max(0) as usize;
    let muscle = text(args.muscle.as_deref());

    let matched: Vec<&Exercise> = catalogue
        .exercises
        .iter()
        .filter(|e| {
            query.as_ref().is_none_or(|q| {
                e.name.to_lowercase().contains(q)
                    || e.aliases().iter().any(|a| a.to_lowercase().contains(q))
            })
        })
        .filter(|e| args.pattern.is_none_or(|p| e.pattern == Some(p.into())))
        .filter(|e| args.modality.is_none_or(|m| e.modality == Some(m.into())))
        .filter(|e| {
            muscle
                .as_ref()
                .is_none_or(|m| e.primary_muscles.contains(m) || e.secondary_muscles().contains(m))
        })
        .collect();

    let count = matched.len();
    let mut json = Map::new();
    json.insert("count".into(), json!(count));
    json.insert(
        "exercises".into(),
        Value::Array(
            matched
                .iter()
                .take(limit)
                .map(|e| serde_json::to_value(e).expect("encodable"))
                .collect(),
        ),
    );
    if count > limit {
        json.insert(
            "truncated".into(),
            json!(format!("showing {limit} of {count}")),
        );
    }
    json.insert("known_muscles".into(), json!(catalogue.known_muscles()));
    Ok(Value::Object(json))
}

pub fn add_exercise(workspace: &Workspace, args: &AddExercise) -> ToolResult<Value> {
    let catalogue = require_catalogue(workspace)?;
    let name = require_text(Some(&args.name), "name")?;
    let existing = catalogue
        .exercises
        .iter()
        .find(|e| e.name.to_lowercase() == name.to_lowercase())
        .cloned();
    if existing.is_some() && !args.overwrite.unwrap_or(false) {
        return Err(fail(format!(
            "\"{name}\" is already in the catalogue — pass \"overwrite\": true to replace it"
        )));
    }
    let primary: Vec<String> = args
        .primary_muscles
        .iter()
        .filter_map(|m| text(Some(m)))
        .collect();
    if primary.is_empty() {
        return Err(fail(
            "\"primary_muscles\" is required — an exercise with none contributes nothing to the muscle map",
        ));
    }

    let secondary = args.secondary_muscles.clone();
    let known = catalogue.known_muscles();
    let unknown: Vec<String> = primary
        .iter()
        .chain(secondary.iter().flatten())
        .filter(|m| !known.contains(m))
        .cloned()
        .collect();

    let exercise = Exercise {
        name: name.clone(),
        aliases: args.aliases.clone(),
        pattern: args.pattern.map(Into::into),
        modality: args.modality.map(Into::into),
        category: args.category.map_or(Default::default(), Into::into),
        primary_muscles: primary,
        secondary_muscles: secondary,
        notes: text(args.notes.as_deref()),
        // Hand-maintained keys on the entry being replaced must survive.
        extras: existing
            .as_ref()
            .map(|e| e.extras.clone())
            .unwrap_or_default(),
    };

    let updated = catalogue.with_exercise(exercise.clone());
    workspace
        .save_catalogue(&updated)
        .map_err(|e| fail(e.to_string()))?;

    let mut json = Map::new();
    json.insert(
        "saved".into(),
        json!(workspace.catalogue_file().display().to_string()),
    );
    json.insert("replaced".into(), json!(existing.is_some()));
    json.insert(
        "exercise".into(),
        serde_json::to_value(&exercise).expect("encodable"),
    );
    if !unknown.is_empty() {
        json.insert(
            "new_muscle_names".into(),
            json!({
                "names": unknown,
                "note": "outside the vocabulary the rest of the catalogue uses, so the muscle map will not colour them — check the spelling against known_muscles",
            }),
        );
    }
    Ok(Value::Object(json))
}

pub fn list_cycles(workspace: &Workspace, args: &ListCycles) -> ToolResult<Value> {
    let cycles = workspace.load_cycles().map_err(|e| fail(e.to_string()))?;
    let ids: Vec<String> = cycles.cycles.iter().map(|c| c.id.clone()).collect();

    if let Some(wanted) = text(args.id.as_deref()) {
        let cycle = cycles
            .by_id(&wanted)
            .ok_or_else(|| fail(format!("no cycle \"{wanted}\" — have {}", ids.join(", "))))?;
        return Ok(json!({"cycle": serde_json::to_value(cycle).expect("encodable")}));
    }

    Ok(json!({
        "cycles": cycles.cycles.iter().map(|cycle| json!({
            "id": cycle.id,
            "name": cycle.name,
            "training_days": cycle.training_days,
            "start_date": cycle.start_date,
            "days": cycle.sessions.iter().map(|session| json!({
                "cycle_day": session.cycle_day,
                "title": session.title,
                "weekday": session.weekday,
                "blocks": session.blocks.iter().map(|block| {
                    block.exercise.clone()
                        .or_else(|| block.machine.clone())
                        .unwrap_or_else(|| block.block_type.clone())
                }).collect::<Vec<_>>(),
            })).collect::<Vec<_>>(),
        })).collect::<Vec<_>>(),
    }))
}

pub fn generate_cycle(workspace: &mut Workspace, args: &GenerateCycle) -> ToolResult<Value> {
    let cycles = workspace.load_cycles().map_err(|e| fail(e.to_string()))?;
    let ids: Vec<String> = cycles.cycles.iter().map(|c| c.id.clone()).collect();
    let cycle_id = require_text(Some(&args.cycle_id), "cycle_id")?;
    let mut expanded = cycles
        .by_id(&cycle_id)
        .ok_or_else(|| fail(format!("no cycle \"{cycle_id}\" — have {}", ids.join(", "))))?
        .clone();
    if let Some(start) = date(args.start_date.as_deref(), "start_date")? {
        expanded.start_date = start;
    }

    let generated = cycle_generator::generate(&expanded).map_err(|e| fail(e.to_string()))?;
    let (force, dry_run) = (args.force.unwrap_or(false), args.dry_run.unwrap_or(false));

    let mut rows = Vec::new();
    for session in &generated {
        let id = SessionStore::id_for(session);
        let exists = workspace
            .sessions
            .exists(&id)
            .map_err(|e| fail(e.to_string()))?;
        let skipped = exists && !force;
        let status = if skipped {
            "skipped (exists — pass \"force\": true to overwrite)"
        } else if dry_run {
            "would write"
        } else if exists {
            "overwritten"
        } else {
            "written"
        };
        if !skipped && !dry_run {
            workspace
                .sessions
                .save(session, None)
                .map_err(|e| fail(e.to_string()))?;
        }
        rows.push(json!({"id": id, "cycle_day": session.cycle_day, "status": status}));
    }

    Ok(json!({
        "cycle": expanded.id,
        "start_date": expanded.start_date,
        "dry_run": dry_run,
        "sessions": rows,
    }))
}

// ------------------------------------------------------------- analysis tools

fn muscle_json(scores: &Scores, mode: WeightingMode) -> Value {
    let mut ordered: Vec<(&String, &f64)> = scores.iter().collect();
    ordered.sort_by(|a, b| {
        b.1.partial_cmp(a.1)
            .unwrap_or(std::cmp::Ordering::Equal)
            .then(a.0.cmp(b.0))
    });

    let mut muscles = Map::new();
    for (name, score) in &ordered {
        muscles.insert((*name).clone(), json!((*score * 1000.0).round() / 1000.0));
    }
    let mut groups: BTreeMap<&str, f64> = BTreeMap::new();
    for (name, score) in &ordered {
        if let Some(group) = MuscleGroup::of(name) {
            *groups.entry(group.name()).or_insert(0.0) += **score;
        }
    }
    let mut json = Map::new();
    json.insert("weighting".into(), json!(mode.wire()));
    json.insert("muscles".into(), Value::Object(muscles));
    json.insert(
        "groups".into(),
        Value::Object(
            groups
                .into_iter()
                .map(|(k, v)| (k.to_string(), json!((v * 1000.0).round() / 1000.0)))
                .collect(),
        ),
    );
    if let Some(dominant) = MuscleGroup::dominant(scores) {
        json.insert("dominant_group".into(), json!(dominant.name()));
    }
    Value::Object(json)
}

pub fn analyze_session(workspace: &Workspace, args: &AnalyzeSession) -> ToolResult<Value> {
    let id = resolve_id(
        workspace,
        args.id.as_deref(),
        args.date.as_deref(),
        args.cycle_day.as_deref(),
    )?;
    let session = workspace
        .sessions
        .load(&id)
        .map_err(|e| fail(e.to_string()))?;
    let mode: WeightingMode = args.mode.map_or(WeightingMode::SetCount, Into::into);
    let catalogue = catalogue_or_none(workspace);

    let mut json = Map::new();
    json.insert("id".into(), json!(id));
    json.insert("metrics".into(), SessionMetrics::of(&session).to_json());
    if let Some(catalogue) = &catalogue {
        let scores = MuscleActivation::default().for_session(&session, catalogue, mode);
        json.insert("muscles".into(), muscle_json(&scores, mode));
    }
    json.insert("issues".into(), issues_json(&session, catalogue.as_ref()));
    Ok(Value::Object(json))
}

pub fn analyze_training(workspace: &Workspace, args: &AnalyzeTraining) -> ToolResult<Value> {
    let from = date(args.from.as_deref(), "from")?;
    let to = date(args.to.as_deref(), "to")?;
    let catalogue = require_catalogue(workspace)?;
    let loaded = workspace.load_all(from.as_deref(), to.as_deref());
    if loaded.sessions.is_empty() {
        return Err(fail("no sessions in that range"));
    }
    let report = TrainingReport::of(
        &loaded.sessions,
        &catalogue,
        args.cycle_length_days.unwrap_or(12),
        args.volume_share_threshold.unwrap_or(0.3),
    );
    let mut json = report.to_json();
    let object = json.as_object_mut().expect("the report is an object");
    if !args.include_load.unwrap_or(true) {
        object.remove("load");
    }
    if !loaded.failures.is_empty() {
        object.insert("unreadable".into(), unreadable(&loaded.failures));
    }
    Ok(json)
}

pub fn exercise_progress(workspace: &Workspace, args: &ExerciseProgressArgs) -> ToolResult<Value> {
    let exercise = require_text(Some(&args.exercise), "exercise")?;
    let from = date(args.from.as_deref(), "from")?;
    let to = date(args.to.as_deref(), "to")?;
    let catalogue = catalogue_or_none(workspace);
    let loaded = workspace.load_all(from.as_deref(), to.as_deref());
    let report = progress::report(
        &loaded.sessions,
        &exercise,
        catalogue.as_ref(),
        args.by_pattern.unwrap_or(false),
    );
    let mut json = report.to_json();
    if report.series.is_empty() {
        json.as_object_mut().expect("an object").insert(
            "note".into(),
            json!("no sets recorded for this lift in the range — list_exercises will say whether the name is the catalogue one"),
        );
    }
    Ok(json)
}

pub fn muscle_activation(workspace: &Workspace, args: &MuscleActivationArgs) -> ToolResult<Value> {
    let catalogue = require_catalogue(workspace)?;
    let mode: WeightingMode = args.mode.map_or(WeightingMode::SetCount, Into::into);
    let activation = MuscleActivation::default();

    // `cycle_day` alone does not select a session; it only disambiguates a date.
    if args.id.is_some() || args.date.is_some() {
        let id = resolve_id(
            workspace,
            args.id.as_deref(),
            args.date.as_deref(),
            args.cycle_day.as_deref(),
        )?;
        let session = workspace
            .sessions
            .load(&id)
            .map_err(|e| fail(e.to_string()))?;
        let scores = activation.for_session(&session, &catalogue, mode);
        let mut json = muscle_json(&scores, mode);
        json.as_object_mut()
            .expect("an object")
            .insert("id".into(), json!(id));
        return Ok(json);
    }

    let from = date(args.from.as_deref(), "from")?;
    let to = date(args.to.as_deref(), "to")?;
    let loaded = workspace.load_all(from.as_deref(), to.as_deref());
    if loaded.sessions.is_empty() {
        return Err(fail("no sessions in that range"));
    }
    let scores = activation.for_sessions(&loaded.sessions, &catalogue, mode);
    let mut json = muscle_json(&scores, mode);
    let object = json.as_object_mut().expect("an object");
    object.insert("sessions".into(), json!(loaded.sessions.len()));
    object.insert("from".into(), json!(loaded.sessions[0].date));
    object.insert(
        "to".into(),
        json!(loaded.sessions[loaded.sessions.len() - 1].date),
    );
    Ok(json)
}

pub fn validate_archive(workspace: &Workspace, args: &ValidateArchive) -> ToolResult<Value> {
    let from = date(args.from.as_deref(), "from")?;
    let to = date(args.to.as_deref(), "to")?;
    let catalogue = catalogue_or_none(workspace);
    let loaded = workspace.load_all(from.as_deref(), to.as_deref());

    let (mut errors, mut warnings) = (0, 0);
    let mut rows = Vec::new();
    for session in &loaded.sessions {
        let issues = validate(session, catalogue.as_ref());
        if issues.is_empty() {
            continue;
        }
        errors += issues
            .iter()
            .filter(|i| i.severity == IssueSeverity::Error)
            .count();
        warnings += issues
            .iter()
            .filter(|i| i.severity == IssueSeverity::Warning)
            .count();
        rows.push(json!({
            "id": SessionStore::id_for(session),
            "issues": issues_json(session, catalogue.as_ref()),
        }));
    }

    let mut json = Map::new();
    json.insert("checked".into(), json!(loaded.sessions.len()));
    json.insert("errors".into(), json!(errors));
    json.insert("warnings".into(), json!(warnings));
    json.insert("sessions".into(), Value::Array(rows));
    if !loaded.failures.is_empty() {
        json.insert("unreadable".into(), unreadable(&loaded.failures));
    }
    Ok(Value::Object(json))
}
