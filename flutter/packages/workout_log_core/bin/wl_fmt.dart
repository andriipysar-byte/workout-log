import 'dart:io';

import 'package:workout_log_core/workout_log_core.dart';

/// Rewrite every session file in canonical form: sorted keys, two-space indent,
/// no null-valued keys, integral weights without a `.0`, trailing newline.
///
/// Decoding before encoding means this cannot change what a file *means* — only
/// how it is spelled. Run it after hand-editing a file so the next save from the
/// app produces no incidental diff.
///
/// Usage: dart run wl_fmt [--check] [<dir>]
void main(List<String> argv) {
  final checkOnly = argv.contains('--check');
  final positional = argv.where((a) => !a.startsWith('--')).toList();
  final dir = Directory(positional.isEmpty ? '../../../data' : positional.single);

  if (!dir.existsSync()) {
    stderr.writeln('error: no such directory: ${dir.path}');
    exit(2);
  }

  var changed = 0;
  final files = dir.listSync().whereType<File>().where(
        (f) => f.path.endsWith('.json'),
      );
  for (final file in files) {
    final name = file.uri.pathSegments.last;
    final original = file.readAsStringSync();
    final canonical = SessionCoding.encode(SessionCoding.decode(original));
    if (canonical == original) continue;
    changed++;
    if (checkOnly) {
      stdout.writeln('would reformat $name');
      continue;
    }
    file.writeAsStringSync(canonical);
    stdout.writeln('reformatted $name');
  }

  stdout.writeln(changed == 0 ? 'all files already canonical' : '$changed file(s)');
  if (checkOnly && changed > 0) exit(1);
}
