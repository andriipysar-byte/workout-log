import 'dart:io';

/// Copies the repo-root reference data into `assets/data/`.
///
/// Mobile and web have no access to the repo folder, so the catalogue and the
/// cycle definitions have to ship inside the bundle. Run after editing either
/// file; `test/asset_sync_test.dart` fails when the copies drift.
///
/// Usage: dart run tool/sync_assets.dart [--check]
const bundledFiles = ['exercises.json', 'cycles.json'];

void main(List<String> argv) {
  final checkOnly = argv.contains('--check');
  final root = Directory.current.parent;
  final target = Directory('${Directory.current.path}/assets/data')
    ..createSync(recursive: true);

  var stale = 0;
  for (final name in bundledFiles) {
    final source = File('${root.path}/$name');
    final copy = File('${target.path}/$name');
    final wanted = source.readAsStringSync();
    if (copy.existsSync() && copy.readAsStringSync() == wanted) {
      stdout.writeln('up to date: assets/data/$name');
      continue;
    }
    stale++;
    if (checkOnly) {
      stderr.writeln('stale: assets/data/$name');
      continue;
    }
    copy.writeAsStringSync(wanted);
    stdout.writeln('synced: assets/data/$name');
  }

  if (checkOnly && stale > 0) {
    stderr.writeln('run: dart run tool/sync_assets.dart');
    exit(1);
  }
}
