import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

Session _squatSession(
  String date,
  String cycleDay,
  List<Map<String, dynamic>> sets, {
  String exercise = 'присід фронтальний',
}) =>
    Session.fromJson({
      'date': date,
      'cycle_day': cycleDay,
      'blocks': [
        {'type': 'strength', 'exercise': exercise, 'sets': sets},
      ],
    });

void main() {
  group('OneRepMax', () {
    test('a single is its own maximum', () {
      expect(OneRepMax.epley(100, 1), 100);
      expect(OneRepMax.estimate(100, 1), 100);
    });

    test('estimates sit between Epley and Brzycki', () {
      final epley = OneRepMax.epley(100, 6)!; // 120
      final brzycki = OneRepMax.brzycki(100, 6)!; // ~116.1
      final estimate = OneRepMax.estimate(100, 6)!;

      expect(estimate, greaterThan(brzycki));
      expect(estimate, lessThan(epley));
    });

    test('a bodyweight or unrecorded set has no estimate', () {
      expect(OneRepMax.estimate(null, 6), isNull);
      expect(OneRepMax.estimate(100, null), isNull);
      expect(OneRepMax.estimate(100, 0), isNull);
    });
  });

  group('ExerciseProgress', () {
    final catalogue = loadCatalogue();

    test('splits one lift into a track per rep band (P6)', () {
      final report = ExerciseProgress.report(
        [
          _squatSession('2026-07-01', 'C2', [
            {'reps': 6, 'weight_kg': 100},
            {'reps': 3, 'weight_kg': 110},
          ]),
          _squatSession('2026-07-13', 'C2', [
            {'reps': 6, 'weight_kg': 105},
            {'reps': 3, 'weight_kg': 112.5},
          ]),
        ],
        exercise: 'присід фронтальний',
        catalogue: catalogue,
      );

      expect(report.series.map((s) => s.band), [RepBand.heavy, RepBand.base]);
      final base = report.series.firstWhere((s) => s.band == RepBand.base);
      expect(base.points.map((p) => p.weightKg), [100, 105]);
      expect(base.trendKg, greaterThan(0));
    });

    test('the top set is the signal, back-offs are volume beside it (P2)', () {
      final report = ExerciseProgress.report(
        [
          _squatSession('2026-07-01', 'C2', [
            {'reps': 6, 'weight_kg': 90},
            {'reps': 6, 'weight_kg': 100},
            {'reps': 6, 'weight_kg': 70, 'is_backoff': true},
          ]),
        ],
        exercise: 'присід фронтальний',
        catalogue: catalogue,
      );

      final point = report.series.single.points.single;
      expect(point.weightKg, 100);
      expect(point.backoffSets, 1);
      expect(point.backoffTonnageKg, 420);
      expect(point.sets, 3);
    });

    test('an alias resolves to the canonical lift', () {
      final report = ExerciseProgress.report(
        [
          _squatSession('2026-07-01', 'C2', [
            {'reps': 6, 'weight_kg': 100},
          ], exercise: 'фр. присід'),
        ],
        exercise: 'присід фронтальний',
        catalogue: catalogue,
      );

      expect(report.resolved, 'присід фронтальний');
      expect(report.series.single.points, hasLength(1));
    });

    test('byPattern gathers the variants of one pattern (P8)', () {
      final sessions = [
        _squatSession('2026-07-01', 'C2', [
          {'reps': 6, 'weight_kg': 100},
        ]),
        _squatSession('2026-07-08', 'C2', [
          {'reps': 6, 'weight_kg': 80},
        ], exercise: 'присід на плечах'),
      ];

      final single = ExerciseProgress.report(
        sessions,
        exercise: 'присід фронтальний',
        catalogue: catalogue,
      );
      final byPattern = ExerciseProgress.report(
        sessions,
        exercise: 'присід фронтальний',
        catalogue: catalogue,
        byPattern: true,
      );

      expect(single.variants, ['присід фронтальний']);
      expect(byPattern.pattern, MovementPattern.squat);
      expect(byPattern.variants, hasLength(2));
      expect(byPattern.series, hasLength(2));
    });

    test('an exercise the catalogue does not know is still tracked', () {
      final report = ExerciseProgress.report(
        [
          _squatSession('2026-07-01', 'C2', [
            {'reps': 5, 'weight_kg': 60},
          ], exercise: 'вправа якої немає'),
        ],
        exercise: 'вправа якої немає',
        catalogue: catalogue,
      );

      expect(report.resolved, isNull);
      expect(report.series.single.points.single.weightKg, 60);
    });
  });
}
