import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:workout_log_core/workout_log_core.dart';
import 'package:workout_log/app_model.dart';
import 'package:workout_log/platform/session_folder.dart';
import 'package:workout_log/ui/plan_view.dart';

import 'helpers.dart';

/// A writable stand-in for the repo's exercises.json / cycles.json.
class _MemoryReferenceStore implements ReferenceStore {
  _MemoryReferenceStore(this.files);

  final Map<String, String> files;

  @override
  bool get canWrite => true;

  @override
  String get label => 'test references';

  @override
  Future<String?> read(String name) async => files[name];

  @override
  Future<void> write(String name, String contents) async =>
      files[name] = contents;
}

void main() {
  late Map<String, String> reference;

  setUp(() {
    reference = {
      'cycles.json': File('../cycles.json').readAsStringSync(),
      'exercises.json': File('../exercises.json').readAsStringSync(),
    };
  });

  Future<AppModel> planModel(WidgetTester tester) async {
    final model = AppModel();
    await tester.runAsync(
      () => model.start(
        folder: SessionFolder(
          storage: MemoryStorage(label: 'test archive'),
          references: _MemoryReferenceStore(reference),
          canChooseFolder: false,
          isBrowserCopy: false,
        ),
      ),
    );
    return model;
  }

  Widget host(AppModel model) => hostedIn(model, const PlanView());

  testWidgets('the plan loads the real cycles.json', (tester) async {
    final model = await planModel(tester);
    await tester.pumpWidget(host(model));
    await tester.pumpAndSettle();

    expect(model.cycles, hasLength(1));
    expect(model.cycles.single.sessions, hasLength(8));
    expect(model.canEditPlan, isTrue);
    expect(find.text('8 workouts · starts 2026-07-21 · Tue, Thu, Sun'),
        findsOneWidget);
  });

  testWidgets('each planned workout shows a muscle map', (tester) async {
    final model = await planModel(tester);
    final cycle = model.cycles.single;
    for (final workout in cycle.sessions) {
      expect(model.planMapSVG(workout), isNotNull, reason: workout.cycleDay);
    }
    expect(model.planDominantGroup(cycle.sessions.first), isNotNull);
  });

  testWidgets('adding a workout continues the A1/A2 progression',
      (tester) async {
    final model = await planModel(tester);
    final cycle = model.cycles.single;
    expect(cycle.sessions.last.cycleDay, 'D2');

    final added = model.addWorkout(cycle);
    expect(added.cycleDay, 'E1');
    expect(model.addWorkout(cycle).cycleDay, 'E2');
    expect(model.addWorkout(cycle).cycleDay, 'F1');
  });

  testWidgets('a new CrossFit day is prefilled, a hard day differently',
      (tester) async {
    final model = await planModel(tester);
    final cycle = model.cycles.single;

    final conditioning = model.addWorkout(cycle); // E1
    expect(conditioning.blocks.map((b) => b.type), contains('metcon'));
    expect(conditioning.blocks.map((b) => b.role), contains('explosive'));

    final heavy = model.addWorkout(cycle); // E2
    expect(heavy.blocks.map((b) => b.role), contains('main'));
    expect(heavy.blocks.map((b) => b.type), isNot(contains('metcon')));
  });

  testWidgets('a new workout lands on the next real training date',
      (tester) async {
    final model = await planModel(tester);
    final cycle = model.cycles.single;
    model.addWorkout(cycle);

    final date = model.plannedDate(cycle, cycle.sessions.length - 1)!;
    expect(CycleGenerator.isoDate(date), '2026-08-09');
    expect(cycle.sessions.last.weekday, 'Sun');
  });

  testWidgets('removing a workout re-derives the weekdays that shifted',
      (tester) async {
    final model = await planModel(tester);
    final cycle = model.cycles.single;
    model.removeWorkout(cycle, cycle.sessions.first);

    expect(cycle.sessions, hasLength(7));
    // Everything moved one slot earlier, so the stored weekday must follow or
    // the generator would reject the whole cycle.
    for (var i = 0; i < cycle.sessions.length; i++) {
      final date = model.plannedDate(cycle, i)!;
      expect(
        cycle.sessions[i].weekday,
        CycleGenerator.weekdayAbbreviations[date.weekday - 1],
      );
    }
    expect(() => CycleGenerator.generate(cycle), returnsNormally);
  });

  testWidgets('moving a workout reorders it and keeps dates consistent',
      (tester) async {
    final model = await planModel(tester);
    final cycle = model.cycles.single;
    final second = cycle.sessions[1];
    model.moveWorkout(cycle, 1, 0);

    expect(cycle.sessions.first, same(second));
    expect(() => CycleGenerator.generate(cycle), returnsNormally);
  });

  testWidgets('renaming a workout updates its code and its day type',
      (tester) async {
    final model = await planModel(tester);
    final cycle = model.cycles.single;
    final workout = cycle.sessions.first;

    model.retitleWorkout(cycle, workout, const CycleDay('C', DayKind.heavy));
    expect(workout.cycleDay, 'C2');
    expect(workout.type, 'heavy');
  });

  testWidgets('cloning copies the workouts without aliasing the original',
      (tester) async {
    final model = await planModel(tester);
    final source = model.cycles.single;
    final clone = model.cloneCycle(source, id: 'hybrid-8-b', name: 'Copy');

    expect(model.cycles, hasLength(2));
    expect(clone.sessions, hasLength(source.sessions.length));

    clone.sessions.first.cycleDay = 'Z1';
    expect(source.sessions.first.cycleDay, isNot('Z1'),
        reason: 'the clone must be a deep copy');
  });

  testWidgets('a duplicate cycle id is reported', (tester) async {
    final model = await planModel(tester);
    expect(model.cycleIdTaken('hybrid-8'), isTrue);
    expect(model.cycleIdTaken('  HYBRID-8 '), isTrue);
    expect(model.cycleIdTaken('something-else'), isFalse);
  });

  testWidgets('saving cycles.json preserves every hand-written key',
      (tester) async {
    final model = await planModel(tester);
    final before = jsonDecode(reference['cycles.json']!);

    await model.saveCycles();
    final after = jsonDecode(reference['cycles.json']!);

    expect(canonicalJson(after), canonicalJson(before));
    expect(model.status, 'Saved cycles.json');
  });

  testWidgets('a saved cycle still generates the same session stubs',
      (tester) async {
    final model = await planModel(tester);
    await model.saveCycles();

    final reloaded = CycleCatalogue.fromJson(
      jsonMap(reference['cycles.json']!),
    ).byId('hybrid-8')!;
    expect(
      CycleGenerator.generate(reloaded).map(SessionStore.idFor),
      CycleGenerator.generate(model.cycles.single).map(SessionStore.idFor),
    );
  });

  testWidgets('a new exercise is written into exercises.json', (tester) async {
    final model = await planModel(tester);
    await model.addExercise(
      Exercise(
        name: 'тяга сумо',
        aliases: const ['сумо'],
        pattern: MovementPattern.hinge,
        category: ExerciseCategory.strength,
        primaryMuscles: const ['glutes', 'hamstrings'],
        secondaryMuscles: const ['spinal_erectors'],
      ),
    );

    expect(model.catalogue!.resolve('сумо')?.name, 'тяга сумо');
    final written = Catalogue.fromJson(jsonMap(reference['exercises.json']!));
    expect(written.resolve('тяга сумо'), isNotNull);
    expect(written.comment, isNotNull, reason: 'the file comment survives');
    expect(written.exercises.length,
        Catalogue.fromJson(jsonMap(File('../exercises.json').readAsStringSync()))
                .exercises
                .length +
            1);
  });

  testWidgets('a newly added exercise reaches the plan muscle map',
      (tester) async {
    final model = await planModel(tester);
    await model.addExercise(
      Exercise(name: 'тяга сумо', primaryMuscles: const ['glutes']),
    );

    final workout = CycleSession(
      cycleDay: 'A1',
      blocks: [BlockTemplate(type: 'strength', exercise: 'тяга сумо')],
    );
    expect(model.planMapSVG(workout), isNotNull);
    expect(model.planDominantGroup(workout), MuscleGroup.legs);
  });

  testWidgets('the Plan tab is reachable from the shell', (tester) async {
    final model = await planModel(tester);
    await tester.pumpWidget(ChangeNotifierProviderHostForPlan(model: model));
    await tester.pumpAndSettle();

    await tester.tap(find.byTooltip('Open navigation menu'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Plan'));
    await tester.pumpAndSettle();

    expect(find.byType(PlanView), findsOneWidget);
    expect(find.text('Save cycles.json'), findsOneWidget);
  });

  testWidgets('a read-only platform disables every editing control',
      (tester) async {
    final model = AppModel();
    await tester.runAsync(
      () => model.start(
        folder: SessionFolder(
          storage: MemoryStorage(label: 'browser copy'),
          canChooseFolder: false,
          isBrowserCopy: true,
        ),
      ),
    );
    await tester.pumpWidget(host(model));
    await tester.pumpAndSettle();

    expect(model.canEditPlan, isFalse);
    final save = tester.widget<FilledButton>(
      find.ancestor(
        of: find.text('Save cycles.json'),
        matching: find.byType(FilledButton),
      ),
    );
    expect(save.onPressed, isNull);
    expect(find.byIcon(Icons.lock_outline), findsOneWidget);
  });
}

class ChangeNotifierProviderHostForPlan extends StatelessWidget {
  const ChangeNotifierProviderHostForPlan({required this.model, super.key});

  final AppModel model;

  @override
  Widget build(BuildContext context) => appUnder(model);
}
