//! The `HH:MM` bracket timestamps from the paper log, as minutes since midnight.
//!
//! Times are wall clock without a date, so they are compared inside one session
//! only; a session that crosses midnight is not something the log records.

pub fn minutes(text: Option<&str>) -> Option<i64> {
    let text = text?.trim();
    let (hour, minute) = text.split_once(':')?;
    if hour.is_empty() || hour.len() > 2 || minute.len() != 2 {
        return None;
    }
    let hour: i64 = hour.parse().ok()?;
    let minute: i64 = minute.parse().ok()?;
    if hour > 23 || minute > 59 {
        return None;
    }
    Some(hour * 60 + minute)
}

pub fn is_valid(text: Option<&str>) -> bool {
    text.is_none() || minutes(text).is_some()
}

pub fn format(total: i64) -> String {
    format!("{:02}:{:02}", total / 60, total % 60)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_and_rejects() {
        assert_eq!(minutes(Some("08:00")), Some(480));
        assert_eq!(minutes(Some("8:05")), Some(485));
        assert_eq!(minutes(Some("23:59")), Some(1439));
        assert_eq!(minutes(Some("24:00")), None);
        assert_eq!(minutes(Some("08:60")), None);
        assert_eq!(minutes(Some("0800")), None);
        assert_eq!(minutes(Some("8:5")), None);
        assert_eq!(minutes(None), None);
        assert!(is_valid(None));
        assert_eq!(format(485), "08:05");
    }
}
