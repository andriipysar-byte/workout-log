import '../coding.dart';
import 'block.dart';

/// Files on disk are the source of truth (ADR-001); this is a lossless projection.
class Session with JsonEquality {
  Session({
    required this.date,
    required this.cycleDay,
    this.startTime,
    this.kind = Kind.training,
    this.bodyweightKg,
    this.notes,
    List<Block>? blocks,
  }) : blocks = blocks ?? [];

  factory Session.fromJson(Map<String, dynamic> json) => Session(
        date: json['date'] as String,
        cycleDay: json['cycle_day'] as String,
        startTime: json['start_time'] as String?,
        kind: Kind.fromJson(json['kind'] as String?) ?? Kind.training,
        bodyweightKg: asDouble(json['bodyweight_kg']),
        notes: json['notes'] as String?,
        blocks: [
          for (final b in json['blocks'] as List)
            Block.fromJson(b as Map<String, dynamic>),
        ],
      );

  String date;
  String cycleDay;
  String? startTime;
  Kind kind;
  double? bodyweightKg;
  String? notes;
  List<Block> blocks;

  @override
  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'date': date, 'cycle_day': cycleDay};
    put(json, 'start_time', startTime);
    json['kind'] = kind.name;
    put(json, 'bodyweight_kg', bodyweightKg);
    put(json, 'notes', notes);
    json['blocks'] = [for (final b in blocks) b.toJson()];
    return json;
  }

  Session copy() => Session.fromJson(toJson());
}

enum Kind {
  training,
  deload,
  retest;

  static Kind? fromJson(String? value) =>
      value == null ? null : Kind.values.byName(value);
}
