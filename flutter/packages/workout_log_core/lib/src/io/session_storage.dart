/// The byte-level backend a [SessionStore] sits on.
///
/// Declared here, implemented per platform: this is the seam that keeps
/// `dart:io` (and the browser, which has no filesystem at all) out of the core.
/// An id is a bare filename such as `2026-08-06_D2.json`.
abstract interface class SessionStorage {
  Future<List<String>> listIds();

  Future<String> read(String id);

  Future<void> write(String id, String contents);

  Future<void> delete(String id);

  /// A human-readable name for the backing location, for the status bar.
  String get label;
}

/// In-memory backend: the whole store for web, and the test double everywhere else.
class MemoryStorage implements SessionStorage {
  MemoryStorage({Map<String, String>? seed, this.label = 'in-memory'})
      : _files = {...?seed};

  final Map<String, String> _files;

  @override
  final String label;

  Map<String, String> get snapshot => Map.unmodifiable(_files);

  @override
  Future<List<String>> listIds() async => _files.keys.toList();

  @override
  Future<String> read(String id) async {
    final contents = _files[id];
    if (contents == null) throw StateError('no such session: $id');
    return contents;
  }

  @override
  Future<void> write(String id, String contents) async => _files[id] = contents;

  @override
  Future<void> delete(String id) async => _files.remove(id);
}
