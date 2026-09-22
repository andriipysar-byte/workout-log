//! Parses the terse paper-log notation into model sets. Pure domain logic
//! (ADR-004); see `docs/03-log-notation.md` for the full grammar.
//!
//! Supported strength forms:
//! ```text
//! 6 × [70, 80, 90, 100, 110]          fixed reps × ascending weights → 5 sets
//! 6 × [70, 80] + 6 × [30]             trailing groups are back-off sets
//! 6 × [30, 60, 80(4), 86(2), 90(1)]   per-set rep override in parentheses
//! 5+5+4+3+3 (20)                      cluster set: chain + total in parens
//! 4 × [54c, 40c, 36c, 42c]            timed holds (c/с = seconds) → duration sets
//! {60c, 70c, 30c, 35c}                bare bracketed list, no count prefix
//! ```

use std::sync::LazyLock;

use regex::Regex;

use crate::models::work_set::WorkSet;

/// Warn-never-block: a line we can't fully parse still yields the sets it can.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct ParsedSets {
    pub sets: Vec<WorkSet>,
    pub warnings: Vec<String>,
}

/// The Cyrillic `х`/`Х` and `с`/`С` are distinct code points from their Latin
/// lookalikes; the paper log uses both, so both must be accepted.
const MULTIPLIERS: [char; 6] = ['×', 'x', 'X', 'х', 'Х', '*'];
const SECONDS_SUFFIX: [char; 4] = ['c', 'C', 'с', 'С'];

static TRAILING_PAREN_NUMBER: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"\(\s*(\d+)\s*\)\s*$").expect("valid pattern"));

fn is_multiplier(ch: char) -> bool {
    MULTIPLIERS.contains(&ch)
}

pub fn parse_strength_sets(raw: &str) -> ParsedSets {
    let mut warnings = Vec::new();
    let line = raw.trim();
    if line.is_empty() {
        return ParsedSets::default();
    }

    let has_multiplier = line.chars().any(is_multiplier);
    let has_bracket = line.contains('[') || line.contains('{');

    if !has_multiplier && !has_bracket && line.contains('+') {
        return parse_cluster(line, warnings);
    }

    // First group = ramp/straight sets, later top-level '+' groups = back-offs.
    let mut sets = Vec::new();
    for (index, group) in split_top_level(line, '+').iter().enumerate() {
        sets.extend(parse_group(group, index > 0, &mut warnings));
    }
    if sets.is_empty() && warnings.is_empty() {
        warnings.push(format!("could not parse: {line}"));
    }
    ParsedSets { sets, warnings }
}

fn parse_cluster(line: &str, mut warnings: Vec<String>) -> ParsedSets {
    let mut body = line.to_string();
    let mut total = None;
    if let Some(found) = TRAILING_PAREN_NUMBER.find(line) {
        total = TRAILING_PAREN_NUMBER
            .captures(line)
            .and_then(|c| c[1].parse::<i64>().ok());
        body = format!("{}{}", &line[..found.start()], &line[found.end()..]);
    }
    let parts: Vec<&str> = body
        .split('+')
        .filter(|p| !p.is_empty())
        .map(str::trim)
        .collect();
    let reps: Vec<i64> = parts.iter().filter_map(|p| p.parse::<i64>().ok()).collect();
    if reps.is_empty() || reps.len() != parts.len() {
        warnings.push(format!("could not parse cluster: {line}"));
        return ParsedSets {
            sets: Vec::new(),
            warnings,
        };
    }
    let total_reps = total.unwrap_or_else(|| reps.iter().sum());
    ParsedSets {
        sets: vec![WorkSet {
            cluster: Some(reps),
            total_reps: Some(total_reps),
            ..WorkSet::default()
        }],
        warnings,
    }
}

fn parse_group(group: &str, is_backoff: bool, warnings: &mut Vec<String>) -> Vec<WorkSet> {
    let mut count = None;
    let mut list_part = group.to_string();

    if let Some((byte_index, multiplier)) = group.char_indices().find(|(_, c)| is_multiplier(*c)) {
        let count_text = group[..byte_index].trim();
        match count_text.parse::<i64>() {
            Ok(parsed) => count = Some(parsed),
            Err(_) => warnings.push(format!(
                "unrecognised rep count \"{count_text}\" in \"{group}\""
            )),
        }
        list_part = group[byte_index + multiplier.len_utf8()..].to_string();
    }

    let trimmed = list_part.trim_matches(|c| matches!(c, '[' | ']' | '{' | '}' | ' ' | '\t'));
    let items: Vec<&str> = trimmed
        .split(',')
        .map(str::trim)
        .filter(|s| !s.is_empty())
        .collect();

    if items.is_empty() {
        warnings.push(format!("no values in \"{group}\""));
        return Vec::new();
    }

    let backoff = if is_backoff { Some(true) } else { None };
    let mut out = Vec::new();
    for item in items {
        if let Some(seconds) = parse_duration(item) {
            out.push(WorkSet {
                duration_sec: Some(seconds),
                is_backoff: backoff,
                ..WorkSet::default()
            });
            continue;
        }
        match parse_weight(item) {
            Some((weight, reps)) => out.push(WorkSet {
                weight_kg: Some(weight),
                reps: reps.or(count),
                is_backoff: backoff,
                ..WorkSet::default()
            }),
            None => warnings.push(format!("unparseable value \"{item}\" in \"{group}\"")),
        }
    }
    out
}

/// "54c" / "54с" (Latin or Cyrillic suffix) → 54 seconds; None if not a duration token.
fn parse_duration(token: &str) -> Option<f64> {
    let last = token.chars().last()?;
    if !SECONDS_SUFFIX.contains(&last) {
        return None;
    }
    token[..token.len() - last.len_utf8()]
        .trim()
        .replace(',', ".")
        .parse()
        .ok()
}

/// "90" or "90(2)" or "47.5" → (weight, optional rep override).
fn parse_weight(token: &str) -> Option<(f64, Option<i64>)> {
    let mut text = token.to_string();
    let mut reps = None;
    if let Some(found) = TRAILING_PAREN_NUMBER.find(token) {
        reps = TRAILING_PAREN_NUMBER
            .captures(token)
            .and_then(|c| c[1].parse::<i64>().ok());
        text = format!("{}{}", &token[..found.start()], &token[found.end()..]);
    }
    let weight = text.trim().replace(',', ".").parse().ok()?;
    Some((weight, reps))
}

/// Split on `separator` at bracket depth 0 (so weights inside [...] stay intact).
fn split_top_level(input: &str, separator: char) -> Vec<String> {
    let mut result = Vec::new();
    let mut current = String::new();
    let mut depth = 0usize;
    for ch in input.chars() {
        match ch {
            '[' | '{' => {
                depth += 1;
                current.push(ch);
            }
            ']' | '}' => {
                depth = depth.saturating_sub(1);
                current.push(ch);
            }
            c if c == separator && depth == 0 => {
                result.push(std::mem::take(&mut current));
            }
            _ => current.push(ch),
        }
    }
    result.push(current);
    result
        .into_iter()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .collect()
}
