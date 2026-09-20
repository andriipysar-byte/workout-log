part of 'block.dart';

class MetconBlock extends Block {
  MetconBlock({
    this.format,
    this.scheme,
    List<MetconExercise>? exercises,
    this.rounds,
    this.startTime,
    this.endTime,
    this.notes,
  }) : exercises = exercises ?? [];

  factory MetconBlock.fromJson(Map<String, dynamic> json) => MetconBlock(
        format: MetconFormat.fromJson(json['format'] as String?),
        scheme: asIntList(json['scheme']),
        exercises: [
          for (final e in json['exercises'] as List? ?? const [])
            MetconExercise.fromJson(e as Map<String, dynamic>),
        ],
        rounds: json['rounds'] == null
            ? null
            : [
                for (final r in json['rounds'] as List)
                  MetconRound.fromJson(r as Map<String, dynamic>),
              ],
        startTime: json['start_time'] as String?,
        endTime: json['end_time'] as String?,
        notes: json['notes'] as String?,
      );

  MetconFormat? format;
  List<int>? scheme;
  List<MetconExercise> exercises;
  List<MetconRound>? rounds;
  String? startTime;
  String? endTime;
  String? notes;

  @override
  String get type => 'metcon';

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'type': type};
    put(json, 'format', format?.wire);
    put(json, 'scheme', scheme);
    json['exercises'] = [for (final e in exercises) e.toJson()];
    put(json, 'rounds', rounds?.map((r) => r.toJson()).toList());
    put(json, 'start_time', startTime);
    put(json, 'end_time', endTime);
    put(json, 'notes', notes);
    return json;
  }
}

enum MetconFormat {
  forTime('for_time'),
  amrap('amrap'),
  emom('emom'),
  intervals('intervals'),
  ladder('ladder'),
  chipper('chipper');

  const MetconFormat(this.wire);

  final String wire;

  static MetconFormat? fromJson(String? value) => value == null
      ? null
      : MetconFormat.values.firstWhere((f) => f.wire == value);
}

class MetconExercise with JsonEquality {
  MetconExercise({required this.name, this.weightKg, this.repsOverride});

  factory MetconExercise.fromJson(Map<String, dynamic> json) => MetconExercise(
        name: json['name'] as String,
        weightKg: asDouble(json['weight_kg']),
        repsOverride: asIntList(json['reps_override']),
      );

  String name;
  double? weightKg;
  List<int>? repsOverride;

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'name': name};
    put(json, 'weight_kg', weightKg);
    put(json, 'reps_override', repsOverride);
    return json;
  }
}

/// Splits are stored cumulative (as on paper); per-round splits are derived at read time, never stored.
class MetconRound with JsonEquality {
  MetconRound({
    required this.round,
    this.reps,
    this.splitCumulativeSec,
    this.heartRate,
  });

  factory MetconRound.fromJson(Map<String, dynamic> json) => MetconRound(
        round: asInt(json['round'])!,
        reps: asInt(json['reps']),
        splitCumulativeSec: asDouble(json['split_cumulative_sec']),
        heartRate: asInt(json['heart_rate']),
      );

  int round;
  int? reps;
  double? splitCumulativeSec;
  int? heartRate;

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'round': round};
    put(json, 'reps', reps);
    put(json, 'split_cumulative_sec', splitCumulativeSec);
    put(json, 'heart_rate', heartRate);
    return json;
  }
}
