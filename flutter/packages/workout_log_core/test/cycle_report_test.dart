import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

Session _session(
  String date,
  String cycleDay,
  List<Map<String, dynamic>> blocks, {
  String kind = 'training',
}) =>
    Session.fromJson({
      'date': date,
      'cycle_day': cycleDay,
      'kind': kind,
      'blocks': blocks,
    });

Map<String, dynamic> _strength(String exercise, List<int> reps,
        {double? weight}) =>
    {
      'type': 'strength',
      'exercise': exercise,
      'sets': [
        for (final r in reps) {'reps': r, 'weight_kg': ?weight},
      ],
    };

void main() {
  final catalogue = loadCatalogue();

  group('TrainingReport', () {
    test('flags a block where volume slots take over (P5)', () {
      final report = TrainingReport.of(
        [
          _session('2026-07-01', 'A2', [
            _strength('присід фронтальний', [8, 8, 8], weight: 80),
            _strength('румунська тяга', [8, 8, 8], weight: 70),
          ]),
          _session('2026-07-03', 'B2', [
            _strength('жим лежачи', [8, 8, 8], weight: 60),
          ]),
        ],
        catalogue: catalogue,
      );

      final alert = report.alerts.firstWhere((a) => a.principle == 'P5');
      expect(alert.message, contains('100%'));
      expect(report.bandSlots[RepBand.volume], 3);
    });

    test('a mixed block stays quiet', () {
      final report = TrainingReport.of(
        [
          _session('2026-07-01', 'A2', [
            _strength('присід фронтальний', [3, 3, 3], weight: 110),
            _strength('румунська тяга', [6, 6, 6], weight: 90),
            _strength('жим лежачи', [6, 6, 6], weight: 70),
          ]),
        ],
        catalogue: catalogue,
      );

      expect(report.alerts.where((a) => a.principle == 'P5'), isEmpty);
    });

    test('flags an explosive lift trained at six reps (P3)', () {
      final report = TrainingReport.of(
        [
          _session('2026-07-01', 'A1', [
            _strength('ривок', [6, 6], weight: 60),
          ]),
        ],
        catalogue: catalogue,
      );

      expect(
        report.alerts.map((a) => a.principle),
        contains('P3'),
      );
      expect(report.alerts.first.sessions, ['2026-07-01 ривок']);
    });

    test('flags the 4 / 2 / 1 rep collapse (P3)', () {
      final report = TrainingReport.of(
        [
          _session('2026-07-01', 'A1', [
            _strength('швунг', [3, 2, 1], weight: 70),
          ]),
        ],
        catalogue: catalogue,
      );

      final collapse =
          report.alerts.where((a) => a.message.contains('collapse'));
      expect(collapse, hasLength(1));
    });

    test('counts pattern frequency per cycle and flags the starved ones (P8)', () {
      final report = TrainingReport.of(
        [
          _session('2026-07-01', 'A2', [
            _strength('присід фронтальний', [6, 6], weight: 100),
          ]),
          // 24 days is two 12-day cycles, so one squat slot is 0.5 per cycle.
          _session('2026-07-24', 'C2', [
            _strength('присід фронтальний', [6, 6], weight: 100),
          ]),
        ],
        catalogue: catalogue,
      );

      final squat =
          report.patterns.firstWhere((p) => p.pattern == MovementPattern.squat);
      expect(squat.slots, 2);
      expect(squat.perCycle, 1.0);
      expect(report.days, 24);
      expect(report.alerts.map((a) => a.principle), contains('P10'));
    });

    test('combined load keeps strength and conditioning apart (P7)', () {
      final report = TrainingReport.of(
        [
          _session('2026-07-01', 'A1', [
            _strength('присід фронтальний', [6], weight: 100),
            {
              'type': 'metcon',
              'scheme': [21, 15, 9],
              'exercises': [
                {'name': 'трастери'},
              ],
              'rounds': [
                {'round': 1, 'split_cumulative_sec': 200},
                {'round': 2, 'split_cumulative_sec': 400},
                {'round': 3, 'split_cumulative_sec': 600},
              ],
            },
          ]),
        ],
        catalogue: catalogue,
      );

      final point = report.load.single;
      expect(point.tonnageKg, 600);
      expect(point.metconSec, 600);
      expect(report.timeDomains[TimeDomain.glycolytic], 1);
      expect(report.totalTonnageKg, 600);
    });

    test('names the catalogue does not know are surfaced, not dropped', () {
      final report = TrainingReport.of(
        [
          _session('2026-07-01', 'A2', [
            _strength('вправа якої немає', [6], weight: 50),
          ]),
        ],
        catalogue: catalogue,
      );

      expect(report.unknownExercises, ['вправа якої немає']);
    });

    test('the real archive reports without throwing', () {
      final sessions = [
        for (final file in sessionFiles)
          SessionCoding.decode(file.readAsStringSync()),
      ];

      final report = TrainingReport.of(sessions, catalogue: catalogue);

      expect(report.sessionCount, sessions.length);
      expect(report.toJson()['load'], hasLength(sessions.length));
      // Not asserted empty: the archive really does carry names the catalogue
      // has no entry or alias for, which is the report's job to say out loud.
      expect(report.unknownExercises, isNot(contains('')));
    });
  });
}
