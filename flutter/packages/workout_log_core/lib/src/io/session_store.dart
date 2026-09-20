import '../coding.dart';
import '../models/session.dart';
import 'session_storage.dart';

class LoadFailure {
  LoadFailure(this.id, this.error);

  final String id;
  final Object error;
}

class LoadResult {
  LoadResult(this.sessions, this.failures);

  final List<Session> sessions;
  final List<LoadFailure> failures;
}

/// Files are the source of truth (ADR-001). Filename: `YYYY-MM-DD_<cycleDay>.json`.
class SessionStore {
  SessionStore(this.storage);

  final SessionStorage storage;

  /// Sorting and the `.json` filter live here rather than in each backend, so
  /// every platform presents the archive in the same order.
  Future<List<String>> listIds() async {
    final ids = (await storage.listIds())
        .where((id) => id.endsWith('.json'))
        .toList()
      ..sort();
    return ids;
  }

  Future<Session> load(String id) async =>
      SessionCoding.decode(await storage.read(id));

  /// A corrupted file costs one session, never the archive: parse failures are
  /// collected, not thrown.
  Future<LoadResult> loadAll() async {
    final sessions = <Session>[];
    final failures = <LoadFailure>[];
    for (final id in await listIds()) {
      try {
        sessions.add(await load(id));
      } catch (error) {
        failures.add(LoadFailure(id, error));
      }
    }
    return LoadResult(sessions, failures);
  }

  static String idFor(Session session) =>
      '${session.date}_${session.cycleDay}.json';

  Future<bool> exists(String id) async => (await listIds()).contains(id);

  /// Writes the session under its derived id and, when the header moved it,
  /// removes the file it used to live in — otherwise editing the date or cycle
  /// day silently orphans the old file.
  Future<String> save(Session session, {String? previousId}) async {
    final id = idFor(session);
    await storage.write(id, SessionCoding.encode(session));
    if (previousId != null && previousId != id) {
      await storage.delete(previousId);
    }
    return id;
  }

  Future<void> delete(String id) => storage.delete(id);
}
