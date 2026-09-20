import 'package:dart_mcp/server.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../arguments.dart';
import '../workspace.dart';
import 'resolve.dart';

/// The exercise catalogue, filtered. `known_muscles` travels with every answer
/// because it is the vocabulary a new entry has to pick from.
Future<Object?> listExercises(Workspace ws, CallToolRequest request) async {
  final catalogue = requireCatalogue(ws);
  final query = request.string('query')?.toLowerCase();
  final pattern = request.string('pattern');
  final modality = request.string('modality');
  final muscle = request.string('muscle');
  final limit = request.integer('limit') ?? 40;

  final matches = [
    for (final exercise in catalogue.exercises)
      if ((query == null ||
              exercise.name.toLowerCase().contains(query) ||
              exercise.aliases.any((a) => a.toLowerCase().contains(query))) &&
          (pattern == null || exercise.pattern?.name == pattern) &&
          (modality == null || exercise.modality?.name == modality) &&
          (muscle == null ||
              exercise.primaryMuscles.contains(muscle) ||
              exercise.secondaryMuscles.contains(muscle)))
        exercise,
  ];

  return {
    'count': matches.length,
    'exercises': [for (final e in matches.take(limit)) e.toJson()],
    if (matches.length > limit) 'truncated': 'showing $limit of ${matches.length}',
    'known_muscles': catalogue.knownMuscles,
  };
}

/// Adds an entry to `exercises.json`, or replaces the one with that name.
///
/// The catalogue is hand-maintained, so a write preserves the keys the model
/// does not know about and the file's own `$comment`.
Future<Object?> addExercise(Workspace ws, CallToolRequest request) async {
  final catalogue = requireCatalogue(ws);
  final name = request.requireString('name');
  final existing = catalogue.exercises
      .where((e) => e.name.toLowerCase() == name.toLowerCase())
      .firstOrNull;
  if (existing != null && !request.flag('overwrite')) {
    throw ToolFailure(
      '"$name" is already in the catalogue — pass "overwrite": true to replace it',
    );
  }

  final primary = request.strings('primary_muscles');
  if (primary.isEmpty) {
    throw ToolFailure(
      '"primary_muscles" is required — an exercise with none contributes '
      'nothing to the muscle map',
    );
  }
  final secondary = request.strings('secondary_muscles');
  final known = catalogue.knownMuscles.toSet();
  final unknown = [
    for (final muscle in [...primary, ...secondary])
      if (!known.contains(muscle)) muscle,
  ];

  final exercise = Exercise(
    name: name,
    aliases: request.strings('aliases'),
    pattern: request.enumeration<MovementPattern?>(
      'pattern',
      {for (final p in MovementPattern.values) p.name: p},
      null,
    ),
    modality: request.enumeration<Modality?>(
      'modality',
      {for (final m in Modality.values) m.name: m},
      null,
    ),
    category: request.enumeration(
      'category',
      {for (final c in ExerciseCategory.values) c.name: c},
      ExerciseCategory.strength,
    ),
    primaryMuscles: primary,
    secondaryMuscles: secondary,
    notes: request.string('notes'),
    extras: existing?.extras,
    presentKeys: existing?.presentKeys,
  );

  ws.saveCatalogue(catalogue.withExercise(exercise));
  return {
    'saved': ws.catalogueFile.path,
    'replaced': existing != null,
    'exercise': exercise.toJson(),
    if (unknown.isNotEmpty)
      'new_muscle_names': {
        'names': unknown,
        'note': 'outside the vocabulary the rest of the catalogue uses, so the '
            'muscle map will not colour them — check the spelling against '
            'known_muscles',
      },
  };
}

/// The cycle definitions: the plan side, `cycles.json`.
Future<Object?> listCycles(Workspace ws, CallToolRequest request) async {
  final catalogue = ws.loadCycles();
  final id = request.string('id');
  if (id != null) {
    final cycle = catalogue.byId(id);
    if (cycle == null) {
      throw ToolFailure(
        'no cycle "$id" — have ${catalogue.cycles.map((c) => c.id).join(', ')}',
      );
    }
    return {'cycle': cycle.toJson()};
  }
  return {
    'cycles': [
      for (final cycle in catalogue.cycles)
        {
          'id': cycle.id,
          'name': cycle.name,
          'training_days': cycle.trainingDays,
          'start_date': cycle.startDate,
          'days': [
            for (final session in cycle.sessions)
              {
                'cycle_day': session.cycleDay,
                if (session.title != null) 'title': session.title,
                if (session.weekday != null) 'weekday': session.weekday,
                'blocks': [
                  for (final block in session.blocks)
                    block.exercise ?? block.machine ?? block.type,
                ],
              },
          ],
        },
    ],
  };
}

/// Expands a cycle onto the calendar, writing one session stub per template day
/// — the same job as `wl_gen_cycle`, callable from a conversation.
Future<Object?> generateCycle(Workspace ws, CallToolRequest request) async {
  final catalogue = ws.loadCycles();
  final id = request.requireString('cycle_id');
  final cycle = catalogue.byId(id);
  if (cycle == null) {
    throw ToolFailure(
      'no cycle "$id" — have ${catalogue.cycles.map((c) => c.id).join(', ')}',
    );
  }
  final startDate = request.date('start_date');
  final force = request.flag('force');
  final dryRun = request.flag('dry_run');

  final expanded = cycle.copy();
  if (startDate != null) expanded.startDate = startDate;

  final List<Session> sessions;
  try {
    sessions = CycleGenerator.generate(expanded);
  } on CycleGeneratorException catch (error) {
    throw ToolFailure('$error');
  }

  final written = <Map<String, dynamic>>[];
  for (final session in sessions) {
    final sessionId = SessionStore.idFor(session);
    final exists = await ws.sessions.exists(sessionId);
    final skipped = exists && !force;
    if (!skipped && !dryRun) await ws.sessions.save(session);
    written.add({
      'id': sessionId,
      'cycle_day': session.cycleDay,
      'status': switch ((skipped, dryRun)) {
        (true, _) => 'skipped (exists — pass "force": true to overwrite)',
        (false, true) => 'would write',
        (false, false) => exists ? 'overwritten' : 'written',
      },
    });
  }

  return {
    'cycle': cycle.id,
    'start_date': expanded.startDate,
    'dry_run': dryRun,
    'sessions': written,
  };
}
