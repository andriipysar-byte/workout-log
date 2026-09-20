import 'package:collection/collection.dart';

import '../coding.dart';
import 'block.dart';

/// A reusable cycle definition: an ordered list of session templates with no
/// weights. `role` and `setsReps` are planning-only helpers consumed by
/// `CycleGenerator` and never written to a session file.
class CycleCatalogue {
  CycleCatalogue({required this.cycles});

  factory CycleCatalogue.fromJson(Map<String, dynamic> json) => CycleCatalogue(
        cycles: [
          for (final c in json['cycles'] as List)
            Cycle.fromJson(c as Map<String, dynamic>),
        ],
      );

  final List<Cycle> cycles;

  Cycle? byId(String id) => cycles.where((c) => c.id == id).firstOrNull;
}

class Cycle {
  Cycle({
    required this.id,
    required this.name,
    required this.trainingDays,
    required this.startDate,
    required this.sessions,
  });

  factory Cycle.fromJson(Map<String, dynamic> json) => Cycle(
        id: json['id'] as String,
        name: json['name'] as String? ?? json['id'] as String,
        trainingDays: asStringList(json['training_days']),
        startDate: json['start_date'] as String,
        sessions: [
          for (final s in json['sessions'] as List)
            CycleSession.fromJson(s as Map<String, dynamic>),
        ],
      );

  final String id;
  final String name;
  final List<String> trainingDays;
  final String startDate;
  final List<CycleSession> sessions;
}

class CycleSession {
  CycleSession({
    required this.cycleDay,
    required this.blocks,
    this.week,
    this.weekday,
    this.title,
    this.sessionNotes,
  });

  factory CycleSession.fromJson(Map<String, dynamic> json) => CycleSession(
        cycleDay: json['cycle_day'] as String,
        week: asInt(json['week']),
        weekday: json['weekday'] as String?,
        title: json['title'] as String?,
        sessionNotes: json['session_notes'] as String?,
        blocks: [
          for (final b in json['blocks'] as List)
            BlockTemplate.fromJson(b as Map<String, dynamic>),
        ],
      );

  final String cycleDay;
  final int? week;
  final String? weekday;
  final String? title;
  final String? sessionNotes;
  final List<BlockTemplate> blocks;
}

class BlockTemplate {
  BlockTemplate({
    required this.type,
    this.machine,
    this.durationMin,
    this.exercise,
    this.setsReps = const [],
    this.notes,
    this.format,
    this.scheme,
    this.exercises = const [],
  });

  factory BlockTemplate.fromJson(Map<String, dynamic> json) => BlockTemplate(
        type: json['type'] as String,
        machine: json['machine'] as String?,
        durationMin: asDouble(json['duration_min']),
        exercise: json['exercise'] as String?,
        setsReps: asIntList(json['sets_reps']) ?? const [],
        notes: json['notes'] as String?,
        format: MetconFormat.fromJson(json['format'] as String?),
        scheme: asIntList(json['scheme']),
        exercises: [
          for (final e in json['exercises'] as List? ?? const [])
            MetconExercise.fromJson(e as Map<String, dynamic>),
        ],
      );

  final String type;
  final String? machine;
  final double? durationMin;
  final String? exercise;
  final List<int> setsReps;
  final String? notes;
  final MetconFormat? format;
  final List<int>? scheme;
  final List<MetconExercise> exercises;
}
