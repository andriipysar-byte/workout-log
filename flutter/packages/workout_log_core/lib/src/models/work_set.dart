import '../coding.dart';

/// All fields optional so a weightless / bodyweight / timed set round-trips
/// without inventing values. Named `WorkSet` to avoid clashing with Dart's `Set`.
class WorkSet with JsonEquality {
  WorkSet({
    this.weightKg,
    this.reps,
    this.durationSec,
    this.cluster,
    this.totalReps,
    this.rir,
    this.repBand,
    this.barSpeed,
    this.isBackoff,
    this.plannedReps,
    this.notes,
  });

  factory WorkSet.fromJson(Map<String, dynamic> json) => WorkSet(
        weightKg: asDouble(json['weight_kg']),
        reps: asInt(json['reps']),
        durationSec: asDouble(json['duration_sec']),
        cluster: asIntList(json['cluster']),
        totalReps: asInt(json['total_reps']),
        rir: asDouble(json['rir']),
        repBand: RepBand.fromJson(json['rep_band'] as String?),
        barSpeed: BarSpeed.fromJson(json['bar_speed'] as String?),
        isBackoff: json['is_backoff'] as bool?,
        plannedReps: asInt(json['planned_reps']),
        notes: json['notes'] as String?,
      );

  double? weightKg;
  int? reps;
  double? durationSec;
  List<int>? cluster;
  int? totalReps;
  double? rir;
  RepBand? repBand;
  BarSpeed? barSpeed;
  bool? isBackoff;
  int? plannedReps;
  String? notes;

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    put(json, 'weight_kg', weightKg);
    put(json, 'reps', reps);
    put(json, 'duration_sec', durationSec);
    put(json, 'cluster', cluster);
    put(json, 'total_reps', totalReps);
    put(json, 'rir', rir);
    put(json, 'rep_band', repBand?.name);
    put(json, 'bar_speed', barSpeed?.name);
    put(json, 'is_backoff', isBackoff);
    put(json, 'planned_reps', plannedReps);
    put(json, 'notes', notes);
    return json;
  }

  WorkSet copy() => WorkSet.fromJson(toJson());
}

enum RepBand {
  heavy, // <=3
  base, // 4-6
  volume; // 7+

  static RepBand? fromJson(String? value) =>
      value == null ? null : RepBand.values.byName(value);
}

enum BarSpeed {
  fast,
  ok,
  slow,
  grind;

  static BarSpeed? fromJson(String? value) =>
      value == null ? null : BarSpeed.values.byName(value);
}
