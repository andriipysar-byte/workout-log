import 'package:dart_mcp/server.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../arguments.dart';
import '../workspace.dart';

/// The session file a call means: `id`, or the `date` (plus `cycle_day`, on the
/// rare day that holds two sessions) that names it.
Future<String> resolveId(Workspace ws, CallToolRequest request) async {
  final ids = await ws.sessions.listIds();
  final explicit = request.string('id');
  if (explicit != null) {
    final id = explicit.endsWith('.json') ? explicit : '$explicit.json';
    if (!ids.contains(id)) throw ToolFailure('no such session: $id');
    return id;
  }

  final date = request.date('date');
  if (date == null) throw ToolFailure('pass "id", or "date" and "cycle_day"');
  final cycleDay = request.string('cycle_day');
  final matches = [
    for (final id in ids)
      if (id.startsWith('${date}_') &&
          (cycleDay == null ||
              id.toLowerCase() == '${date}_${cycleDay.toLowerCase()}.json'))
        id,
  ];
  if (matches.isEmpty) throw ToolFailure('no session on $date');
  if (matches.length > 1) {
    throw ToolFailure(
      '$date has several sessions (${matches.join(', ')}) — pass "id"',
    );
  }
  return matches.single;
}

/// The catalogue, or null where it is only an improvement: an unknown exercise
/// name warns, it never blocks a write (ADR-006).
Catalogue? catalogueOrNull(Workspace ws) {
  try {
    return ws.loadCatalogue();
  } on WorkspaceException {
    return null;
  }
}

/// The catalogue, for the analytics that cannot mean anything without it.
Catalogue requireCatalogue(Workspace ws) {
  try {
    return ws.loadCatalogue();
  } on WorkspaceException catch (error) {
    throw ToolFailure('$error');
  }
}
