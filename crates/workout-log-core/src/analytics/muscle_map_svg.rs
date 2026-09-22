//! Colourises the muscle-map template by splicing an inline fill after every
//! `data-muscle` attribute. Everything else in the template stays byte-identical.

use std::collections::BTreeMap;
use std::sync::LazyLock;

use regex::Regex;

pub const LOW_COLOR: &str = "#86b6ef";
pub const HIGH_COLOR: &str = "#0d366b";
pub const ZERO_COLOR: &str = "#e8e8e3";

static MUSCLE_ATTRIBUTE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"data-muscle="([a-z_]+)""#).expect("valid pattern"));

pub fn colorize(template: &str, scores: &BTreeMap<String, f64>) -> String {
    let mut out = String::with_capacity(template.len() + 1024);
    let mut cursor = 0;
    for captures in MUSCLE_ATTRIBUTE.captures_iter(template) {
        let whole = captures.get(0).expect("group 0 always exists");
        let muscle = captures.get(1).expect("group 1 is in the pattern").as_str();
        out.push_str(&template[cursor..whole.end()]);
        out.push_str(&format!(
            " style=\"fill:{}\"",
            color(scores.get(muscle).copied().unwrap_or(0.0))
        ));
        cursor = whole.end();
    }
    out.push_str(&template[cursor..]);
    out
}

pub fn color(score: f64) -> String {
    if score <= 0.0 {
        return ZERO_COLOR.to_string();
    }
    let t = score.clamp(0.0, 1.0);
    let (low, high) = (rgb(LOW_COLOR), rgb(HIGH_COLOR));
    let channel = |a: u8, b: u8| (a as f64 + (b as f64 - a as f64) * t).round() as u8;
    format!(
        "#{:02x}{:02x}{:02x}",
        channel(low.0, high.0),
        channel(low.1, high.1),
        channel(low.2, high.2)
    )
}

fn rgb(hex: &str) -> (u8, u8, u8) {
    let hex = hex.trim_start_matches('#');
    let parse = |range: std::ops::Range<usize>| {
        hex.get(range)
            .and_then(|s| u8::from_str_radix(s, 16).ok())
            .unwrap_or(0)
    };
    (parse(0..2), parse(2..4), parse(4..6))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ramp_endpoints_and_midpoint() {
        assert_eq!(color(0.0), "#e8e8e3");
        assert_eq!(color(-1.0), "#e8e8e3");
        assert_eq!(color(1.0), "#0d366b");
        assert_eq!(color(2.0), "#0d366b");
        assert_eq!(color(0.000001), "#86b6ef");
        assert_eq!(color(0.5), "#4a76ad");
    }

    #[test]
    fn splices_one_fill_per_tagged_element_and_leaves_the_rest_alone() {
        let template = r#"<svg><path data-muscle="quads" d="M0 0"/><path d="M1 1"/></svg>"#;
        let scores = BTreeMap::from([("quads".to_string(), 1.0)]);
        let out = colorize(template, &scores);
        assert_eq!(
            out,
            r#"<svg><path data-muscle="quads" style="fill:#0d366b" d="M0 0"/><path d="M1 1"/></svg>"#
        );
        // Stripping the injected attributes must reproduce the input exactly.
        let stripped = Regex::new(r#" style="fill:#[0-9a-f]{6}""#)
            .expect("valid")
            .replace_all(&out, "");
        assert_eq!(stripped, template);
    }
}
