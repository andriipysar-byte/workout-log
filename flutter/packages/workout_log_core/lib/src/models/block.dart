import '../coding.dart';
import 'work_set.dart';

part 'metcon.dart';

/// Flat tagged union: peek `type`, then build the matching subclass from the
/// *same* map (the tag is a sibling of the payload, not a wrapper around it).
///
/// `MetconBlock` lives in `metcon.dart` as a part because `sealed` requires every
/// subclass in the same library, and that is what buys exhaustive `switch`.
sealed class Block with JsonEquality {
  Block();

  factory Block.fromJson(Map<String, dynamic> json) {
    final type = json['type'] as String;
    return switch (type) {
      'cardio' => CardioBlock.fromJson(json),
      'strength' => StrengthBlock.fromJson(json),
      'metcon' => MetconBlock.fromJson(json),
      'cooldown' => CooldownBlock.fromJson(json),
      _ => throw FormatException('Unknown block type "$type"'),
    };
  }

  String get type;

  Block copy() => Block.fromJson(toJson());
}

/// `end_time` (on every block) is the bracket timestamp from the paper log — the
/// source of block duration and session density.
class CardioBlock extends Block {
  CardioBlock({
    required this.machine,
    this.durationMin,
    this.distanceM,
    this.endTime,
  });

  factory CardioBlock.fromJson(Map<String, dynamic> json) => CardioBlock(
        machine: json['machine'] as String,
        durationMin: asDouble(json['duration_min']),
        distanceM: asDouble(json['distance_m']),
        endTime: json['end_time'] as String?,
      );

  String machine;
  double? durationMin;
  double? distanceM;
  String? endTime;

  @override
  String get type => 'cardio';

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'type': type, 'machine': machine};
    put(json, 'duration_min', durationMin);
    put(json, 'distance_m', distanceM);
    put(json, 'end_time', endTime);
    return json;
  }
}

class StrengthBlock extends Block {
  StrengthBlock({
    required this.exercise,
    List<WorkSet>? sets,
    this.startTime,
    this.endTime,
    this.notes,
  }) : sets = sets ?? [];

  factory StrengthBlock.fromJson(Map<String, dynamic> json) => StrengthBlock(
        exercise: json['exercise'] as String,
        sets: [
          for (final s in json['sets'] as List)
            WorkSet.fromJson(s as Map<String, dynamic>),
        ],
        startTime: json['start_time'] as String?,
        endTime: json['end_time'] as String?,
        notes: json['notes'] as String?,
      );

  String exercise;
  List<WorkSet> sets;
  String? startTime;
  String? endTime;
  String? notes;

  @override
  String get type => 'strength';

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'type': type, 'exercise': exercise};
    json['sets'] = [for (final s in sets) s.toJson()];
    put(json, 'start_time', startTime);
    put(json, 'end_time', endTime);
    put(json, 'notes', notes);
    return json;
  }
}

class CooldownBlock extends Block {
  CooldownBlock({this.endTime, this.notes});

  factory CooldownBlock.fromJson(Map<String, dynamic> json) => CooldownBlock(
        endTime: json['end_time'] as String?,
        notes: json['notes'] as String?,
      );

  String? endTime;
  String? notes;

  @override
  String get type => 'cooldown';

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'type': type};
    put(json, 'end_time', endTime);
    put(json, 'notes', notes);
    return json;
  }
}
