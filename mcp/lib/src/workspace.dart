import 'dart:io';

import 'package:workout_log_core/workout_log_core.dart';

/// The session folder as a real directory (ADR-007's desktop case).
///
/// Only the byte-level plumbing lives here: every rule about what the bytes
/// mean stays in `workout_log_core` (ADR-004).
class DirectoryStorage implements SessionStorage {
  DirectoryStorage(this.directory);

  final Directory directory;

  @override
  String get label => directory.path;

  @override
  Future<List<String>> listIds() async {
    if (!directory.existsSync()) return const [];
    return directory
        .listSync()
        .whereType<File>()
        .map((f) => f.uri.pathSegments.last)
        .toList();
  }

  @override
  Future<String> read(String id) => _file(id).readAsString();

  @override
  Future<void> write(String id, String contents) async {
    await directory.create(recursive: true);
    // Write-then-rename, as the app does: a crash mid-write leaves the previous
    // session intact rather than a truncated file.
    final temporary = File('${_file(id).path}.tmp');
    await temporary.writeAsString(contents, flush: true);
    await temporary.rename(_file(id).path);
  }

  @override
  Future<void> delete(String id) async {
    final file = _file(id);
    if (file.existsSync()) await file.delete();
  }

  File _file(String id) {
    if (id.contains('/') || id.contains('..')) {
      throw ArgumentError.value(id, 'id', 'must be a bare file name');
    }
    return File('${directory.path}/$id');
  }
}

class WorkspaceException implements Exception {
  WorkspaceException(this.message);

  final String message;

  @override
  String toString() => message;
}

/// The repository the server works against: the session folder plus the two
/// hand-maintained reference files beside it.
class Workspace {
  Workspace({required this.root, required Directory data})
      : sessions = SessionStore(DirectoryStorage(data)),
        dataDirectory = data;

  /// Resolution order matches the app's (ADR-007): an explicit path, then
  /// `$WORKOUTLOG_DATA`, then `data/` under whichever ancestor holds cycles.json.
  factory Workspace.open({
    String? root,
    String? data,
    Map<String, String> environment = const {},
  }) {
    final repo = Directory(root ?? _findRoot()?.path ?? Directory.current.path);
    final dataPath = data ?? environment['WORKOUTLOG_DATA'];
    return Workspace(
      root: repo,
      data: Directory(dataPath ?? '${repo.path}/data'),
    );
  }

  static Directory? _findRoot() {
    var dir = Directory.current.absolute;
    while (true) {
      if (File('${dir.path}/cycles.json').existsSync()) return dir;
      final parent = dir.parent;
      if (parent.path == dir.path) return null;
      dir = parent;
    }
  }

  final Directory root;
  final Directory dataDirectory;
  final SessionStore sessions;

  String get label => dataDirectory.path;

  File get catalogueFile => File('${root.path}/exercises.json');

  File get cyclesFile => File('${root.path}/cycles.json');

  Catalogue loadCatalogue() =>
      SessionCoding.decodeCatalogue(_readReference(catalogueFile));

  CycleCatalogue loadCycles() =>
      CycleCatalogue.fromJson(jsonMap(_readReference(cyclesFile)));

  void saveCatalogue(Catalogue catalogue) =>
      _writeReference(catalogueFile, SessionCoding.encodeJson(catalogue.toJson()));

  void saveCycles(CycleCatalogue cycles) =>
      _writeReference(cyclesFile, SessionCoding.encodeJson(cycles.toJson()));

  /// Every session in the archive, newest last, with unreadable files reported
  /// rather than thrown: a corrupted file costs one session, never the archive.
  Future<LoadResult> loadAll({String? from, String? to}) async {
    final result = await sessions.loadAll();
    final kept = [
      for (final session in result.sessions)
        if (_inRange(session.date, from, to)) session,
    ]..sort((a, b) => a.date.compareTo(b.date));
    return LoadResult(kept, result.failures);
  }

  static bool _inRange(String date, String? from, String? to) =>
      (from == null || date.compareTo(from) >= 0) &&
      (to == null || date.compareTo(to) <= 0);

  String _readReference(File file) {
    if (!file.existsSync()) {
      throw WorkspaceException(
        'no ${file.uri.pathSegments.last} under ${root.path} — point the server '
        'at the repository with --repo',
      );
    }
    return file.readAsStringSync();
  }

  /// Writing a reference file also refreshes the copy bundled into the Flutter
  /// app, which is what `tool/sync_assets.dart` does by hand; without it the
  /// asset-sync test starts failing the moment this server edits a catalogue.
  void _writeReference(File file, String contents) {
    file.writeAsStringSync(contents);
    final bundled = File(
      '${root.path}/flutter/assets/data/${file.uri.pathSegments.last}',
    );
    if (bundled.parent.existsSync()) bundled.writeAsStringSync(contents);
  }
}
