import 'dart:io';

import 'package:flutter_test/flutter_test.dart';

import '../tool/sync_assets.dart' show bundledFiles;

/// The bundled copies exist only because mobile and web cannot read the repo
/// folder; without this test the duplication rots silently.
void main() {
  for (final name in bundledFiles) {
    test('assets/data/$name matches the repo root', () {
      final root = File('../$name');
      final bundled = File('assets/data/$name');
      expect(bundled.existsSync(), isTrue,
          reason: 'run: dart run tool/sync_assets.dart');
      expect(bundled.readAsStringSync(), root.readAsStringSync(),
          reason: 'run: dart run tool/sync_assets.dart');
    });
  }
}
