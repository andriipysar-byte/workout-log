import 'dart:convert';
import 'dart:io';

import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

/// The planner writes `cycles.json` and `exercises.json` back out, so anything
/// the models drop on read would be deleted from the user's file on the first
/// save. These assert value-level losslessness against the real files.
void main() {
  Object? valueOf(String path) =>
      jsonDecode(File('${repoRoot.path}/$path').readAsStringSync());

  group('cycles.json', () {
    test('round-trips without losing a single key', () {
      final original = valueOf('cycles.json');
      final reencoded = CycleCatalogue.fromJson(
        original! as Map<String, dynamic>,
      ).toJson();
      expect(canonicalJson(reencoded), canonicalJson(original));
    });

    test('keeps the planning-only role on every block', () {
      final cycle = loadCycles().byId('hybrid-8')!;
      final roles = [
        for (final session in cycle.sessions)
          for (final block in session.blocks) block.role,
      ];
      expect(roles, isNot(contains(null)));
      expect(roles, contains('explosive'));
      expect(roles, contains('grip'));
    });

    test('keeps cycle-level keys the model has no field for', () {
      final cycle = loadCycles().byId('hybrid-8')!;
      expect(cycle.extras.keys, contains('skipped'));
      expect(cycle.toJson()['skipped'], isNotNull);
    });

    test('the top-level comment survives a save', () {
      final catalogue = loadCycles();
      expect(catalogue.comment, contains('Reusable cycle definitions'));
      expect(catalogue.toJson()[r'$comment'], catalogue.comment);
    });
  });

  group('exercises.json', () {
    test('round-trips without losing a single key', () {
      final original = valueOf('exercises.json');
      final reencoded = Catalogue.fromJson(
        original! as Map<String, dynamic>,
      ).toJson();
      expect(canonicalJson(reencoded), canonicalJson(original));
    });

    test('the top-level comment survives a save', () {
      final catalogue = loadCatalogue();
      expect(catalogue.comment, contains('Exercise catalogue'));
      expect(catalogue.toJson()[r'$comment'], catalogue.comment);
    });

    test('adding an exercise appends without disturbing the rest', () {
      final before = loadCatalogue();
      final after = before.withExercise(
        Exercise(
          name: 'тест вправа',
          category: ExerciseCategory.strength,
          primaryMuscles: ['chest'],
        ),
      );
      expect(after.exercises, hasLength(before.exercises.length + 1));
      expect(after.resolve('тест вправа')?.primaryMuscles, ['chest']);
      expect(after.comment, before.comment);
      expect(
        after.exercises.take(before.exercises.length),
        before.exercises,
      );
    });

    test('adding an existing name replaces it rather than duplicating', () {
      final before = loadCatalogue();
      final after = before.withExercise(
        Exercise(name: 'жим лежачи', primaryMuscles: ['chest']),
      );
      expect(after.exercises, hasLength(before.exercises.length));
      expect(after.resolve('жим лежачи')?.primaryMuscles, ['chest']);
    });

    test('knownMuscles is the vocabulary a new exercise picks from', () {
      final muscles = loadCatalogue().knownMuscles;
      expect(muscles, contains('quads'));
      expect(muscles, contains('spinal_erectors'));
      expect(muscles, equals(muscles.toList()..sort()));
      expect(muscles.every((m) => MuscleGroup.of(m) != null), isTrue);
    });
  });
}
