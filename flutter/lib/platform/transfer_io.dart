import 'dart:convert';
import 'dart:io';

import 'package:file_picker/file_picker.dart';

import 'transfer.dart';

Future<List<TransferFile>> importSessions() async {
  final result = await FilePicker.platform.pickFiles(
    allowMultiple: true,
    type: FileType.custom,
    allowedExtensions: const ['json'],
    dialogTitle: 'Import session files',
  );
  if (result == null) return const [];
  return [
    for (final picked in result.files)
      if (picked.path != null)
        TransferFile(picked.name, File(picked.path!).readAsStringSync())
      else if (picked.bytes != null)
        TransferFile(picked.name, utf8.decode(picked.bytes!)),
  ];
}

Future<String?> exportSessions(List<TransferFile> files) async {
  final path = await FilePicker.platform.getDirectoryPath(
    dialogTitle: 'Export sessions to folder',
  );
  if (path == null) return null;
  for (final file in files) {
    File('$path/${file.name}').writeAsStringSync(file.contents);
  }
  return '$path (${files.length} file(s))';
}
