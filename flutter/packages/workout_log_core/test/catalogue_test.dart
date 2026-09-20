import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

void main() {
  late Catalogue catalogue;

  setUpAll(() => catalogue = loadCatalogue());

  test('catalogue is non-empty', () {
    expect(catalogue.exercises, isNotEmpty);
  });

  test('canonical name resolves with its pattern', () {
    expect(
      catalogue.resolve('присід фронтальний')?.pattern,
      MovementPattern.squat,
    );
  });

  test('alias resolves to the canonical name', () {
    expect(catalogue.resolve('фр. присід')?.name, 'присід фронтальний');
    expect(catalogue.resolve('гирі')?.name, 'махи гирею');
  });

  test('resolution is case- and whitespace-insensitive', () {
    expect(catalogue.resolve('  ПРИСІД ФРОНТАЛЬНИЙ ')?.name,
        'присід фронтальний');
  });

  test('an unknown name returns null (warn-not-block)', () {
    expect(catalogue.resolve('невідома вправа'), isNull);
  });

  test('жим лежачи carries chest and triceps as primaries', () {
    expect(catalogue.resolve('жим лежачи')?.primaryMuscles,
        ['chest', 'triceps']);
  });

  test('every exercise has primary muscles', () {
    final without =
        catalogue.exercises.where((e) => e.primaryMuscles.isEmpty).toList();
    expect(without.map((e) => e.name), isEmpty);
  });

  test('every catalogue muscle maps to a group', () {
    final ungrouped = {
      for (final e in catalogue.exercises)
        ...[...e.primaryMuscles, ...e.secondaryMuscles],
    }.where((m) => MuscleGroup.of(m) == null).toList()
      ..sort();
    expect(ungrouped, isEmpty);
  });

  test('dominant group is the highest-scoring region', () {
    expect(MuscleGroup.dominant({'chest': 3, 'biceps': 1}), MuscleGroup.chest);
    expect(MuscleGroup.dominant(const {}), isNull);
    expect(MuscleGroup.dominant({'not_a_muscle': 9}), isNull);
  });

  test('category decodes and drives isExplosive', () {
    final snatch = catalogue.resolve('ривок');
    expect(snatch?.category, ExerciseCategory.power);
    expect(snatch?.isExplosive, isTrue);
    expect(catalogue.resolve('присід фронтальний')?.isExplosive, isFalse);
  });

  test('explosive movements are exactly the power and speed categories', () {
    final explosive = catalogue.exercises.where((e) => e.isExplosive).toSet();
    final byCategory = catalogue.exercises
        .where((e) =>
            e.category == ExerciseCategory.power ||
            e.category == ExerciseCategory.speed)
        .toSet();
    expect(explosive, byCategory);
  });
}
