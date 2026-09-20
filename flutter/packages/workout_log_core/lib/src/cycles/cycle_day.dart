/// A workout code such as `A1`: the letter groups workouts by main muscle
/// emphasis, the number says which kind of day it is.
class CycleDay implements Comparable<CycleDay> {
  const CycleDay(this.letter, this.kind);

  /// Null rather than throwing: a hand-written cycle may use a code that does
  /// not follow the convention, and reading it must never fail.
  static CycleDay? tryParse(String code) {
    final match = RegExp(r'^([A-Za-z])([12])$').firstMatch(code.trim());
    if (match == null) return null;
    return CycleDay(
      match[1]!.toUpperCase(),
      DayKind.fromNumber(int.parse(match[2]!))!,
    );
  }

  final String letter;
  final DayKind kind;

  String get code => '$letter${kind.number}';

  /// `A2` → `B1`: the next slot in the A1, A2, B1, B2, … progression.
  CycleDay get next => kind == DayKind.conditioning
      ? CycleDay(letter, DayKind.heavy)
      : CycleDay(
          String.fromCharCode(letter.codeUnitAt(0) + 1),
          DayKind.conditioning,
        );

  @override
  int compareTo(CycleDay other) => code.compareTo(other.code);

  @override
  bool operator ==(Object other) =>
      other is CycleDay && other.letter == letter && other.kind == kind;

  @override
  int get hashCode => Object.hash(letter, kind);

  @override
  String toString() => code;
}

enum DayKind {
  conditioning(1, 'CrossFit'),
  heavy(2, 'Hard work');

  const DayKind(this.number, this.label);

  final int number;
  final String label;

  static DayKind? fromNumber(int number) =>
      DayKind.values.where((k) => k.number == number).firstOrNull;
}
