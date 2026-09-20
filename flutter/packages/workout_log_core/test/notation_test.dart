import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

void main() {
  test('ramp expands to one set per weight at the fixed rep count', () {
    final parsed = Notation.parseStrengthSets('6 × [70, 80, 90, 100, 110]');
    expect(parsed.sets, hasLength(5));
    expect(parsed.sets.map((s) => s.reps), everyElement(6));
    expect(parsed.sets.map((s) => s.weightKg), [70, 80, 90, 100, 110]);
    expect(parsed.warnings, isEmpty);
  });

  test('a trailing group is marked as back-off sets', () {
    final parsed =
        Notation.parseStrengthSets('6 × [70, 80, 90, 100, 110] + 6 × [30]');
    expect(parsed.sets, hasLength(6));
    expect(parsed.sets.last.weightKg, 30);
    expect(parsed.sets.last.isBackoff, isTrue);
    expect(parsed.sets.take(5).map((s) => s.isBackoff), everyElement(isNull));
  });

  test('per-set rep overrides beat the group count', () {
    final parsed =
        Notation.parseStrengthSets('6 × [30, 60, 80(4), 86(2), 90(1)]');
    expect(parsed.sets.map((s) => s.reps), [6, 6, 4, 2, 1]);
  });

  test('cluster with an explicit total', () {
    final parsed = Notation.parseStrengthSets('5+5+4+3+3 (20)');
    expect(parsed.sets, hasLength(1));
    expect(parsed.sets.single.cluster, [5, 5, 4, 3, 3]);
    expect(parsed.sets.single.totalReps, 20);
  });

  test('cluster total is derived from the sum when unstated', () {
    final parsed = Notation.parseStrengthSets('4+4+3+2+2+2+2+1');
    expect(parsed.sets.single.totalReps, 20);
  });

  test('timed holds become duration sets with no reps', () {
    final parsed = Notation.parseStrengthSets('4 × [54c, 40c, 36c, 42c]');
    expect(parsed.sets, hasLength(4));
    expect(parsed.sets.map((s) => s.durationSec), [54, 40, 36, 42]);
    expect(parsed.sets.map((s) => s.reps), everyElement(isNull));
  });

  test('a bare brace list with the Cyrillic seconds suffix parses', () {
    final parsed = Notation.parseStrengthSets('{60с, 70с, 30с, 35с}');
    expect(parsed.sets, hasLength(4));
    expect(parsed.sets.map((s) => s.durationSec), [60, 70, 30, 35]);
  });

  test('all six multiplier characters are accepted', () {
    for (final multiplier in ['×', 'x', 'X', 'х', 'Х', '*']) {
      final parsed = Notation.parseStrengthSets('6 $multiplier [70, 80]');
      expect(parsed.sets.map((s) => s.weightKg), [70, 80],
          reason: 'multiplier "$multiplier"');
      expect(parsed.sets.map((s) => s.reps), everyElement(6));
    }
  });

  test('all four seconds suffixes are accepted', () {
    for (final suffix in ['c', 'C', 'с', 'С']) {
      final parsed = Notation.parseStrengthSets('3 × [40$suffix]');
      expect(parsed.sets.single.durationSec, 40, reason: 'suffix "$suffix"');
    }
  });

  test('decimal weights use a dot; a comma always separates values', () {
    expect(
      Notation.parseStrengthSets('1 × [47.5]').sets.single.weightKg,
      47.5,
    );
    expect(
      Notation.parseStrengthSets('1 × [47,5]').sets.map((s) => s.weightKg),
      [47, 5],
    );
  });

  test('empty input yields nothing and warns about nothing', () {
    final parsed = Notation.parseStrengthSets('   ');
    expect(parsed.sets, isEmpty);
    expect(parsed.warnings, isEmpty);
  });

  test('warn-never-block: a bad value still yields its siblings', () {
    final parsed = Notation.parseStrengthSets('6 × [70, банан, 90]');
    expect(parsed.sets.map((s) => s.weightKg), [70, 90]);
    expect(parsed.warnings, hasLength(1));
    expect(parsed.warnings.single, contains('банан'));
  });

  test('an unparseable line warns instead of throwing', () {
    final parsed = Notation.parseStrengthSets('нічого корисного');
    expect(parsed.sets, isEmpty);
    expect(parsed.warnings, isNotEmpty);
  });

  test('parsed sets survive a JSON round-trip inside a session', () {
    final parsed = Notation.parseStrengthSets('6 × [70, 80]');
    final encoded = SessionCoding.encode(
      Session(
        date: '2026-07-21',
        cycleDay: 'A1',
        blocks: [
          StrengthBlock(exercise: 'присід фронтальний', sets: parsed.sets),
        ],
      ),
    );
    final block = SessionCoding.decode(encoded).blocks.single as StrengthBlock;
    expect(block.sets, hasLength(2));
    expect(block.sets.first.weightKg, 70);
    expect(block.sets.first.reps, 6);
  });
}
