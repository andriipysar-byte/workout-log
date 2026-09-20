import 'dart:async';

import 'package:dart_mcp/server.dart';

import 'arguments.dart';
import 'tools/analysis_tools.dart';
import 'tools/plan_tools.dart';
import 'tools/session_tools.dart';
import 'workspace.dart';

const _instructions = '''
A hybrid strength + conditioning training log: one JSON file per session in a
folder, which is the source of truth (ADR-001). Exercise names are Ukrainian and
resolve through exercises.json, which also carries aliases, movement patterns and
muscle targets; cycles.json holds the reusable session templates. A session is
identified by its file name, `YYYY-MM-DD_<cycle day>.json`, where the cycle day
is A1…F2 — the letter groups workouts by emphasis, 1 is a conditioning day and 2
is a heavy day.

Sets can be written in the log's own notation rather than as objects:
`6 × [70, 80, 90, 100, 110]` is five sets of six at ascending weights,
`6 × [70, 80] + 6 × [30]` makes the trailing group back-off sets, `90(4)` is a
per-set rep override, `5+5+4+3+3 (20)` is a cluster, and `54c` is a 54-second
hold. Use log_sets for that; use write_session for anything the notation cannot
express.

The analysis tools speak in the training principles the log is built on
(docs/01-training-principles.md): rep bands are independent progress tracks (P6),
the top set is the signal and back-offs are volume (P2), explosive lifts belong
at 2-3 reps (P3), and a block where volume slots take over is what produced an
involuntary deload (P5). The tools report; the programming decisions are the
athlete's.
''';

/// The MCP surface over the log. Every rule it applies lives in
/// `workout_log_core`; this layer parses arguments, calls the core and formats
/// JSON back (ADR-004 again, with an LLM as the UI).
base class WorkoutLogServer extends MCPServer with ToolsSupport {
  WorkoutLogServer(super.channel, {required this.workspace})
      : super.fromStreamChannel(
          implementation: Implementation(
            name: 'workout-log',
            version: '1.0.0',
          ),
          instructions: _instructions,
        ) {
    _registerTools();
  }

  final Workspace workspace;

  void _registerTools() {
    _tool(
      Tool(
        name: 'list_sessions',
        description: 'List logged sessions, newest last, with a one-line summary '
            'of each. Filter by date range, cycle day, kind or exercise.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(properties: {
          'from': _dateSchema('Earliest date, inclusive'),
          'to': _dateSchema('Latest date, inclusive'),
          'cycle_day': Schema.string(description: 'A1…F2'),
          'kind': _kindSchema,
          'exercise': Schema.string(
            description: 'Only sessions containing this exercise; aliases resolve',
          ),
          'limit': Schema.int(description: 'Default 50'),
        }),
      ),
      listSessions,
    );

    _tool(
      Tool(
        name: 'read_session',
        description: 'The full JSON of one session as it is on disk, with any '
            'validation issues it has.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(properties: _selector),
      ),
      readSession,
    );

    _tool(
      Tool(
        name: 'create_session',
        description: 'Write a new session file. When a cycle in cycles.json '
            'defines that cycle day, the session starts from its template — the '
            'planned blocks, exercises and rep schemes — otherwise it is empty. '
            'Fails if the file exists unless overwrite is set.',
        inputSchema: Schema.object(
          properties: {
            'date': _dateSchema('The day it was trained'),
            'cycle_day': Schema.string(description: 'A1…F2'),
            'from_cycle': Schema.string(
              description: 'Cycle id to take the template from; inferred when '
                  'only one cycle defines that day',
            ),
            'kind': _kindSchema,
            'start_time': Schema.string(description: 'HH:MM'),
            'bodyweight_kg': Schema.num(),
            'notes': Schema.string(),
            'overwrite': Schema.bool(description: 'Replace an existing file'),
          },
          required: ['date', 'cycle_day'],
        ),
      ),
      createSession,
    );

    _tool(
      Tool(
        name: 'write_session',
        description: 'Replace a session with the document given — the general '
            'edit: read_session, change the JSON, write it back. Moving the date '
            'or cycle day renames the file. Refuses documents with errors; '
            'warnings come back with the result.',
        annotations: ToolAnnotations(destructiveHint: true, idempotentHint: true),
        inputSchema: Schema.object(
          properties: {
            'session': Schema.object(
              description: 'A whole session document, as read_session returns it',
            ),
            'id': Schema.string(
              description: 'The file being edited, when the header may move it',
            ),
            'overwrite': Schema.bool(
              description: 'Allow writing a file that does not exist yet',
            ),
          },
          required: ['session'],
        ),
      ),
      writeSession,
    );

    _tool(
      Tool(
        name: 'log_sets',
        description: 'Fill a strength slot from the log notation, e.g. '
            '"6 × [70, 80, 90, 100, 110]" or "5+5+4+3+3 (20)". Names the slot by '
            'exercise or by block index.',
        inputSchema: Schema.object(
          properties: {
            ..._selector,
            'notation': Schema.string(description: 'The line as written on paper'),
            'exercise': Schema.string(description: 'Which slot; aliases resolve'),
            'block_index': Schema.int(description: 'Which slot, by position'),
            'mode': Schema.string(
              description: 'replace (default) or append',
              enumValues: ['replace', 'append'],
            ),
          },
          required: ['notation'],
        ),
      ),
      logSets,
    );

    _tool(
      Tool(
        name: 'delete_session',
        description: 'Delete a session file. The folder is the only copy, so '
            'this requires confirm: true.',
        annotations: ToolAnnotations(destructiveHint: true),
        inputSchema: Schema.object(
          properties: {
            ..._selector,
            'confirm': Schema.bool(description: 'Must be true'),
          },
          required: ['confirm'],
        ),
      ),
      deleteSession,
    );

    _tool(
      Tool(
        name: 'parse_notation',
        description: 'Turn a notation line into sets without writing anything — '
            'check what a line means before logging it.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(
          properties: {'notation': Schema.string()},
          required: ['notation'],
        ),
      ),
      parseNotation,
    );

    _tool(
      Tool(
        name: 'list_exercises',
        description: 'The exercise catalogue: canonical Ukrainian names, '
            'aliases, movement pattern, modality, category and muscle targets. '
            'Always returns the muscle vocabulary the catalogue uses.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(properties: {
          'query': Schema.string(description: 'Substring of a name or alias'),
          'pattern': _patternSchema,
          'modality': Schema.string(
            enumValues: ['barbell', 'dumbbell', 'kettlebell', 'bodyweight', 'machine'],
          ),
          'muscle': Schema.string(description: 'A muscle token, e.g. quads'),
          'limit': Schema.int(description: 'Default 40'),
        }),
      ),
      listExercises,
    );

    _tool(
      Tool(
        name: 'add_exercise',
        description: 'Add an exercise to exercises.json, or replace the entry '
            'with that name. Pattern is the load-bearing field: it groups '
            'variants of one movement so rotation reads as progress.',
        inputSchema: Schema.object(
          properties: {
            'name': Schema.string(description: 'Canonical name, Ukrainian'),
            'aliases': Schema.list(items: Schema.string()),
            'pattern': _patternSchema,
            'modality': Schema.string(
              enumValues: ['barbell', 'dumbbell', 'kettlebell', 'bodyweight', 'machine'],
            ),
            'category': Schema.string(
              description: 'Force-velocity emphasis; power and speed drive the '
                  'P3 bar-speed rule',
              enumValues: ['strength', 'power', 'speed', 'longevity'],
            ),
            'primary_muscles': Schema.list(items: Schema.string()),
            'secondary_muscles': Schema.list(items: Schema.string()),
            'notes': Schema.string(),
            'overwrite': Schema.bool(),
          },
          required: ['name', 'primary_muscles'],
        ),
      ),
      addExercise,
    );

    _tool(
      Tool(
        name: 'list_cycles',
        description: 'The cycle definitions in cycles.json — the plan side: '
            'which workouts a cycle holds and what each day is built around.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(properties: {
          'id': Schema.string(description: 'Full definition of one cycle'),
        }),
      ),
      listCycles,
    );

    _tool(
      Tool(
        name: 'generate_cycle',
        description: 'Expand a cycle onto the calendar, writing one session stub '
            'per template day. Existing files are skipped unless force is set; '
            'dry_run reports what it would write.',
        inputSchema: Schema.object(
          properties: {
            'cycle_id': Schema.string(),
            'start_date': _dateSchema('Overrides the cycle start date'),
            'force': Schema.bool(description: 'Overwrite existing files'),
            'dry_run': Schema.bool(),
          },
          required: ['cycle_id'],
        ),
      ),
      generateCycle,
    );

    _tool(
      Tool(
        name: 'analyze_session',
        description: 'One session derived: tonnage, sets by rep band, top sets '
            'against back-offs, work-to-clock density from the bracket '
            'timestamps, metcon round splits with pace decay and heart rate, and '
            'the muscles the work landed on.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(properties: {
          ..._selector,
          'mode': _weightingSchema,
        }),
      ),
      analyzeSession,
    );

    _tool(
      Tool(
        name: 'analyze_training',
        description: 'A stretch of sessions read as a block: rep-band '
            'distribution by set and by slot, movement-pattern frequency per '
            'cycle, strength tonnage beside conditioning time, metcon time '
            'domains, and alerts against the training principles (P3, P5, P8, '
            'P10). The safety metric the log exists for.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(properties: {
          'from': _dateSchema('Earliest date, inclusive'),
          'to': _dateSchema('Latest date, inclusive'),
          'cycle_length_days': Schema.int(description: 'Default 12'),
          'volume_share_threshold': Schema.num(
            description: 'Share of 7+ rep slots that raises the P5 alert; '
                'default 0.3',
          ),
          'include_load': Schema.bool(
            description: 'Per-session load series; default true',
          ),
        }),
      ),
      analyzeTraining,
    );

    _tool(
      Tool(
        name: 'exercise_progress',
        description: 'Best set over time for one lift, one track per rep band, '
            'with estimated 1RM and back-off volume reported separately. '
            'by_pattern widens it to every variant of the same movement pattern.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(
          properties: {
            'exercise': Schema.string(description: 'Canonical name or alias'),
            'by_pattern': Schema.bool(
              description: 'Compare variants of one pattern (P8)',
            ),
            'from': _dateSchema('Earliest date, inclusive'),
            'to': _dateSchema('Latest date, inclusive'),
          },
          required: ['exercise'],
        ),
      ),
      exerciseProgress,
    );

    _tool(
      Tool(
        name: 'muscle_activation',
        description: 'Muscle scores for one session or a whole range, normalized '
            'so the hottest muscle is 1.0, plus the coarse group rollup. The '
            'three weightings deliberately disagree: sets, reps or tonnage.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(properties: {
          ..._selector,
          'from': _dateSchema('Range mode: earliest date'),
          'to': _dateSchema('Range mode: latest date'),
          'mode': _weightingSchema,
        }),
      ),
      muscleActivation,
    );

    _tool(
      Tool(
        name: 'validate_archive',
        description: 'Check every session against the schema and against what '
            'the log means: malformed headers, overlapping blocks, splits that '
            'go backwards, exercises no catalogue entry resolves.',
        annotations: ToolAnnotations(readOnlyHint: true),
        inputSchema: Schema.object(properties: {
          'from': _dateSchema('Earliest date, inclusive'),
          'to': _dateSchema('Latest date, inclusive'),
        }),
      ),
      validateArchive,
    );
  }

  /// Failures come back as tool output rather than protocol errors, so the model
  /// reads what went wrong and corrects it instead of seeing a dead call.
  void _tool(
    Tool tool,
    Future<Object?> Function(Workspace, CallToolRequest) handler,
  ) {
    registerTool(tool, (request) async {
      try {
        return CallToolResult(
          content: [Content.text(text: jsonText(await handler(workspace, request)))],
        );
      } on ToolFailure catch (failure) {
        return _failure(failure.message);
      } catch (error) {
        return _failure('$error');
      }
    });
  }

  CallToolResult _failure(String message) => CallToolResult(
        isError: true,
        content: [Content.text(text: message)],
      );
}

/// Every tool that works on one session accepts the same three ways of naming it.
final Map<String, Schema> _selector = {
  'id': Schema.string(description: 'File name, e.g. 2026-08-10_F1.json'),
  'date': _dateSchema('The session date, when it names one file'),
  'cycle_day': Schema.string(description: 'A1…F2, to disambiguate a date'),
};

Schema _dateSchema(String description) =>
    Schema.string(description: '$description (YYYY-MM-DD)');

final _kindSchema = Schema.string(
  description: 'training, deload or retest — deloads and retests are the '
      'anchors progress is measured between (P10)',
  enumValues: ['training', 'deload', 'retest'],
);

final _patternSchema = Schema.string(
  description: 'Movement pattern — the grouping that makes variant rotation '
      'read as progress (P8)',
  enumValues: ['squat', 'hinge', 'press', 'pull', 'olympic', 'carry', 'core', 'grip'],
);

final _weightingSchema = Schema.string(
  description: 'How muscle work is weighted: set_count (default), rep_volume '
      'or tonnage',
  enumValues: ['set_count', 'rep_volume', 'tonnage'],
);
