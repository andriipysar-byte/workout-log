import 'dart:io';

import 'package:workout_log_core/workout_log_core.dart';

/// Tests read the real repository files: they are the source of truth (ADR-001),
/// so a fixture copy would only test the copy.
final Directory repoRoot = _findRepoRoot();

Directory _findRepoRoot() {
  var dir = Directory.current.absolute;
  while (true) {
    if (File('${dir.path}/cycles.json').existsSync()) return dir;
    final parent = dir.parent;
    if (parent.path == dir.path) {
      throw StateError('repo root not found from ${Directory.current.path}');
    }
    dir = parent;
  }
}

Directory get dataDir => Directory('${repoRoot.path}/data');

List<File> get sessionFiles => dataDir
    .listSync()
    .whereType<File>()
    .where((f) => f.path.endsWith('.json'))
    .toList()
  ..sort((a, b) => a.path.compareTo(b.path));

Catalogue loadCatalogue() => SessionCoding.decodeCatalogue(
      File('${repoRoot.path}/exercises.json').readAsStringSync(),
    );

CycleCatalogue loadCycles() => CycleCatalogue.fromJson(
      jsonMap(File('${repoRoot.path}/cycles.json').readAsStringSync()),
    );

String loadMapTemplate() =>
    File('${repoRoot.path}/flutter/assets/muscle-map.svg').readAsStringSync();
