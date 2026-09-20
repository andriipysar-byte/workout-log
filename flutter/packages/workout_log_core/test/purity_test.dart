import 'dart:io';

import 'package:test/test.dart';

/// ADR-004, made mechanical: the Swift core carried a "ZERO SwiftUI/AppKit"
/// comment, which a comment cannot enforce. This can.
void main() {
  test('lib/ depends on neither Flutter nor dart:io', () {
    final offenders = <String>[];
    for (final entity in Directory('lib').listSync(recursive: true)) {
      if (entity is! File || !entity.path.endsWith('.dart')) continue;
      final source = entity.readAsStringSync();
      for (final banned in const ['dart:io', 'dart:ui', 'package:flutter/']) {
        if (source.contains("import '$banned") ||
            source.contains('import "$banned')) {
          offenders.add('${entity.path} imports $banned');
        }
      }
    }
    expect(offenders, isEmpty);
  });
}
