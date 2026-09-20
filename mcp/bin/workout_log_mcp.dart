import 'dart:io';

import 'package:dart_mcp/stdio.dart';
import 'package:workout_log_mcp/workout_log_mcp.dart';

/// Serves the log over MCP on stdio.
///
/// Only `dart:io` lives out here, never in the core (ADR-004), and nothing may
/// ever write to stdout: it carries the protocol. Diagnostics go to stderr.
const _usage = '''
usage: dart run workout_log_mcp [--repo <dir>] [--data <dir>]

  --repo <dir>  the repository holding exercises.json and cycles.json
                (default: the nearest ancestor of the working directory that has
                cycles.json)
  --data <dir>  the session folder (default: \$WORKOUTLOG_DATA, else <repo>/data)
''';

void main(List<String> argv) {
  String? repo;
  String? data;

  for (var i = 0; i < argv.length; i++) {
    String next(String flag) {
      if (i + 1 >= argv.length) _fail('missing value for $flag');
      return argv[++i];
    }

    switch (argv[i]) {
      case '--help' || '-h':
        stdout.writeln(_usage);
        return;
      case '--repo':
        repo = next('--repo');
      case '--data':
        data = next('--data');
      default:
        _fail('unknown argument "${argv[i]}"\n$_usage');
    }
  }

  final workspace = Workspace.open(
    root: repo,
    data: data,
    environment: Platform.environment,
  );
  if (!workspace.cyclesFile.existsSync()) {
    stderr.writeln(
      'warning: no cycles.json under ${workspace.root.path} — pass --repo',
    );
  }
  stderr.writeln('workout-log: sessions in ${workspace.label}');

  WorkoutLogServer(
    stdioChannel(input: stdin, output: stdout),
    workspace: workspace,
  );
}

Never _fail(String message) {
  stderr.writeln('error: $message');
  exit(2);
}
