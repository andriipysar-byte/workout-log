import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

/// The session from `docs/02-data-model.md`, which is a real transcription: if
/// the metrics cannot read that shape they cannot read the archive.
Session _exampleSession() => Session.fromJson({
      'date': '2026-08-10',
      'cycle_day': 'F1',
      'start_time': '08:02',
      'kind': 'training',
      'blocks': [
        {
          'type': 'cardio',
          'machine': 'велотренажер',
          'duration_min': 15,
          'distance_m': 6440,
          'end_time': '08:18',
        },
        {
          'type': 'strength',
          'exercise': 'гіперекстензія',
          'sets': [
            {'reps': 12, 'weight_kg': 15},
            {'reps': 12, 'weight_kg': 30},
          ],
          'end_time': '08:28',
        },
        {
          'type': 'metcon',
          'scheme': [21, 15, 9],
          'format': 'for_time',
          'start_time': '08:30',
          'exercises': [
            {'name': 'трастери', 'weight_kg': 50},
            {'name': 'бьорпі'},
          ],
          'rounds': [
            {'round': 1, 'reps': 21, 'split_cumulative_sec': 463, 'heart_rate': 156},
            {'round': 2, 'reps': 15, 'split_cumulative_sec': 1155, 'heart_rate': 178},
            {'round': 3, 'reps': 9, 'split_cumulative_sec': 1450, 'heart_rate': 189},
          ],
          'end_time': '08:54',
        },
        {
          'type': 'strength',
          'exercise': 'присід фронтальний',
          'sets': [
            {'reps': 6, 'weight_kg': 90},
            {'reps': 3, 'weight_kg': 110},
            {'reps': 6, 'weight_kg': 70, 'is_backoff': true},
          ],
          'end_time': '09:12',
        },
        {'type': 'cooldown', 'end_time': '09:20'},
      ],
    });

void main() {
  group('SessionMetrics', () {
    test('sums tonnage and separates back-off sets', () {
      final metrics = SessionMetrics.of(_exampleSession());

      expect(metrics.tonnageKg, 12 * 15 + 12 * 30 + 6 * 90 + 3 * 110 + 6 * 70);
      expect(metrics.sets, 5);
      expect(metrics.backoffSets, 1);
      expect(metrics.topSets, 4);
    });

    test('counts rep bands by set and by slot', () {
      final metrics = SessionMetrics.of(_exampleSession());

      expect(metrics.bandSets[RepBand.volume], 2); // the two sets of twelve
      expect(metrics.bandSets[RepBand.base], 2);
      expect(metrics.bandSets[RepBand.heavy], 1);
      // The front squat slot is two sixes to one triple: a base slot.
      expect(metrics.bandSlots[RepBand.base], 1);
      expect(metrics.bandSlots[RepBand.volume], 1);
    });

    test('derives density from the bracket timestamps', () {
      final metrics = SessionMetrics.of(_exampleSession()).density;

      expect(metrics.totalMin, 78); // 08:02 → 09:20
      // 16 + 10 + 24 + 18 + 8, with the two minutes before the metcon a transition.
      expect(metrics.workMin, 76);
      expect(metrics.transitionMin, 2);
      expect(metrics.workShare, closeTo(0.974, 0.001));
    });

    test('differences the cumulative metcon splits', () {
      final metcon = SessionMetrics.of(_exampleSession()).metcons.single;

      expect(metcon.rounds.map((r) => r.splitSec), [463, 692, 295]);
      expect(metcon.rounds.first.secondsPerRep, closeTo(22.05, 0.01));
      expect(metcon.totalSec, 1450);
      expect(metcon.domain, TimeDomain.aerobic); // 24:10 on the clock
      expect(metcon.peakHeartRate, 189);
      // 32.78 s/rep on the last round against 22.05 on the first.
      expect(metcon.paceDecayPercent, 48.7);
    });

    test('a planned session with no times and no weights reads as empty', () {
      final metrics = SessionMetrics.of(Session.fromJson({
        'date': '2026-08-04',
        'cycle_day': 'D1',
        'blocks': [
          {'type': 'strength', 'exercise': 'ривок', 'sets': <Object>[]},
        ],
      }));

      expect(metrics.tonnageKg, 0);
      expect(metrics.bandSlots, isEmpty);
      expect(metrics.density.totalMin, isNull);
      expect(metrics.toJson()['density'], isNot(contains('total_min')));
    });

    test('a cardio block with no timestamps still contributes its minutes', () {
      final metrics = SessionMetrics.of(Session.fromJson({
        'date': '2026-08-04',
        'cycle_day': 'D1',
        'blocks': [
          {'type': 'cardio', 'machine': '', 'duration_min': 10},
        ],
      }));

      expect(metrics.density.workMin, 10);
    });
  });
}
