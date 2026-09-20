import 'package:workout_log_core/workout_log_core.dart';

import 'session_folder_io.dart'
    if (dart.library.js_interop) 'session_folder_web.dart' as impl;

/// Where a platform's sessions live, and whether the user may point it elsewhere.
class SessionFolder {
  SessionFolder({
    required this.storage,
    required this.canChooseFolder,
    required this.isBrowserCopy,
  });

  final SessionStorage storage;

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
