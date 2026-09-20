import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

Session _session(Map<String, dynamic> overrides) => Session.fromJson({
      'date': '2026-08-10',
      'cycle_day': 'F1',
      'blocks': <Object>[],
      ...overrides,
    });

List<String> _paths(List<ValidationIssue> issues, IssueSeverity severity) => [
      for (final issue in issues)
        if (issue.severity == severity) issue.path,
    ];

void main() {
  group('SessionValidator', () {
    test('a session file from the archive is clean of errors', () {
      final catalogue = loadCatalogue();
      for (final file in sessionFiles) {
        final session = SessionCoding.decode(file.readAsStringSync());
        final issues = SessionValidator.validate(session, catalogue: catalogue);
        expect(
          SessionValidator.hasErrors(issues),
          isFalse,
          reason: '${file.path}: $issues',
        );
      }
    });

    test('a malformed header is an error', () {
      final issues = SessionValidator.validate(_session({
        'date': '10.08.2026',
        'start_time': '8h02',
      }));

      expect(_paths(issues, IssueSeverity.error), ['date', 'start_time']);
    });

    test('an off-convention cycle day is a warning, not a refusal', () {
      final issues = SessionValidator.validate(_session({'cycle_day': 'Z9'}));

      expect(SessionValidator.hasErrors(issues), isFalse);
      expect(_paths(issues, IssueSeverity.warning), contains('cycle_day'));
    });

    test('an unfinished plan warns but never errors', () {
      final issues = SessionValidator.validate(_session({
        'blocks': [
          {'type': 'strength', 'exercise': 'присід фронтальний', 'sets': <Object>[]},
        ],
      }));

      expect(SessionValidator.hasErrors(issues), isFalse);
      expect(_paths(issues, IssueSeverity.warning), ['blocks[0].sets']);
    });

    test('a strength block with no exercise cannot be written', () {
      final issues = SessionValidator.validate(_session({
        'blocks': [
          {'type': 'strength', 'exercise': '  ', 'sets': <Object>[]},
        ],
      }));

      expect(_paths(issues, IssueSeverity.error), ['blocks[0].exercise']);
    });

    test('a cluster whose total contradicts its chain is a warning', () {
      final issues = SessionValidator.validate(_session({
        'blocks': [
          {
            'type': 'strength',
            'exercise': 'підтягування',
            'sets': [
              {'cluster': [5, 5, 4, 3, 3], 'total_reps': 19},
            ],
          },
        ],
      }));

      expect(
        _paths(issues, IssueSeverity.warning),
        contains('blocks[0].sets[0].total_reps'),
      );
    });

    test('splits that go backwards are caught — they are cumulative', () {
      final issues = SessionValidator.validate(_session({
        'blocks': [
          {
            'type': 'metcon',
            'exercises': [
              {'name': 'трастери'},
            ],
            'rounds': [
              {'round': 1, 'split_cumulative_sec': 463},
              {'round': 2, 'split_cumulative_sec': 292},
            ],
          },
        ],
      }));

      expect(
        _paths(issues, IssueSeverity.warning),
        contains('blocks[0].rounds[1].split_cumulative_sec'),
      );
    });

    test('blocks that overlap in time are caught', () {
      final issues = SessionValidator.validate(_session({
        'start_time': '08:02',
        'blocks': [
          {'type': 'cardio', 'machine': 'велотренажер', 'end_time': '08:30'},
          {
            'type': 'strength',
            'exercise': 'присід фронтальний',
            'sets': [
              {'reps': 6},
            ],
            'start_time': '08:20',
            'end_time': '08:50',
          },
        ],
      }));

      expect(_paths(issues, IssueSeverity.warning), contains('blocks[1]'));
    });

    test('an exercise outside the catalogue is a warning with the name in it', () {
      final issues = SessionValidator.validate(
        _session({
          'blocks': [
            {
              'type': 'strength',
              'exercise': 'вправа якої немає',
              'sets': [
                {'reps': 6},
              ],
            },
          ],
        }),
        catalogue: loadCatalogue(),
      );

      expect(issues.single.message, contains('вправа якої немає'));
      expect(issues.single.severity, IssueSeverity.warning);
    });
  });
}
