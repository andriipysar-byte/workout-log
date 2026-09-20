import 'dart:io';

import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

/// A `dart:io` backend used only to prove the store works over a real folder;
/// the app's desktop backend is the same idea in `flutter/lib/platform/`.
class _TempDirStorage implements SessionStorage {
  _TempDirStorage(this.dir);

  final Directory dir;

  @override
  String get label => dir.path;

  @override
  Future<List<String>> listIds() async => dir
      .listSync()
      .whereType<File>()
      .map((f) => f.uri.pathSegments.last)
      .toList();

  @override
  Future<String> read(String id) async =>
      File('${dir.path}/$id').readAsString();

  @override
  Future<void> write(String id, String contents) async =>
      File('${dir.path}/$id').writeAsString(contents);

  @override
  Future<void> delete(String id) async {
    final file = File('${dir.path}/$id');
    if (file.existsSync()) file.deleteSync();
  }
}

void main() {
  Session sample() => Session(
        date: '2026-07-21',
        cycleDay: 'A1',
        blocks: [
          CardioBlock(machine: 'гребля', durationMin: 10),
          StrengthBlock(
            exercise: 'присід фронтальний',
            sets: [WorkSet(reps: 6, weightKg: 70)],
          ),
        ],
      );

  test('the id follows YYYY-MM-DD_<cycleDay>.json', () {
    expect(SessionStore.idFor(sample()), '2026-07-21_A1.json');
  });

  test('save then reload is identical', () async {
    final store = SessionStore(MemoryStorage());
    final id = await store.save(sample());
    expect(await store.load(id), sample());
  });

  test('listing is sorted and ignores non-JSON files', () async {
    final store = SessionStore(MemoryStorage(seed: {
      'b.json': '{}',
      'a.json': '{}',
      'notes.txt': 'x',
    }));
    expect(await store.listIds(), ['a.json', 'b.json']);
  });

  test('a corrupted file costs one session, never the archive', () async {
    final store = SessionStore(MemoryStorage(seed: {
      'good.json': SessionCoding.encode(sample()),
      'bad.json': '{ not json',
    }));
    final result = await store.loadAll();
    expect(result.sessions, hasLength(1));
    expect(result.failures.single.id, 'bad.json');
  });

  test('saving after a header edit removes the file it moved from', () async {
    final storage = MemoryStorage();
    final store = SessionStore(storage);
    final original = await store.save(sample());

    final moved = sample()..date = '2026-07-22';
    final renamed = await store.save(moved, previousId: original);

    expect(renamed, '2026-07-22_A1.json');
    expect(await store.listIds(), [renamed]);
  });

  test('saving without a header edit keeps the single file', () async {
    final store = SessionStore(MemoryStorage());
    final id = await store.save(sample());
    expect(await store.save(sample(), previousId: id), id);
    expect(await store.listIds(), [id]);
  });

  test('delete removes just that session', () async {
    final store = SessionStore(MemoryStorage());
    final id = await store.save(sample());
    await store.save(sample()..cycleDay = 'A2');
    await store.delete(id);
    expect(await store.listIds(), ['2026-07-21_A2.json']);
  });

  test('exists reports collisions before a create overwrites one', () async {
    final store = SessionStore(MemoryStorage());
    expect(await store.exists('2026-07-21_A1.json'), isFalse);
    await store.save(sample());
    expect(await store.exists('2026-07-21_A1.json'), isTrue);
  });

  test('round-trips a real session over a real directory', () async {
    final dir = Directory.systemTemp.createTempSync('wl-store-test');
    addTearDown(() => dir.deleteSync(recursive: true));

    final source = sessionFiles.first;
    final loaded = SessionCoding.decode(source.readAsStringSync());
    final store = SessionStore(_TempDirStorage(dir));
    final id = await store.save(loaded);

    expect(id, '${loaded.date}_${loaded.cycleDay}.json');
    expect(await store.load(id), loaded);
  });
}
