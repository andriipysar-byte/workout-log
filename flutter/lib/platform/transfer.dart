import 'transfer_io.dart'
    if (dart.library.js_interop) 'transfer_web.dart' as impl;

/// A session file moving between the app and the world outside it.
class TransferFile {
  TransferFile(this.name, this.contents);

  final String name;
  final String contents;
}

/// Picks `.json` session files to copy into the archive. Empty if cancelled.
Future<List<TransferFile>> importSessions() => impl.importSessions();

/// Writes the archive back out: a folder on desktop and mobile, downloads in the
/// browser. Returns a description of where they went, or null if cancelled.
Future<String?> exportSessions(List<TransferFile> files) =>
    impl.exportSessions(files);
