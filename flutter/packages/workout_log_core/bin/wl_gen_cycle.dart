import 'dart:io';

import 'package:workout_log_core/workout_log_core.dart';

/// Generate dated session-stub JSON files from a cycle definition in cycles.json.
///
/// Only `dart:io` lives here, never in `lib/`, so the core itself stays
/// platform-free (enforced by test/purity_test.dart).
const _usage = 'usage: dart run wl_gen_cycle '
    '[--cycle <id>] [--out <dir>] [--repo <dir>] [--force]';

void main(List<String> argv) {
  var cycleId = 'hybrid-8';
  var outDirName = 'data';
  var repo = _repoRoot();
  var force = false;

  for (var i = 0; i < argv.length; i++) {
    String next(String flag) {
      if (i + 1 >= argv.length) _fail('missing value for $flag');
      return argv[++i];
    }

    switch (argv[i]) {
      case '--force':
        force = true;
      case '--help' || '-h':
        stdout.writeln(_usage);
        return;
      case '--cycle':
        cycleId = next('--cycle');
      case '--out':
        outDirName = next('--out');
      case '--repo':
        repo = next('--repo');
      default:
        _fail('unknown argument "${argv[i]}"\n$_usage');
    }
  }

  final cyclesFile = File('$repo/cycles.json');
  if (!cyclesFile.existsSync()) {
    _fail('no cycles.json under $repo (pass --repo <dir>)');
  }

  final catalogue = CycleCatalogue.fromJson(
    jsonMap(cyclesFile.readAsStringSync()),
  );
  final cycle = catalogue.byId(cycleId);
  if (cycle == null) _fail('cycle "$cycleId" not found in ${cyclesFile.path}');

  final outDir = Directory('$repo/$outDirName')..createSync(recursive: true);

  for (final session in CycleGenerator.generate(cycle)) {
    final name = SessionStore.idFor(session);
    final file = File('${outDir.path}/$name');
    if (file.existsSync() && !force) {
      stdout.writeln('skip (exists): $name  — pass --force to overwrite');
      continue;
    }
    file.writeAsStringSync(SessionCoding.encode(session));
    stdout.writeln('wrote $name');
  }
}

/// Walks up from the working directory to whichever ancestor holds cycles.json.
String _repoRoot() {
  var dir = Directory.current.absolute;
  while (true) {
    if (File('${dir.path}/cycles.json').existsSync()) return dir.path;
    final parent = dir.parent;
    if (parent.path == dir.path) return Directory.current.path;
    dir = parent;
  }
}

Never _fail(String message) {
  stderr.writeln('error: $message');
  exit(2);
}
