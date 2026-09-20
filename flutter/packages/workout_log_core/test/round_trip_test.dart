import 'dart:convert';

import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

void main() {
  test('data/ holds at least one session', () {
    expect(sessionFiles, isNotEmpty);
  });

  group('round-trip', () {
    for (final file in sessionFiles) {
      final name = file.uri.pathSegments.last;
      test(name, () {
        final first = SessionCoding.decode(file.readAsStringSync());
        final encoded = SessionCoding.encode(first);
        final second = SessionCoding.decode(encoded);
        expect(second, first, reason: 'model is stable across a round-trip');
        expect(
          SessionCoding.encode(second),
          encoded,
          reason: 'the encoder is idempotent',
        );
      });
    }
  });

  test('every session file decodes through the store', () async {
    final storage = MemoryStorage(seed: {
      for (final f in sessionFiles)
        f.uri.pathSegments.last: f.readAsStringSync(),
    });
    final result = await SessionStore(storage).loadAll();
    expect(result.failures, isEmpty);
    expect(result.sessions, hasLength(sessionFiles.length));
  });

  test('integral doubles encode without a .0 suffix', () {
    final session = Session(
      date: '2026-01-01',
      cycleDay: 'A1',
      bodyweightKg: 82,
      blocks: [
        StrengthBlock(
          exercise: 'присід фронтальний',
          sets: [WorkSet(reps: 6, weightKg: 70), WorkSet(weightKg: 47.5)],
        ),
      ],
    );
    final encoded = SessionCoding.encode(session);
    expect(encoded, contains('"weight_kg": 70\n'));
    expect(encoded, contains('"weight_kg": 47.5'));
    expect(encoded, contains('"bodyweight_kg": 82,'));
    expect(encoded, isNot(contains('70.0')));
    expect(encoded, isNot(contains('82.0')));
  });

  test('keys are sorted at every depth and the file ends with a newline', () {
    final encoded = SessionCoding.encode(
      Session(
        date: '2026-01-01',
        cycleDay: 'A1',
        startTime: '7:45',
        notes: 'n',
        blocks: [CardioBlock(machine: 'гребля', durationMin: 10, endTime: '8:00')],
      ),
    );
    expect(encoded.endsWith('}\n'), isTrue);

    final top = jsonDecode(encoded) as Map<String, dynamic>;
    expect(top.keys.toList(), equals(top.keys.toList()..sort()));
    final block = (top['blocks'] as List).first as Map<String, dynamic>;
    expect(block.keys.toList(), equals(block.keys.toList()..sort()));
  });

  test('optional fields are omitted, never written as null', () {
    final encoded = SessionCoding.encode(
      Session(date: '2026-01-01', cycleDay: 'A1', blocks: [CooldownBlock()]),
    );
    expect(encoded, isNot(contains('null')));
    expect(encoded, isNot(contains('start_time')));
    expect(encoded, isNot(contains('bodyweight_kg')));
  });

  test('an unknown block type is rejected rather than silently dropped', () {
    expect(
      () => SessionCoding.decode(
        '{"date":"2026-01-01","cycle_day":"A1","blocks":[{"type":"yoga"}]}',
      ),
      throwsFormatException,
    );
  });

  test('kind defaults to training when absent', () {
    final session = SessionCoding.decode(
      '{"date":"2026-01-01","cycle_day":"A1","blocks":[]}',
    );
    expect(session.kind, Kind.training);
  });
}
