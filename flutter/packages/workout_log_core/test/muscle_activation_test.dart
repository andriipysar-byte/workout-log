import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

void main() {
  late Catalogue catalogue;
  const activation = MuscleActivation();

  setUpAll(() => catalogue = loadCatalogue());

  test('per-exercise map: primaries 1.0, secondaries 0.5', () {
    final frontSquat = catalogue.resolve('присід фронтальний')!;
    final map = activation.forExercise(frontSquat);
    expect(map['quads'], 1.0);
    expect(map['glutes'], 1.0);
    expect(map['spinal_erectors'], 0.5);
  });

  group('over a real session', () {
    late Session session;

    setUpAll(() {
      final file = sessionFiles
          .firstWhere((f) => f.path.endsWith('2026-06-23_C2.json'));
      session = SessionCoding.decode(file.readAsStringSync());
    });

    for (final mode in WeightingMode.values) {
      test('${mode.wire} normalizes to a peak of 1.0', () {
        final map =
            activation.forSession(session, catalogue: catalogue, mode: mode);
        expect(map.values.reduce((a, b) => a > b ? a : b), closeTo(1.0, 1e-9));
      });
    }

    test('the modes surface different hottest muscles', () {
      String peak(WeightingMode mode) {
        final map =
            activation.forSession(session, catalogue: catalogue, mode: mode);
        return map.entries.reduce((a, b) => a.value >= b.value ? a : b).key;
      }

      expect(peak(WeightingMode.setCount), 'forearms',
          reason: 'grip-set heavy day');
      expect(peak(WeightingMode.repVolume), anyOf('quads', 'glutes'));
      expect(peak(WeightingMode.tonnage), anyOf('quads', 'glutes'));
      expect(peak(WeightingMode.setCount),
          isNot(peak(WeightingMode.tonnage)));
    });
  });

  test('an empty session produces an empty map', () {
    final map = activation.forSession(
      Session(date: '2026-01-01', cycleDay: 'A1'),
      catalogue: catalogue,
      mode: WeightingMode.setCount,
    );
    expect(map, isEmpty);
  });

  test('unknown exercise names contribute nothing', () {
    final map = activation.forSession(
      Session(
        date: '2026-01-01',
        cycleDay: 'A1',
        blocks: [
          StrengthBlock(exercise: 'вправа якої немає', sets: [WorkSet(reps: 5)]),
        ],
      ),
      catalogue: catalogue,
      mode: WeightingMode.setCount,
    );
    expect(map, isEmpty);
  });

  test('cycle totals sum raw volume before normalizing once', () {
    final heavy = Session(
      date: '2026-01-01',
      cycleDay: 'A1',
      blocks: [
        StrengthBlock(
          exercise: 'присід фронтальний',
          sets: [for (var i = 0; i < 10; i++) WorkSet(reps: 6, weightKg: 100)],
        ),
      ],
    );
    final light = Session(
      date: '2026-01-02',
      cycleDay: 'A2',
      blocks: [
        StrengthBlock(
          exercise: 'жим лежачи',
          sets: [WorkSet(reps: 6, weightKg: 40)],
        ),
      ],
    );
    final map = activation.forSessions(
      [heavy, light],
      catalogue: catalogue,
      mode: WeightingMode.tonnage,
    );
    expect(map['quads'], 1.0);
    expect(map['chest']!, lessThan(0.1));
  });

  test('WeightingMode round-trips through its wire name', () {
    for (final mode in WeightingMode.values) {
      expect(WeightingMode.fromWire(mode.wire), mode);
    }
    expect(WeightingMode.fromWire('nonsense'), isNull);
  });
}
