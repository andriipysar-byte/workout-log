import 'dart:convert';
import 'dart:js_interop';

import 'package:file_picker/file_picker.dart';
import 'package:web/web.dart' as web;

import 'transfer.dart';

Future<List<TransferFile>> importSessions() async {
  final result = await FilePicker.platform.pickFiles(
    allowMultiple: true,
    type: FileType.custom,
    allowedExtensions: const ['json'],
  );
  if (result == null) return const [];
  return [
    for (final picked in result.files)
      if (picked.bytes != null)
        TransferFile(picked.name, utf8.decode(picked.bytes!)),
  ];
}

/// One download per session: the browser has no folder to write into, so an
/// anchor click per file is the only portable way out (ADR-007).
Future<String?> exportSessions(List<TransferFile> files) async {
  for (final file in files) {
    final blob = web.Blob(
      [file.contents.toJS].toJS,
      web.BlobPropertyBag(type: 'application/json'),
    );
    final url = web.URL.createObjectURL(blob);
    (web.document.createElement('a') as web.HTMLAnchorElement)
      ..href = url
      ..download = file.name
      ..click();
    web.URL.revokeObjectURL(url);
  }
  return 'downloads (${files.length} file(s))';
}
