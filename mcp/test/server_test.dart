import 'package:test/test.dart';

import 'harness.dart';

Map<String, dynamic> _session(
  String date,
  String cycleDay, {
  List<Map<String, dynamic>> blocks = const [],
  String? startTime,
}) =>
    {
      'date': date,
      'cycle_day': cycleDay,
      'kind': 'training',
      'start_time': ?startTime,
      'blocks': blocks,
    };

Map<String, dynamic> _strength(
  String exercise,
  List<Map<String, dynamic>> sets, {
  String? endTime,
}) =>
    {
      'type': 'strength',
      'exercise': exercise,
      'sets': sets,
      'end_time': ?endTime,
    };

void main() {
  late TestRepo repo;
  late TestClient mcp;

  setUp(() {
    repo = TestRepo();
    mcp = TestClient(repo);
  });

  test('every tool is advertised with a schema', () async {
    final tools = await mcp.tools();

    expect(
      tools.map((t) => t.name),
      containsAll([
        'list_sessions',
        'read_session',
        'create_session',
        'write_session',
        'log_sets',
        'delete_session',
        'parse_notation',
        'list_exercises',
        'add_exercise',
        'list_cycles',
        'generate_cycle',
        'analyze_session',
        'analyze_training',
        'exercise_progress',
        'muscle_activation',
        'validate_archive',
      ]),
    );
    for (final tool in tools) {
      expect(tool.description, isNotNull, reason: tool.name);
      expect(tool.inputSchema.properties, isNotNull, reason: tool.name);
    }
  });

  group('create', () {
    test('a session starts from the cycle template for that day', () async {
      final result = await mcp.call('create_session', {
        'date': '2026-09-22',
        'cycle_day': 'A1',
        'start_time': '08:00',
      });

      expect(result['id'], '2026-09-22_A1.json');
      expect(result['from_cycle'], 'hybrid-8');
      final blocks = result['session']['blocks'] as List;
      expect(blocks, isNotEmpty);
      expect(blocks.first['type'], 'cardio');
      expect(repo.sessionIds, ['2026-09-22_A1.json']);
      expect(repo.readSession('2026-09-22_A1.json')['start_time'], '08:00');
    });

    test('a day no cycle defines is created empty', () async {
      final result = await mcp.call('create_session', {
        'date': '2026-09-22',
        'cycle_day': 'Z1',
      });

      expect(result['session']['blocks'], isEmpty);
      expect((result['issues'] as List).single['path'], 'blocks');
    });

    test('an existing file is never silently replaced', () async {
      await mcp.call('create_session', {'date': '2026-09-22', 'cycle_day': 'A1'});

      expect(
        await mcp.error('create_session', {
          'date': '2026-09-22',
          'cycle_day': 'A1',
        }),
        contains('already exists'),
      );

      final forced = await mcp.call('create_session', {
        'date': '2026-09-22',
        'cycle_day': 'A1',
        'overwrite': true,
      });
      expect(forced['id'], '2026-09-22_A1.json');
    });

    test('a malformed date is refused before anything is written', () async {
      expect(
        await mcp.error('create_session', {
          'date': '22.09.2026',
          'cycle_day': 'A1',
        }),
        contains('ISO date'),
      );
      expect(repo.sessionIds, isEmpty);
    });
  });

  group('edit', () {
    test('log_sets fills a slot from the notation', () async {
      repo.writeSession(
        '2026-09-22_C2.json',
        _session('2026-09-22', 'C2', blocks: [
          _strength('присід фронтальний', const []),
        ]),
      );

      final result = await mcp.call('log_sets', {
        'id': '2026-09-22_C2.json',
        'exercise': 'присід фронтальний',
        'notation': '6 × [70, 80, 90, 100] + 6 × [70]',
      });

      final sets = result['sets'] as List;
      expect(sets, hasLength(5));
      expect(sets.first, {'reps': 6, 'weight_kg': 70});
      expect(sets.last, {'is_backoff': true, 'reps': 6, 'weight_kg': 70});

      final saved = repo.readSession('2026-09-22_C2.json');
      expect((saved['blocks'] as List).first['sets'], hasLength(5));
      // Files stay canonical: sorted keys, no `.0` on integral weights.
      expect(repo.sessionFile('2026-09-22_C2.json').readAsStringSync(),
          contains('"weight_kg": 70\n'));
    });

    test('log_sets names the slot when an exercise appears twice', () async {
      repo.writeSession(
        '2026-09-22_C2.json',
        _session('2026-09-22', 'C2', blocks: [
          _strength('вис', const []),
          _strength('вис', const []),
        ]),
      );

      expect(
        await mcp.error('log_sets', {
          'id': '2026-09-22_C2.json',
          'exercise': 'вис',
          'notation': '4 × [54c]',
        }),
        contains('blocks 0, 1'),
      );

      final result = await mcp.call('log_sets', {
        'id': '2026-09-22_C2.json',
        'block_index': 1,
        'notation': '4 × [54c]',
      });
      expect((result['sets'] as List).first, {'duration_sec': 54});
    });

    test('an alias resolves to the slot it names', () async {
      repo.writeSession(
        '2026-09-22_C2.json',
        _session('2026-09-22', 'C2', blocks: [
          _strength('присід фронтальний', const []),
        ]),
      );

      final result = await mcp.call('log_sets', {
        'date': '2026-09-22',
        'exercise': 'фр. присід',
        'notation': '3 × [110]',
      });

      expect(result['exercise'], 'присід фронтальний');
    });

    test('write_session renames the file when the header moves', () async {
      repo.writeSession('2026-09-22_C2.json', _session('2026-09-22', 'C2'));
      final read = await mcp.call('read_session', {'id': '2026-09-22_C2.json'});
      final session = Map<String, dynamic>.from(read['session'] as Map)
        ..['date'] = '2026-09-23';

      final result = await mcp.call('write_session', {
        'id': '2026-09-22_C2.json',
        'session': session,
      });

      expect(result['id'], '2026-09-23_C2.json');
      expect(result['renamed_from'], '2026-09-22_C2.json');
      expect(repo.sessionIds, ['2026-09-23_C2.json']);
    });

    test('a document that would corrupt the archive is refused', () async {
      repo.writeSession('2026-09-22_C2.json', _session('2026-09-22', 'C2'));

      final message = await mcp.error('write_session', {
        'id': '2026-09-22_C2.json',
        'session': _session('2026-09-22', 'C2', blocks: [
          _strength('   ', const []),
        ]),
      });

      expect(message, contains('blocks[0].exercise'));
      expect(repo.readSession('2026-09-22_C2.json')['blocks'], isEmpty);
    });

    test('warnings travel with a write instead of blocking it', () async {
      repo.writeSession('2026-09-22_C2.json', _session('2026-09-22', 'C2'));

      final result = await mcp.call('write_session', {
        'id': '2026-09-22_C2.json',
        'session': _session('2026-09-22', 'C2', blocks: [
          _strength('вправа якої немає', [
            {'reps': 6, 'weight_kg': 40},
          ]),
        ]),
      });

      expect(
        (result['issues'] as List).single['message'],
        contains('вправа якої немає'),
      );
    });

    test('deleting needs confirmation and reports what it removed', () async {
      repo.writeSession('2026-09-22_C2.json', _session('2026-09-22', 'C2'));

      expect(
        await mcp.error('delete_session', {'id': '2026-09-22_C2.json'}),
        contains('confirm'),
      );
      expect(repo.sessionIds, hasLength(1));

      final result = await mcp.call('delete_session', {
        'id': '2026-09-22_C2.json',
        'confirm': true,
      });
      expect(result['deleted'], '2026-09-22_C2.json');
      expect(repo.sessionIds, isEmpty);
    });

    test('parse_notation explains a line without writing anything', () async {
      final result = await mcp.call('parse_notation', {
        'notation': '5+5+4+3+3 (20)',
      });

      expect((result['sets'] as List).single, {
        'cluster': [5, 5, 4, 3, 3],
        'total_reps': 20,
      });
      expect(repo.sessionIds, isEmpty);
    });
  });

  group('analyse', () {
    setUp(() {
      repo.writeSession(
        '2026-09-22_C2.json',
        _session('2026-09-22', 'C2', startTime: '08:00', blocks: [
          _strength('присід фронтальний', [
            {'reps': 6, 'weight_kg': 90},
            {'reps': 3, 'weight_kg': 110},
            {'reps': 6, 'weight_kg': 70, 'is_backoff': true},
          ], endTime: '08:40'),
        ]),
      );
      repo.writeSession(
        '2026-10-04_C2.json',
        _session('2026-10-04', 'C2', blocks: [
          _strength('присід фронтальний', [
            {'reps': 6, 'weight_kg': 95},
          ]),
          _strength('ривок', [
            {'reps': 6, 'weight_kg': 60},
          ]),
        ]),
      );
    });

    test('analyze_session derives tonnage, bands, density and muscles', () async {
      final result = await mcp.call('analyze_session', {
        'id': '2026-09-22_C2.json',
      });

      final strength = result['metrics']['strength'] as Map;
      expect(strength['tonnage_kg'], 6 * 90 + 3 * 110 + 6 * 70);
      expect(strength['backoff_sets'], 1);
      expect(strength['rep_band_sets'], {'heavy': 1, 'base': 2});
      expect(result['metrics']['density']['work_min'], 40);
      expect(result['muscles']['dominant_group'], 'legs');
    });

    test('analyze_training reports bands, patterns and alerts', () async {
      final result = await mcp.call('analyze_training', {});

      expect(result['sessions'], 2);
      expect(result['rep_bands']['slots'], containsPair('base', 3));
      final patterns = (result['patterns'] as List)
          .map((p) => p['pattern'] as String)
          .toList();
      expect(patterns, containsAll(['squat', 'olympic']));
      // Снатч at six reps is the P3 signal the log exists to catch.
      expect(
        (result['alerts'] as List).map((a) => a['principle']),
        contains('P3'),
      );
      expect(result['load'], hasLength(2));
    });

    test('analyze_training can be narrowed to a range', () async {
      final result = await mcp.call('analyze_training', {
        'from': '2026-10-01',
        'include_load': false,
      });

      expect(result['sessions'], 1);
      expect(result.containsKey('load'), isFalse);
      expect(await mcp.error('analyze_training', {'from': '2027-01-01'}),
          contains('no sessions'));
    });

    test('exercise_progress splits the tracks and estimates 1RM', () async {
      final result = await mcp.call('exercise_progress', {
        'exercise': 'фр. присід',
      });

      expect(result['resolved'], 'присід фронтальний');
      final base = (result['series'] as List)
          .firstWhere((s) => s['rep_band'] == 'base');
      expect((base['points'] as List).map((p) => p['weight_kg']), [90, 95]);
      expect(base['points'][0]['backoff_sets'], 1);
      expect(base['points'][0]['estimated_1rm_kg'], greaterThan(90));
      expect(base['trend_1rm_kg'], greaterThan(0));
    });

    test('exercise_progress says so when a name logs nothing', () async {
      final result = await mcp.call('exercise_progress', {
        'exercise': 'станова тяга',
      });

      expect(result['series'], isEmpty);
      expect(result['note'], contains('list_exercises'));
    });

    test('muscle_activation covers one session or the whole range', () async {
      final one = await mcp.call('muscle_activation', {
        'date': '2026-09-22',
        'mode': 'tonnage',
      });
      expect(one['weighting'], 'tonnage');
      expect((one['muscles'] as Map)['quads'], 1);

      final all = await mcp.call('muscle_activation', {});
      expect(all['sessions'], 2);
      expect(all['groups'], isNotEmpty);
    });

    test('validate_archive counts what it found', () async {
      repo.sessionFile('broken.json').writeAsStringSync('{ not json');

      final result = await mcp.call('validate_archive', {});

      expect(result['checked'], 2);
      expect((result['unreadable'] as List).single['id'], 'broken.json');
    });

    test('list_sessions summarises and filters', () async {
      final all = await mcp.call('list_sessions', {});
      expect(all['count'], 2);
      expect(all['sessions'][0]['tonnage_kg'], 1290);

      final filtered = await mcp.call('list_sessions', {'exercise': 'ривок'});
      expect(filtered['count'], 1);
      expect(filtered['sessions'][0]['id'], '2026-10-04_C2.json');
    });
  });

  group('plan', () {
    test('list_exercises filters and carries the muscle vocabulary', () async {
      final result = await mcp.call('list_exercises', {'pattern': 'squat'});

      expect(
        (result['exercises'] as List).map((e) => e['name']),
        contains('присід фронтальний'),
      );
      expect(result['known_muscles'], contains('quads'));
    });

    test('add_exercise appends without disturbing the rest of the file', () async {
      final before = repo.workspace.loadCatalogue();

      final result = await mcp.call('add_exercise', {
        'name': 'жим гантелей стоячи',
        'aliases': ['жим гантелей'],
        'pattern': 'press',
        'modality': 'dumbbell',
        'primary_muscles': ['front_delts'],
        'secondary_muscles': ['triceps'],
      });

      expect(result['replaced'], false);
      final after = repo.workspace.loadCatalogue();
      expect(after.exercises, hasLength(before.exercises.length + 1));
      expect(after.comment, before.comment);
      expect(after.resolve('жим гантелей')?.name, 'жим гантелей стоячи');
    });

    test('add_exercise flags a muscle name outside the vocabulary', () async {
      final result = await mcp.call('add_exercise', {
        'name': 'нова вправа',
        'primary_muscles': ['delts'],
      });

      expect(result['new_muscle_names']['names'], ['delts']);
    });

    test('add_exercise will not shadow an entry unless told to', () async {
      expect(
        await mcp.error('add_exercise', {
          'name': 'присід фронтальний',
          'primary_muscles': ['quads'],
        }),
        contains('already in the catalogue'),
      );
    });

    test('list_cycles shows the plan, one line per day', () async {
      final result = await mcp.call('list_cycles', {});

      final cycle = (result['cycles'] as List).single;
      expect(cycle['id'], 'hybrid-8');
      expect(cycle['days'], hasLength(8));

      final full = await mcp.call('list_cycles', {'id': 'hybrid-8'});
      expect(full['cycle']['sessions'], hasLength(8));
    });

    test('generate_cycle writes the stubs, and dry_run writes nothing', () async {
      final dry = await mcp.call('generate_cycle', {
        'cycle_id': 'hybrid-8',
        'start_date': '2026-10-06',
        'dry_run': true,
      });

      expect((dry['sessions'] as List).first['status'], 'would write');
      expect(repo.sessionIds, isEmpty);

      final written = await mcp.call('generate_cycle', {
        'cycle_id': 'hybrid-8',
        'start_date': '2026-10-06',
      });
      expect(written['sessions'], hasLength(8));
      expect(repo.sessionIds, hasLength(8));

      final again = await mcp.call('generate_cycle', {
        'cycle_id': 'hybrid-8',
        'start_date': '2026-10-06',
      });
      expect(
        (again['sessions'] as List).first['status'],
        startsWith('skipped'),
      );
    });

    test('an unknown cycle says what there is instead', () async {
      expect(
        await mcp.error('generate_cycle', {'cycle_id': 'hybrid-12'}),
        contains('hybrid-8'),
      );
    });
  });
}
