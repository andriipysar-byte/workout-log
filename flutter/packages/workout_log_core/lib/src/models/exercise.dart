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
  })  : aliases = aliases ?? [],
        primaryMuscles = primaryMuscles ?? [],
        secondaryMuscles = secondaryMuscles ?? [];

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
      );

  final String name; // canonical, Ukrainian
  final List<String> aliases;
  final MovementPattern? pattern;
  final Modality? modality;
  final ExerciseCategory category;
  final List<String> primaryMuscles;
  final List<String> secondaryMuscles;
  final String? notes;

  /// The P3 bar-speed rule applies to explosive movements — both power and speed work.
  bool get isExplosive =>
      category == ExerciseCategory.power || category == ExerciseCategory.speed;

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'name': name, 'aliases': aliases};
    put(json, 'pattern', pattern?.name);
    put(json, 'modality', modality?.name);
    json['category'] = category.name;
    json['primary_muscles'] = primaryMuscles;
    json['secondary_muscles'] = secondaryMuscles;
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
  Catalogue({required this.exercises});

  /// The file carries a top-level `$comment`; only `exercises` is decoded.
  factory Catalogue.fromJson(Map<String, dynamic> json) => Catalogue(
        exercises: [
          for (final e in json['exercises'] as List)
            Exercise.fromJson(e as Map<String, dynamic>),
        ],
      );

  final List<Exercise> exercises;

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
  Map<String, dynamic> toJson() => {
        'exercises': [for (final e in exercises) e.toJson()],
      };
}
