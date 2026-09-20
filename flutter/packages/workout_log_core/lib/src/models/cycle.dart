import 'package:collection/collection.dart';

import '../coding.dart';
import 'block.dart';

/// A reusable cycle definition: an ordered list of session templates with no
/// weights. `role` and `sets_reps` are planning-only helpers consumed by
/// `CycleGenerator` and never written to a session file.
///
/// Every level keeps an `extras` map of the keys it does not model. The planner
/// writes `cycles.json` back out, so anything dropped on read would be deleted
/// from the user's file on the next save — `skipped` and `role` are exactly
/// that, and nothing in the app would have noticed.
class CycleCatalogue {
  CycleCatalogue({required this.cycles, this.comment});

  factory CycleCatalogue.fromJson(Map<String, dynamic> json) => CycleCatalogue(
        comment: json[r'$comment'] as String?,
        cycles: [
          for (final c in json['cycles'] as List)
            Cycle.fromJson(c as Map<String, dynamic>),
        ],
      );

  final List<Cycle> cycles;
  final String? comment;

  Cycle? byId(String id) => cycles.firstWhereOrNull((c) => c.id == id);

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    put(json, r'$comment', comment);
    json['cycles'] = [for (final c in cycles) c.toJson()];
    return json;
  }
}

class Cycle {
  Cycle({
    required this.id,
    required this.name,
    required this.trainingDays,
    required this.startDate,
    required this.sessions,
    Map<String, dynamic>? extras,
  }) : extras = extras ?? {};

  factory Cycle.fromJson(Map<String, dynamic> json) => Cycle(
        id: json['id'] as String,
        name: json['name'] as String? ?? json['id'] as String,
        trainingDays: asStringList(json['training_days']),
        startDate: json['start_date'] as String,
        sessions: [
          for (final s in json['sessions'] as List)
            CycleSession.fromJson(s as Map<String, dynamic>),
        ],
        extras: unmodelledKeys(json, _modelled),
      );

  static const _modelled = {
    'id',
    'name',
    'training_days',
    'start_date',
    'sessions',
  };

  String id;
  String name;
  List<String> trainingDays;
  String startDate;
  List<CycleSession> sessions;
  final Map<String, dynamic> extras;

  Cycle copy() => Cycle.fromJson(toJson());

  Map<String, dynamic> toJson() => {
        ...extras,
        'id': id,
        'name': name,
        'training_days': trainingDays,
        'start_date': startDate,
        'sessions': [for (final s in sessions) s.toJson()],
      };
}

class CycleSession {
  CycleSession({
    required this.cycleDay,
    required this.blocks,
    this.week,
    this.weekday,
    this.type,
    this.title,
    this.sessionNotes,
    Map<String, dynamic>? extras,
  }) : extras = extras ?? {};

  factory CycleSession.fromJson(Map<String, dynamic> json) => CycleSession(
        cycleDay: json['cycle_day'] as String,
        week: asInt(json['week']),
        weekday: json['weekday'] as String?,
        type: json['type'] as String?,
        title: json['title'] as String?,
        sessionNotes: json['session_notes'] as String?,
        blocks: [
          for (final b in json['blocks'] as List)
            BlockTemplate.fromJson(b as Map<String, dynamic>),
        ],
        extras: unmodelledKeys(json, _modelled),
      );

  static const _modelled = {
    'cycle_day',
    'week',
    'weekday',
    'type',
    'title',
    'session_notes',
    'blocks',
  };

  String cycleDay;
  int? week;
  String? weekday;
  String? type;
  String? title;
  String? sessionNotes;
  List<BlockTemplate> blocks;
  final Map<String, dynamic> extras;

  CycleSession copy() => CycleSession.fromJson(toJson());

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{...extras, 'cycle_day': cycleDay};
    put(json, 'week', week);
    put(json, 'weekday', weekday);
    put(json, 'type', type);
    put(json, 'title', title);
    put(json, 'session_notes', sessionNotes);
    json['blocks'] = [for (final b in blocks) b.toJson()];
    return json;
  }
}

class BlockTemplate {
  BlockTemplate({
    required this.type,
    this.role,
    this.machine,
    this.durationMin,
    this.exercise,
    List<int>? setsReps,
    this.notes,
    this.format,
    this.scheme,
    List<MetconExercise>? exercises,
    Map<String, dynamic>? extras,
    Set<String>? presentKeys,
  })  : setsReps = setsReps ?? [],
        exercises = exercises ?? [],
        extras = extras ?? {},
        presentKeys = presentKeys ?? const {};

  factory BlockTemplate.fromJson(Map<String, dynamic> json) => BlockTemplate(
        type: json['type'] as String,
        role: json['role'] as String?,
        machine: json['machine'] as String?,
        durationMin: asDouble(json['duration_min']),
        exercise: json['exercise'] as String?,
        setsReps: asIntList(json['sets_reps']),
        notes: json['notes'] as String?,
        format: MetconFormat.fromJson(json['format'] as String?),
        scheme: asIntList(json['scheme']),
        exercises: [
          for (final e in json['exercises'] as List? ?? const [])
            MetconExercise.fromJson(e as Map<String, dynamic>),
        ],
        extras: unmodelledKeys(json, _modelled),
        presentKeys: json.keys.toSet(),
      );

  static const _modelled = {
    'type',
    'role',
    'machine',
    'duration_min',
    'exercise',
    'sets_reps',
    'notes',
    'format',
    'scheme',
    'exercises',
  };

  String type;
  String? role;
  String? machine;
  double? durationMin;
  String? exercise;
  List<int> setsReps;
  String? notes;
  MetconFormat? format;
  List<int>? scheme;
  List<MetconExercise> exercises;
  final Map<String, dynamic> extras;

  /// See [Exercise.presentKeys] — an empty list that was written out by hand
  /// must survive a save.
  final Set<String> presentKeys;

  BlockTemplate copy() => BlockTemplate.fromJson(toJson());

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{...extras, 'type': type};
    put(json, 'role', role);
    put(json, 'machine', machine);
    put(json, 'duration_min', durationMin);
    put(json, 'exercise', exercise);
    if (setsReps.isNotEmpty || presentKeys.contains('sets_reps')) {
      json['sets_reps'] = setsReps;
    }
    put(json, 'notes', notes);
    put(json, 'format', format?.wire);
    put(json, 'scheme', scheme);
    if (exercises.isNotEmpty || presentKeys.contains('exercises')) {
      json['exercises'] = [for (final e in exercises) e.toJson()];
    }
    return json;
  }
}
