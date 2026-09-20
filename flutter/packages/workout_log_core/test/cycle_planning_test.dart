import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

void main() {
  group('CycleDay', () {
    test('parses the letter-and-number convention', () {
      final day = CycleDay.tryParse('B2')!;
      expect(day.letter, 'B');
      expect(day.kind, DayKind.heavy);
      expect(day.code, 'B2');
    });

    test('1 is conditioning and 2 is hard work', () {
      expect(CycleDay.tryParse('A1')!.kind, DayKind.conditioning);
      expect(CycleDay.tryParse('A2')!.kind, DayKind.heavy);
      expect(DayKind.conditioning.label, 'CrossFit');
      expect(DayKind.heavy.label, 'Hard work');
    });

    test('a lowercase code is accepted and normalised', () {
      expect(CycleDay.tryParse('c1')?.code, 'C1');
      expect(CycleDay.tryParse(' a2 ')?.code, 'A2');
    });

    test('a code outside the convention returns null rather than throwing', () {
      for (final code in ['A3', 'AA1', '1A', 'A', '', 'деньА1']) {
        expect(CycleDay.tryParse(code), isNull, reason: code);
      }
    });

    test('next walks A1 → A2 → B1 → B2', () {
      var day = CycleDay.tryParse('A1')!;
      final walk = <String>[day.code];
      for (var i = 0; i < 3; i++) {
        day = day.next;
        walk.add(day.code);
      }
      expect(walk, ['A1', 'A2', 'B1', 'B2']);
    });
  });

  group('next day for a cycle', () {
    test('an empty cycle starts at A1', () {
      expect(CycleTemplates.nextDay(const []).code, 'A1');
    });

    test('hybrid-8 ends at D2, so the next workout is E1', () {
      final cycle = loadCycles().byId('hybrid-8')!;
      final next = CycleTemplates.nextDay(
        cycle.sessions.map((s) => s.cycleDay),
      );
      expect(next.code, 'E1');
    });

    test('codes outside the convention are ignored when picking the next', () {
      expect(CycleTemplates.nextDay(['деload', 'A1']).code, 'A2');
      expect(CycleTemplates.nextDay(['деload']).code, 'A1');
    });
  });

  group('prefilled blocks', () {
    test('a CrossFit day gets an explosive lift and a metcon', () {
      final blocks = CycleTemplates.blocksFor(DayKind.conditioning);
      expect(blocks.map((b) => b.role), contains('explosive'));
      expect(blocks.map((b) => b.type), contains('metcon'));
      expect(blocks.first.type, 'cardio');
      expect(blocks.last.type, 'cooldown');
    });

    test('a hard-work day gets a main lift and no metcon', () {
      final blocks = CycleTemplates.blocksFor(DayKind.heavy);
      expect(blocks.map((b) => b.role), contains('main'));
      expect(blocks.map((b) => b.type), isNot(contains('metcon')));
      expect(blocks.first.durationMin, 15, reason: 'longer warm-up');
    });

    test('a new session carries its code and kind', () {
      final session = CycleTemplates.session(CycleDay.tryParse('C1')!);
      expect(session.cycleDay, 'C1');
      expect(session.type, 'metcon');
      expect(session.blocks, isNotEmpty);
    });
  });

  group('plan preview', () {
    test('a planned exercise counts even with no sets planned', () {
      final session = CycleSession(
        cycleDay: 'A2',
        blocks: [
          BlockTemplate(type: 'strength', role: 'main', exercise: 'жим лежачи'),
        ],
      );
      final scores = const MuscleActivation().forSession(
        CycleGenerator.preview(session),
        catalogue: loadCatalogue(),
        mode: WeightingMode.setCount,
      );
      expect(scores['chest'], 1.0);
    });

    test('skips blocks with no exercise chosen yet', () {
      final session = CycleTemplates.session(CycleDay.tryParse('A2')!);
      final preview = CycleGenerator.preview(session);
      // Only the pre-named hyperextension survives; the blank slots do not.
      final named = preview.blocks.whereType<StrengthBlock>().map((b) => b.exercise);
      expect(named, ['гіперекстензія']);
      expect(preview.blocks.whereType<CardioBlock>(), hasLength(1));
    });

    test('generating an incomplete plan fails loudly instead', () {
      final session = CycleTemplates.session(CycleDay.tryParse('A2')!);
      expect(
        () => CycleGenerator.sessionFromTemplate(session, DateTime(2026, 7, 21)),
        throwsA(isA<CycleGeneratorException>()),
      );
    });

    test('a real planned day produces a muscle map', () {
      final cycle = loadCycles().byId('hybrid-8')!;
      final c2 = cycle.sessions.firstWhere((s) => s.cycleDay == 'C2');
      final scores = const MuscleActivation().forSession(
        CycleGenerator.preview(c2),
        catalogue: loadCatalogue(),
        mode: WeightingMode.setCount,
      );
      expect(scores, isNotEmpty);
      expect(scores['quads'], isNotNull,
          reason: 'the planned main lift must reach the map');
      expect(scores['calves'], isNotNull);
      expect(MuscleGroup.dominant(scores), MuscleGroup.legs);
    });

    test('an empty plan produces an empty map rather than failing', () {
      final scores = const MuscleActivation().forSession(
        CycleGenerator.preview(CycleSession(cycleDay: 'A1', blocks: const [])),
        catalogue: loadCatalogue(),
        mode: WeightingMode.setCount,
      );
      expect(scores, isEmpty);
    });
  });
}
