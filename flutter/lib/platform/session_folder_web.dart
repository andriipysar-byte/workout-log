import 'package:workout_log_core/workout_log_core.dart';

import 'session_folder.dart';

/// ADR-007: a browser has no folder to be the source of truth, so the archive is
/// an in-memory working copy and the user's folder stays canonical via the
/// import and export actions.
Future<SessionFolder> openDefaultFolder() async => SessionFolder(
      storage: MemoryStorage(label: 'browser working copy'),
      canChooseFolder: false,
      isBrowserCopy: true,
    );

Future<SessionFolder?> chooseFolder() async => null;
