/// The A1…F2 convention: a letter naming the emphasis group, then `1` for a
/// conditioning day or `2` for a heavy one.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct CycleDay {
    pub letter: char,
    pub kind: DayKind,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum DayKind {
    Conditioning,
    Heavy,
}

impl DayKind {
    pub fn number(self) -> u8 {
        match self {
            Self::Conditioning => 1,
            Self::Heavy => 2,
        }
    }

    pub fn label(self) -> &'static str {
        match self {
            Self::Conditioning => "CrossFit",
            Self::Heavy => "Hard work",
        }
    }
}

impl CycleDay {
    /// None rather than an error for an off-convention code — the log tolerates
    /// history, and validation downgrades it to a warning.
    pub fn try_parse(code: &str) -> Option<Self> {
        let trimmed = code.trim();
        let mut chars = trimmed.chars();
        let letter = chars.next()?;
        let number = chars.next()?;
        if chars.next().is_some() || !letter.is_ascii_alphabetic() {
            return None;
        }
        let kind = match number {
            '1' => DayKind::Conditioning,
            '2' => DayKind::Heavy,
            _ => return None,
        };
        Some(Self {
            letter: letter.to_ascii_uppercase(),
            kind,
        })
    }

    pub fn code(self) -> String {
        format!("{}{}", self.letter, self.kind.number())
    }

    /// `A1 → A2 → B1 → B2 → …`
    pub fn next(self) -> Self {
        match self.kind {
            DayKind::Conditioning => Self {
                letter: self.letter,
                kind: DayKind::Heavy,
            },
            DayKind::Heavy => Self {
                letter: (self.letter as u8 + 1) as char,
                kind: DayKind::Conditioning,
            },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_the_convention_and_rejects_everything_else() {
        assert_eq!(
            CycleDay::try_parse("a1").map(CycleDay::code),
            Some("A1".into())
        );
        assert_eq!(
            CycleDay::try_parse(" C2 ").map(CycleDay::code),
            Some("C2".into())
        );
        for bad in ["A3", "AA1", "1A", "A", "", "деньА1"] {
            assert!(CycleDay::try_parse(bad).is_none(), "{bad} should not parse");
        }
    }

    #[test]
    fn walks_a1_a2_b1_b2() {
        let mut day = CycleDay::try_parse("A1").expect("parses");
        let mut walk = vec![day.code()];
        for _ in 0..3 {
            day = day.next();
            walk.push(day.code());
        }
        assert_eq!(walk, ["A1", "A2", "B1", "B2"]);
    }
}
