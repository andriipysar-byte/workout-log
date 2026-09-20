import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

void main() {
  late Cycle cycle;

  setUpAll(() => cycle = loadCycles().byId('hybrid-8')!);

  test('training dates are the first N on/after start on a training weekday', () {
    final dates = CycleGenerator.trainingDates(
      DateTime(2026, 7, 21),
      const ['Tue', 'Thu', 'Sun'],
      4,
    );
    expect(dates.map(CycleGenerator.isoDate),
        ['2026-07-21', '2026-07-23', '2026-07-26', '2026-07-28']);
  });

  test('a start date that is itself a training day counts as the first', () {
    final dates =
        CycleGenerator.trainingDates(DateTime(2026, 7, 20), const ['Tue'], 1);
    expect(CycleGenerator.isoDate(dates.single), '2026-07-21');
  });

  test('unrecognised training days fail instead of looping forever', () {
    expect(
      () => CycleGenerator.trainingDates(DateTime(2026, 7, 21), const ['Xyz'], 1),
      throwsA(isA<CycleGeneratorException>()),
    );
  });

  test('a template weekday that disagrees with the calendar is a hard error', () {
    expect(
      () => CycleGenerator.sessionFromTemplate(
        CycleSession(cycleDay: 'A1', weekday: 'Mon', blocks: const []),
        DateTime(2026, 7, 21), // a Tuesday
      ),
      throwsA(isA<CycleGeneratorException>()),
    );
  });

  test('a single session may opt out of the weekday check', () {
    final session = CycleGenerator.sessionFromTemplate(
      CycleSession(cycleDay: 'A1', weekday: 'Mon', blocks: const []),
      DateTime(2026, 7, 21),
      enforceWeekday: false,
    );
    expect(session.date, '2026-07-21');
  });

  test('hybrid-8 expands onto the dates already in data/', () {
    final sessions = CycleGenerator.generate(cycle);
    expect(sessions.map(SessionStore.idFor), [
      '2026-07-21_A1.json',
      '2026-07-23_A2.json',
      '2026-07-26_B1.json',
      '2026-07-28_B2.json',
      '2026-07-30_C1.json',
      '2026-08-02_C2.json',
      '2026-08-04_D1.json',
      '2026-08-06_D2.json',
    ]);
  });

  test('generated stubs are schema-shaped and re-encode unchanged', () {
    for (final session in CycleGenerator.generate(cycle)) {
      expect(session.kind, Kind.training);
      expect(session.date, matches(RegExp(r'^\d{4}-\d{2}-\d{2}$')));
      expect(session.blocks, isNotEmpty);
      final encoded = SessionCoding.encode(session);
      expect(SessionCoding.decode(encoded), session);
    }
  });

  test('planning-only fields never reach the output', () {
    for (final session in CycleGenerator.generate(cycle)) {
      final encoded = SessionCoding.encode(session);
      expect(encoded, isNot(contains('sets_reps')));
      expect(encoded, isNot(contains('"role"')));
      expect(encoded, isNot(contains('weekday')));
      expect(encoded, isNot(contains('"title"')));
    }
  });

  test('sets_reps becomes one rep-only set each', () {
    final block = CycleGenerator.blockFromTemplate(
      BlockTemplate(
        type: 'strength',
        exercise: 'гіперекстензія',
        setsReps: const [12, 12],
      ),
    ) as StrengthBlock;
    expect(block.sets.map((s) => s.reps), [12, 12]);
    expect(block.sets.map((s) => s.weightKg), everyElement(isNull));
  });

  test('a strength template with no sets_reps still emits an empty sets list', () {
    final block = CycleGenerator.blockFromTemplate(
      BlockTemplate(type: 'strength', exercise: 'жим стоячи'),
    );
    expect(block.toJson()['sets'], isEmpty);
  });

  test('metcon templates carry format, scheme and exercises through', () {
    final block = CycleGenerator.blockFromTemplate(
      BlockTemplate(
        type: 'metcon',
        format: MetconFormat.forTime,
        scheme: const [21, 15, 9],
        exercises: [MetconExercise(name: 'трастери')],
      ),
    ) as MetconBlock;
    expect(block.format, MetconFormat.forTime);
    expect(block.scheme, [21, 15, 9]);
    expect(block.toJson()['format'], 'for_time');
  });

  test('an unknown template block type is rejected', () {
    expect(
      () => CycleGenerator.blockFromTemplate(BlockTemplate(type: 'yoga')),
      throwsA(isA<CycleGeneratorException>()),
    );
  });

  test('generating twice is deterministic', () {
    final first = CycleGenerator.generate(cycle).map(SessionCoding.encode);
    final second = CycleGenerator.generate(cycle).map(SessionCoding.encode);
    expect(first, second);
  });
}
