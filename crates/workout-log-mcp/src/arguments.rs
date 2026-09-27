//! Argument coercion with the exact refusal wording the model reads back.

use std::sync::LazyLock;

use regex::Regex;

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
#[error("{0}")]
pub struct ToolFailure(pub String);

pub type ToolResult<T> = Result<T, ToolFailure>;

static ISO_DATE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^\d{4}-\d{2}-\d{2}$").expect("valid pattern"));

/// Trims, and treats blank as absent — `""` must not become a stored value.
pub fn text(value: Option<&str>) -> Option<String> {
    value
        .map(str::trim)
        .filter(|t| !t.is_empty())
        .map(str::to_string)
}

pub fn require_text(value: Option<&str>, key: &str) -> ToolResult<String> {
    text(value).ok_or_else(|| ToolFailure(format!("\"{key}\" is required")))
}

/// Dates are compared as text everywhere, so the format is enforced once here.
pub fn date(value: Option<&str>, key: &str) -> ToolResult<Option<String>> {
    let Some(value) = text(value) else {
        return Ok(None);
    };
    if !ISO_DATE.is_match(&value) || chrono::NaiveDate::parse_from_str(&value, "%Y-%m-%d").is_err()
    {
        return Err(ToolFailure(format!(
            "\"{key}\" must be an ISO date (YYYY-MM-DD), got \"{value}\""
        )));
    }
    Ok(Some(value))
}

pub fn require_date(value: Option<&str>, key: &str) -> ToolResult<String> {
    date(value, key)?.ok_or_else(|| ToolFailure(format!("\"{key}\" is required")))
}
