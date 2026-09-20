import 'package:dart_mcp/server.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../arguments.dart';
import '../workspace.dart';
import 'resolve.dart';

/// The sessions in the archive, filtered, one summary line each.
Future<Object?> listSessions(Workspace ws, CallToolRequest request) async {
  final from = request.date('from');
  final to = request.date('to');
  final cycleDay = request.string('cycle_day');
  final kind = request.string('kind');
  final exercise = request.string('exercise');
  final limit = request.integer('limit') ?? 50;

  final loaded = await ws.loadAll(from: from, to: to);
  final catalogue = catalogueOrNull(ws);
  final matches = [
    for (final session in loaded.sessions)
      if ((cycleDay == null ||
              session.cycleDay.toLowerCase() == cycleDay.toLowerCase()) &&
          (kind == null || session.kind.name == kind) &&
          (exercise == null ||
              _mentions(session, exercise, catalogue)))
        session,
  ];

  return {
    'folder': ws.label,
    'count': matches.length,
    'sessions': [
      for (final session in matches.take(limit)) _summary(session),
    ],
    if (matches.length > limit)
      'truncated': 'showing $limit of ${matches.length} — narrow the range or '
          'raise "limit"',
    if (loaded.failures.isNotEmpty)
      'unreadable': [
        for (final failure in loaded.failures)
          {'id': failure.id, 'error': '${failure.error}'},
      ],
  };
}

/// One session file, exactly as it is on disk, with anything questionable about
/// it listed beside it.
Future<Object?> readSession(Workspace ws, CallToolRequest request) async {
  final id = await resolveId(ws, request);
  final session = await ws.sessions.load(id);
  return {
    'id': id,
    'session': session.toJson(),
    'issues': _issues(session, catalogueOrNull(ws)),
  };
}

/// A new session file, from a cycle template when one covers the day.
Future<Object?> createSession(Workspace ws, CallToolRequest request) async {
  final date = request.date('date');
  if (date == null) throw ToolFailure('"date" is required');
  final cycleDay = request.requireString('cycle_day');
  final overwrite = request.flag('overwrite');

  final template = _templateFor(ws, cycleDay, request.string('from_cycle'));
  final session = template == null
      ? Session(date: date, cycleDay: cycleDay)
      : CycleGenerator.sessionFromTemplate(
          template.$2,
          DateTime.parse(date),
          // The plan says which weekday a slot belongs on; logging the session
          // you actually did on the day you did it is not an error.
          enforceWeekday: false,
        );

  session.kind = Kind.fromJson(request.string('kind')) ?? Kind.training;
  session.startTime = request.string('start_time') ?? session.startTime;
  session.bodyweightKg = request.number('bodyweight_kg');
  session.notes = request.string('notes') ?? session.notes;

  final id = SessionStore.idFor(session);
  if (!overwrite && await ws.sessions.exists(id)) {
    throw ToolFailure('$id already exists — pass "overwrite": true to replace it');
  }

  final issues = _requireWritable(session, catalogueOrNull(ws));
  await ws.sessions.save(session);
  return {
    'id': id,
    'from_cycle': template?.$1,
    'session': session.toJson(),
    'issues': issues,
  };
}

/// Replaces a session file with the document given, which is the general edit:
/// read it, change it, write it back.
Future<Object?> writeSession(Workspace ws, CallToolRequest request) async {
  final document = request.requireObject('session');
  final Session session;
  try {
    session = Session.fromJson(document);
  } catch (error) {
    throw ToolFailure('"session" is not a session document: $error');
  }

  final previousId = request.string('id');
  final id = SessionStore.idFor(session);
  if (previousId != null && !await ws.sessions.exists(previousId)) {
    throw ToolFailure('no such session: $previousId');
  }
  if (previousId == null &&
      !request.flag('overwrite') &&
      !await ws.sessions.exists(id)) {
    throw ToolFailure(
      '$id does not exist yet — use create_session, or pass "overwrite": true',
    );
  }

  final issues = _requireWritable(session, catalogueOrNull(ws));
  await ws.sessions.save(session, previousId: previousId);
  return {
    'id': id,
    if (previousId != null && previousId != id) 'renamed_from': previousId,
    'session': session.toJson(),
    'issues': issues,
  };
}

/// Fills a strength slot from the paper-log notation: `6 × [70, 80, 90]`.
Future<Object?> logSets(Workspace ws, CallToolRequest request) async {
  final id = await resolveId(ws, request);
  final notation = request.requireString('notation');
  final append = request.enumeration(
    'mode',
    const {'replace': false, 'append': true},
    false,
  );

  final session = await ws.sessions.load(id);
  final catalogue = catalogueOrNull(ws);
  final block = _strengthBlock(session, request, catalogue);
  final parsed = Notation.parseStrengthSets(notation);
  if (parsed.sets.isEmpty) {
    throw ToolFailure(
      'nothing parsed out of "$notation"${parsed.warnings.isEmpty ? '' : ': ${parsed.warnings.join('; ')}'}',
    );
  }

  block.$2.sets = append ? [...block.$2.sets, ...parsed.sets] : parsed.sets;
  final issues = _requireWritable(session, catalogue);
  await ws.sessions.save(session);
  return {
    'id': id,
    'block_index': block.$1,
    'exercise': block.$2.exercise,
    'sets': [for (final set in block.$2.sets) set.toJson()],
    'parse_warnings': parsed.warnings,
    'issues': issues,
  };
}

/// Deletes a session file. Requires `confirm` because the folder is the source
/// of truth and there is no other copy of it (ADR-001).
Future<Object?> deleteSession(Workspace ws, CallToolRequest request) async {
  final id = await resolveId(ws, request);
  if (!request.flag('confirm')) {
    throw ToolFailure('refusing to delete $id without "confirm": true');
  }
  final session = await ws.sessions.load(id);
  await ws.sessions.delete(id);
  return {'deleted': id, 'was': session.toJson()};
}

/// The notation grammar on its own, so a line can be checked before it is
/// written to a file.
Future<Object?> parseNotation(Workspace ws, CallToolRequest request) async {
  final parsed = Notation.parseStrengthSets(request.requireString('notation'));
  return {
    'sets': [for (final set in parsed.sets) set.toJson()],
    'warnings': parsed.warnings,
  };
}

Map<String, dynamic> _summary(Session session) {
  final metrics = SessionMetrics.of(session);
  return {
    'id': SessionStore.idFor(session),
    'date': session.date,
    'cycle_day': session.cycleDay,
    'kind': session.kind.name,
    'exercises': _exerciseNames(session),
    if (metrics.sets > 0) 'sets': metrics.sets,
    if (metrics.tonnageKg > 0) 'tonnage_kg': metrics.tonnageKg,
    if (metrics.density.totalMin != null)
      'duration_min': metrics.density.totalMin,
    if (session.notes != null) 'notes': session.notes,
  };
}

List<String> _exerciseNames(Session session) => [
      for (final block in session.blocks)
        ...switch (block) {
          StrengthBlock(:final exercise) => [exercise],
          MetconBlock() => [for (final e in block.exercises) e.name],
          _ => const <String>[],
        },
    ];

bool _mentions(Session session, String exercise, Catalogue? catalogue) {
  final wanted = catalogue?.resolve(exercise)?.name ?? exercise;
  return _exerciseNames(session).any((name) =>
      (catalogue?.resolve(name)?.name ?? name).toLowerCase() ==
      wanted.toLowerCase());
}


/// The strength slot a tool call means: an index, or the exercise's own name.
(int, StrengthBlock) _strengthBlock(
  Session session,
  CallToolRequest request,
  Catalogue? catalogue,
) {
  final index = request.integer('block_index');
  if (index != null) {
    if (index < 0 || index >= session.blocks.length) {
      throw ToolFailure(
        'block_index $index is outside 0..${session.blocks.length - 1}',
      );
    }
    final block = session.blocks[index];
    if (block is! StrengthBlock) {
      throw ToolFailure('block $index is a ${block.type} block, not strength');
    }
    return (index, block);
  }

  final exercise = request.string('exercise');
  if (exercise == null) {
    throw ToolFailure('pass "block_index" or "exercise"');
  }
  final wanted = catalogue?.resolve(exercise)?.name ?? exercise;
  final matches = <(int, StrengthBlock)>[];
  for (var i = 0; i < session.blocks.length; i++) {
    final block = session.blocks[i];
    if (block is! StrengthBlock) continue;
    final name = catalogue?.resolve(block.exercise)?.name ?? block.exercise;
    if (name.toLowerCase() == wanted.toLowerCase()) matches.add((i, block));
  }
  if (matches.isEmpty) {
    throw ToolFailure('no strength block for "$exercise" in this session');
  }
  if (matches.length > 1) {
    throw ToolFailure(
      '"$exercise" appears in blocks ${matches.map((m) => m.$1).join(', ')} — '
      'pass "block_index"',
    );
  }
  return matches.single;
}

/// The cycle template for a day: the one named, else the only cycle that has
/// that day. Several candidates is a question for the caller, not a guess.
(String, CycleSession)? _templateFor(
  Workspace ws,
  String cycleDay,
  String? cycleId,
) {
  final CycleCatalogue cycles;
  try {
    cycles = ws.loadCycles();
  } on WorkspaceException {
    if (cycleId == null) return null;
    rethrow;
  }

  if (cycleId != null) {
    final cycle = cycles.byId(cycleId);
    if (cycle == null) {
      throw ToolFailure(
        'no cycle "$cycleId" — have ${cycles.cycles.map((c) => c.id).join(', ')}',
      );
    }
    final template = _sessionFor(cycle, cycleDay);
    if (template == null) {
      throw ToolFailure('cycle "$cycleId" has no $cycleDay');
    }
    return (cycle.id, template);
  }

  final candidates = [
    for (final cycle in cycles.cycles)
      if (_sessionFor(cycle, cycleDay) case final template?)
        (cycle.id, template),
  ];
  if (candidates.isEmpty) return null;
  if (candidates.length > 1) {
    throw ToolFailure(
      '$cycleDay is defined in ${candidates.map((c) => c.$1).join(', ')} — '
      'pass "from_cycle"',
    );
  }
  return candidates.single;
}

CycleSession? _sessionFor(Cycle cycle, String cycleDay) {
  for (final session in cycle.sessions) {
    if (session.cycleDay.toLowerCase() == cycleDay.toLowerCase()) return session;
  }
  return null;
}


List<Map<String, dynamic>> _issues(Session session, Catalogue? catalogue) => [
      for (final issue in SessionValidator.validate(session, catalogue: catalogue))
        issue.toJson(),
    ];

/// Warnings travel with the result; errors stop the write. A half-written plan
/// is a normal state of a file, a malformed one is not.
List<Map<String, dynamic>> _requireWritable(
  Session session,
  Catalogue? catalogue,
) {
  final issues = SessionValidator.validate(session, catalogue: catalogue);
  if (SessionValidator.hasErrors(issues)) {
    final errors = issues.where((i) => i.severity == IssueSeverity.error);
    throw ToolFailure(
      'refusing to write an invalid session: ${errors.join('; ')}',
    );
  }
  return [for (final issue in issues) issue.toJson()];
}
