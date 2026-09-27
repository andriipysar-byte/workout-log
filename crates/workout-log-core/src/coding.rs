//! Canonical JSON: sorted keys, two-space indent, trailing newline.

use serde_json::{Map, Value};

/// Sorts every nested object by key and narrows integral floats to integers.
///
/// The narrowing matters: the format was first written by Swift's `JSONEncoder`,
/// which wrote `Double(70)` as `70`. Without it every weight in `data/` would
/// gain a `.0` on first save.
pub fn canonical_json(value: &Value) -> Value {
    match value {
        Value::Object(map) => {
            let mut keys: Vec<&String> = map.keys().collect();
            keys.sort();
            // Re-inserted in sorted order rather than relying on `Map`'s backing
            // type: `serde_json/preserve_order` is additive across the dependency
            // graph, so any crate enabling it would otherwise silently reorder us.
            let mut sorted = Map::with_capacity(map.len());
            for key in keys {
                sorted.insert(key.clone(), canonical_json(&map[key]));
            }
            Value::Object(sorted)
        }
        Value::Array(items) => Value::Array(items.iter().map(canonical_json).collect()),
        Value::Number(number) => match number.as_f64() {
            Some(float) if number.is_f64() && float.is_finite() && float.fract() == 0.0 => {
                Value::Number((float as i64).into())
            }
            _ => value.clone(),
        },
        _ => value.clone(),
    }
}

pub fn encode_json(value: &Value) -> String {
    format!(
        "{}\n",
        serde_json::to_string_pretty(&canonical_json(value)).expect("Value is always encodable")
    )
}

pub fn decode_json(text: &str) -> Result<Value, serde_json::Error> {
    serde_json::from_str(text)
}

/// The keys a model has no field for, kept so a read-modify-write of a
/// hand-maintained file cannot silently delete them.
pub fn unmodelled_keys(json: &Map<String, Value>, modelled: &[&str]) -> Map<String, Value> {
    json.iter()
        .filter(|(key, _)| !modelled.contains(&key.as_str()))
        .map(|(key, value)| (key.clone(), value.clone()))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn sorts_keys_at_every_depth() {
        let input = json!({"b": 1, "a": {"d": 2, "c": [{"f": 3, "e": 4}]}});
        assert_eq!(
            encode_json(&input),
            "{\n  \"a\": {\n    \"c\": [\n      {\n        \"e\": 4,\n        \"f\": 3\n      }\n    ],\n    \"d\": 2\n  },\n  \"b\": 1\n}\n"
        );
    }

    #[test]
    fn narrows_integral_floats_but_keeps_real_ones() {
        assert_eq!(encode_json(&json!({"w": 70.0})), "{\n  \"w\": 70\n}\n");
        assert_eq!(encode_json(&json!({"w": 72.5})), "{\n  \"w\": 72.5\n}\n");
    }

    #[test]
    fn writes_non_ascii_unescaped() {
        assert_eq!(
            encode_json(&json!({"e": "ривок"})),
            "{\n  \"e\": \"ривок\"\n}\n"
        );
    }
}
