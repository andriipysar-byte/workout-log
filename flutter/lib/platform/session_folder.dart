import 'package:workout_log_core/workout_log_core.dart';

import 'session_folder_io.dart'
    if (dart.library.js_interop) 'session_folder_web.dart' as impl;

/// The two hand-maintained reference files, `exercises.json` and `cycles.json`.
///
/// They sit beside the session folder rather than inside it, and unlike sessions
/// they ship with the app as a fallback — so `read` returning null means "use the
/// bundled copy", not "missing".
abstract interface class ReferenceStore {
  bool get canWrite;

  String get label;

  Future<String?> read(String name);

  Future<void> write(String name, String contents);
}

/// Web, and anywhere else the reference files cannot be written back.
class BundledReferenceStore implements ReferenceStore {
  const BundledReferenceStore();

  @override
  bool get canWrite => false;

  @override
  String get label => 'bundled (read-only)';

  @override
  Future<String?> read(String name) async => null;

  @override
  Future<void> write(String name, String contents) async =>
      throw UnsupportedError('the bundled catalogue cannot be written');
}

/// Where a platform's sessions live, and whether the user may point it elsewhere.
class SessionFolder {
  SessionFolder({
    required this.storage,
    required this.canChooseFolder,
    required this.isBrowserCopy,
    ReferenceStore? references,
  }) : references = references ?? const BundledReferenceStore();

  final SessionStorage storage;

  final ReferenceStore references;

  /// Desktop can be aimed at any directory; a sandbox or the browser cannot.
  final bool canChooseFolder;

  /// True when the archive is a working copy rather than the canonical folder
  /// (ADR-007) — the UI says so, and offers import/export instead.
  final bool isBrowserCopy;
}

/// Opens the platform's default location: `$WORKOUTLOG_DATA`, the last folder
/// the user chose, or `<cwd>/../data` on desktop; the documents directory on
/// mobile; an in-memory copy in the browser.
Future<SessionFolder> openDefaultFolder() => impl.openDefaultFolder();

/// Desktop/mobile only — picks a new directory, or null if the user cancelled.
Future<SessionFolder?> chooseFolder() => impl.chooseFolder();
