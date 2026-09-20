import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:path_provider/path_provider.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'session_folder.dart';

const _lastFolderKey = 'workout_log.folder';

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
    // Write-then-rename: a crash mid-write leaves the previous session intact
    // rather than a truncated file.
    final temporary = File('${_file(id).path}.tmp');
    await temporary.writeAsString(contents, flush: true);
    await temporary.rename(_file(id).path);
  }

  @override
  Future<void> delete(String id) async {
    final file = _file(id);
    if (file.existsSync()) await file.delete();
  }

  File _file(String id) => File('${directory.path}/$id');
}

Future<SessionFolder> openDefaultFolder() async {
  final directory = await _defaultDirectory();
  return SessionFolder(
    storage: DirectoryStorage(directory),
    canChooseFolder: true,
    isBrowserCopy: false,
  );
}

Future<SessionFolder?> chooseFolder() async {
  final path = await FilePicker.platform.getDirectoryPath(
    dialogTitle: 'Choose session folder',
  );
  if (path == null) return null;
  await (await SharedPreferences.getInstance()).setString(_lastFolderKey, path);
  return SessionFolder(
    storage: DirectoryStorage(Directory(path)),
    canChooseFolder: true,
    isBrowserCopy: false,
  );
}

/// Desktop resolves the real folder (ADR-001); a sandboxed platform can only
/// ever use its own documents directory.
Future<Directory> _defaultDirectory() async {
  if (Platform.isIOS || Platform.isAndroid) {
    final documents = await getApplicationDocumentsDirectory();
    return Directory('${documents.path}/sessions')..createSync(recursive: true);
  }

  final fromEnvironment = Platform.environment['WORKOUTLOG_DATA'];
  if (fromEnvironment != null && fromEnvironment.isNotEmpty) {
    return Directory(_expandTilde(fromEnvironment));
  }

  final remembered =
      (await SharedPreferences.getInstance()).getString(_lastFolderKey);
  if (remembered != null && Directory(remembered).existsSync()) {
    return Directory(remembered);
  }

  final sibling = Directory('${Directory.current.path}/../data');
  if (sibling.existsSync()) return sibling.absolute;

  final documents = await getApplicationDocumentsDirectory();
  return Directory('${documents.path}/WorkoutLog');
}

String _expandTilde(String path) => path.startsWith('~/')
    ? '${Platform.environment['HOME']}${path.substring(1)}'
    : path;
