import '../coding.dart';

/// `pattern` groups variants of one movement so conjugate rotation reads as
/// progress rather than scatter (P8).
class Exercise with JsonEquality {
  Exercise({
    required this.name,
    List<String>? aliases,
    this.pattern,
    this.modality,
    this.category = ExerciseCategory.strength,
    List<String>? primaryMuscles,
    List<String>? secondaryMuscles,
    this.notes,
    Map<String, dynamic>? extras,
    Set<String>? presentKeys,
  })  : aliases = aliases ?? [],
        primaryMuscles = primaryMuscles ?? [],
        secondaryMuscles = secondaryMuscles ?? [],
        extras = extras ?? {},
        presentKeys = presentKeys ?? const {};

  factory Exercise.fromJson(Map<String, dynamic> json) => Exercise(
        name: json['name'] as String,
        aliases: asStringList(json['aliases']),
        pattern: MovementPattern.fromJson(json['pattern'] as String?),
        modality: Modality.fromJson(json['modality'] as String?),
        category: ExerciseCategory.fromJson(json['category'] as String?) ??
            ExerciseCategory.strength,
        primaryMuscles: asStringList(json['primary_muscles']),
        secondaryMuscles: asStringList(json['secondary_muscles']),
        notes: json['notes'] as String?,
        extras: unmodelledKeys(json, _modelled),
        presentKeys: json.keys.toSet(),
      );

  static const _modelled = {
    'name',
    'aliases',
    'pattern',
    'modality',
    'category',
    'primary_muscles',
    'secondary_muscles',
    'notes',
  };

  final String name; // canonical, Ukrainian
  final List<String> aliases;
  final MovementPattern? pattern;
  final Modality? modality;
  final ExerciseCategory category;
  final List<String> primaryMuscles;
  final List<String> secondaryMuscles;
  final String? notes;
  final Map<String, dynamic> extras;

  /// Which keys the source file actually carried. The catalogue writes
  /// `"secondary_muscles": []` for some entries, and omitting an empty list on
  /// save would quietly delete it.
  final Set<String> presentKeys;

  /// The P3 bar-speed rule applies to explosive movements — both power and speed work.
  bool get isExplosive =>
      category == ExerciseCategory.power || category == ExerciseCategory.speed;

  /// An empty list is written only when the source had the key, so re-saving
  /// the catalogue neither adds keys it never had nor drops ones it did.
  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{...extras, 'name': name};
    void putList(String key, List<String> value) {
      if (value.isNotEmpty || presentKeys.contains(key)) json[key] = value;
    }

    putList('aliases', aliases);
    put(json, 'pattern', pattern?.name);
    put(json, 'modality', modality?.name);
    json['category'] = category.name;
    json['primary_muscles'] = primaryMuscles;
    putList('secondary_muscles', secondaryMuscles);
    put(json, 'notes', notes);
    return json;
  }
}

/// Force–velocity / training emphasis of a movement.
/// - `power`: olympic lifts, force under heavy load (drives the P3 bar-speed rule)
/// - `speed`: light ballistic work (also drives P3)
/// - `strength`: max-force / loaded compound work
/// - `longevity`: durability work (grip, core, delts, calves, mobility, conditioning)
enum ExerciseCategory {
  strength,
  power,
  speed,
  longevity;

  static ExerciseCategory? fromJson(String? value) =>
      value == null ? null : ExerciseCategory.values.byName(value);
}

/// Named `MovementPattern` because `Pattern` is taken by `dart:core`.
enum MovementPattern {
  squat,
  hinge,
  press,
  pull,
  olympic,
  carry,
  core,
  grip;

  static MovementPattern? fromJson(String? value) =>
      value == null ? null : MovementPattern.values.byName(value);
}

enum Modality {
  barbell,
  dumbbell,
  kettlebell,
  bodyweight,
  machine;

  static Modality? fromJson(String? value) =>
      value == null ? null : Modality.values.byName(value);
}

class Catalogue with JsonEquality {
  Catalogue({required this.exercises, this.comment});

  factory Catalogue.fromJson(Map<String, dynamic> json) => Catalogue(
        comment: json[r'$comment'] as String?,
        exercises: [
          for (final e in json['exercises'] as List)
            Exercise.fromJson(e as Map<String, dynamic>),
        ],
      );

  final List<Exercise> exercises;

  /// The file's own top-level `$comment`, kept so writing the catalogue back
  /// does not strip its documentation.
  final String? comment;

  /// The catalogue with `exercise` appended, or replacing the entry of the same
  /// canonical name.
  Catalogue withExercise(Exercise exercise) {
    final index =
        exercises.indexWhere((e) => e.name.toLowerCase() == exercise.name.toLowerCase());
    final next = [...exercises];
    if (index >= 0) {
      next[index] = exercise;
    } else {
      next.add(exercise);
    }
    return Catalogue(exercises: next, comment: comment);
  }

  /// Every muscle token the catalogue uses, sorted — the vocabulary a new
  /// exercise should pick from.
  List<String> get knownMuscles {
    final all = <String>{
      for (final e in exercises) ...[...e.primaryMuscles, ...e.secondaryMuscles],
    }.toList()
      ..sort();
    return all;
  }

  /// Matches canonical names and aliases (case-insensitive); null when unknown
  /// (caller warns, never blocks).
  Exercise? resolve(String typed) {
    final key = typed.trim().toLowerCase();
    for (final ex in exercises) {
      if (ex.name.toLowerCase() == key ||
          ex.aliases.any((a) => a.toLowerCase() == key)) {
        return ex;
      }
    }
    return null;
  }

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    put(json, r'$comment', comment);
    json['exercises'] = [for (final e in exercises) e.toJson()];
    return json;
  }
}
