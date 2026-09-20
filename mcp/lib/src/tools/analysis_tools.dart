import 'package:dart_mcp/server.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../arguments.dart';
import '../workspace.dart';
import 'resolve.dart';

const _activation = MuscleActivation();

const _weightingModes = {
  'set_count': WeightingMode.setCount,
  'rep_volume': WeightingMode.repVolume,
  'tonnage': WeightingMode.tonnage,
};

/// One session's derived numbers: tonnage, rep bands, density, metcon splits.
Future<Object?> analyzeSession(Workspace ws, CallToolRequest request) async {
  final id = await resolveId(ws, request);
  final session = await ws.sessions.load(id);
  final catalogue = catalogueOrNull(ws);
  final mode = request.enumeration('mode', _weightingModes, WeightingMode.setCount);

  final result = <String, dynamic>{
    'id': id,
    'metrics': SessionMetrics.of(session).toJson(),
  };
  if (catalogue != null) {
    final scores = _activation.forSession(
      session,
      catalogue: catalogue,
      mode: mode,
    );
    result['muscles'] = _muscleJson(scores, mode);
  }
  result['issues'] = [
    for (final issue in SessionValidator.validate(session, catalogue: catalogue))
      issue.toJson(),
  ];
  return result;
}

/// A stretch of sessions read as a block: rep-band distribution, pattern
/// frequency, combined load, and the alerts those raise.
Future<Object?> analyzeTraining(Workspace ws, CallToolRequest request) async {
  final loaded = await ws.loadAll(
    from: request.date('from'),
    to: request.date('to'),
  );
  if (loaded.sessions.isEmpty) {
    throw ToolFailure('no sessions in that range');
  }
  final report = TrainingReport.of(
    loaded.sessions,
    catalogue: requireCatalogue(ws),
    cycleLengthDays: request.integer('cycle_length_days') ?? 12,
    volumeSlotShareThreshold: request.number('volume_share_threshold') ?? 0.3,
  );
  final json = report.toJson();
  if (!request.flag('include_load', fallback: true)) json.remove('load');
  if (loaded.failures.isNotEmpty) {
    json['unreadable'] = [
      for (final failure in loaded.failures)
        {'id': failure.id, 'error': '${failure.error}'},
    ];
  }
  return json;
}

/// Best set over time for one lift, one track per rep band (P6).
Future<Object?> exerciseProgress(Workspace ws, CallToolRequest request) async {
  final loaded = await ws.loadAll(
    from: request.date('from'),
    to: request.date('to'),
  );
  final report = ExerciseProgress.report(
    loaded.sessions,
    exercise: request.requireString('exercise'),
    catalogue: catalogueOrNull(ws),
    byPattern: request.flag('by_pattern'),
  );
  final json = report.toJson();
  if (report.series.isEmpty) {
    json['note'] = 'no sets recorded for this lift in the range — '
        'list_exercises will say whether the name is the catalogue one';
  }
  return json;
}

/// Which muscles the work landed on, for one session or a whole range.
Future<Object?> muscleActivation(Workspace ws, CallToolRequest request) async {
  final catalogue = requireCatalogue(ws);
  final mode = request.enumeration('mode', _weightingModes, WeightingMode.setCount);
  final id = request.string('id');
  final date = request.date('date');

  if (id != null || date != null) {
    final sessionId = await resolveId(ws, request);
    final session = await ws.sessions.load(sessionId);
    return {
      'id': sessionId,
      ..._muscleJson(
        _activation.forSession(session, catalogue: catalogue, mode: mode),
        mode,
      ),
    };
  }

  final loaded = await ws.loadAll(
    from: request.date('from'),
    to: request.date('to'),
  );
  if (loaded.sessions.isEmpty) throw ToolFailure('no sessions in that range');
  return {
    'sessions': loaded.sessions.length,
    'from': loaded.sessions.first.date,
    'to': loaded.sessions.last.date,
    ..._muscleJson(
      _activation.forSessions(loaded.sessions, catalogue: catalogue, mode: mode),
      mode,
    ),
  };
}

/// Every session the archive cannot read, and everything questionable in the
/// ones it can.
Future<Object?> validateArchive(Workspace ws, CallToolRequest request) async {
  final loaded = await ws.loadAll(
    from: request.date('from'),
    to: request.date('to'),
  );
  final catalogue = catalogueOrNull(ws);
  final reports = <Map<String, dynamic>>[];
  var errors = 0;
  var warnings = 0;
  for (final session in loaded.sessions) {
    final issues = SessionValidator.validate(session, catalogue: catalogue);
    if (issues.isEmpty) continue;
    errors += issues.where((i) => i.severity == IssueSeverity.error).length;
    warnings += issues.where((i) => i.severity == IssueSeverity.warning).length;
    reports.add({
      'id': SessionStore.idFor(session),
      'issues': [for (final issue in issues) issue.toJson()],
    });
  }
  return {
    'checked': loaded.sessions.length,
    'errors': errors,
    'warnings': warnings,
    'sessions': reports,
    if (loaded.failures.isNotEmpty)
      'unreadable': [
        for (final failure in loaded.failures)
          {'id': failure.id, 'error': '${failure.error}'},
      ],
  };
}

/// Scores are normalized so the hottest muscle is 1.0, which is what the map
/// colours; the group rollup answers "which region" instead of "how hard".
Map<String, dynamic> _muscleJson(Map<String, double> scores, WeightingMode mode) {
  final entries = scores.entries.toList()
    ..sort((a, b) {
      final byScore = b.value.compareTo(a.value);
      return byScore != 0 ? byScore : a.key.compareTo(b.key);
    });
  final groups = <String, double>{};
  for (final entry in entries) {
    final group = MuscleGroup.of(entry.key);
    if (group == null) continue;
    groups[group.name] = (groups[group.name] ?? 0) + entry.value;
  }
  return {
    'weighting': mode.wire,
    'muscles': {
      for (final entry in entries) entry.key: _round(entry.value),
    },
    'groups': {
      for (final entry in groups.entries) entry.key: _round(entry.value),
    },
    if (MuscleGroup.dominant(scores) case final dominant?)
      'dominant_group': dominant.name,
  };
}

double _round(double value) => (value * 1000).round() / 1000;
