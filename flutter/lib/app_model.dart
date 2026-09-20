import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'platform/session_folder.dart';
import 'platform/transfer.dart';

class DayInfo {
  DayInfo({
    required this.id,
    required this.date,
    required this.cycleDay,
    required this.isMetcon,
    required this.group,
  });

  final String id;
  final String date;
  final String cycleDay;
  final bool isMetcon;
  final MuscleGroup? group;
}

/// Which exercises appear on which cycle day. `cells[exercise][day]` is presence
/// only (no load); `groups[exercise]` is that exercise's primary muscle group(s)
/// for colour coding.
class CycleMatrix {
  CycleMatrix({
    this.days = const [],
    this.exercises = const [],
    this.cells = const [],
    this.groups = const [],
  });

  final List<String> days;
  final List<String> exercises;
  final List<List<bool>> cells;
  final List<List<MuscleGroup>> groups;

  bool get isEmpty => days.isEmpty;
}

/// All domain work is delegated to workout_log_core (ADR-004): no parsing,
/// validation or analytics here.
class AppModel extends ChangeNotifier {
  AppModel();

  static const _activation = MuscleActivation();

  SessionFolder? _folder;
  SessionStore? _store;
  Catalogue? _catalogue;
  CycleCatalogue? _cycles;
  String? _mapTemplate;

  /// Decoded sessions keyed by file id. The Swift app re-decoded every file and
  /// re-ran a full activation pass on every refresh and every save; this keeps
  /// the work proportional to what actually changed.
  final Map<String, _FileInfo> _cache = {};
  final Map<String, String> _exerciseMapCache = {};

  List<String> files = const [];
  String? selection;
  Session? session;
  String status = '';
  WeightingMode mode = WeightingMode.setCount;
  Map<String, DayInfo> calendar = const {};
  CycleMatrix cycle = CycleMatrix();
  List<Session> cycleSessions = const [];
  bool ready = false;

  String? _dayMapSvg;
  String? _cycleMapSvg;

  /// The id the loaded session came from, so a header edit can move its file
  /// instead of orphaning it.
  String? _loadedId;

  String get folderLabel => _folder?.storage.label ?? '';

  bool get canChooseFolder => _folder?.canChooseFolder ?? false;

  bool get isWorkingCopy => _folder?.isBrowserCopy ?? false;

  bool get hasCatalogue => _catalogue != null;

  List<Cycle> get cycles => _cycles?.cycles ?? const [];

  /// `folder` is injected by tests, which have no platform channels to resolve
  /// a real directory through.
  Future<void> start({SessionFolder? folder}) async {
    await _loadAssets();
    _folder = folder ?? await openDefaultFolder();
    _store = SessionStore(_folder!.storage);
    ready = true;
    await refresh();
  }

  Future<void> _loadAssets() async {
    try {
      _catalogue = SessionCoding.decodeCatalogue(
        await rootBundle.loadString('assets/data/exercises.json'),
      );
      _cycles = CycleCatalogue.fromJson(
        jsonMap(await rootBundle.loadString('assets/data/cycles.json')),
      );
      _mapTemplate = await rootBundle.loadString('assets/muscle-map.svg');
    } catch (error) {
      status = 'Asset load failed: $error';
    }
  }

  Future<void> refresh({bool fromDisk = false}) async {
    final store = _store;
    if (store == null) return;
    if (fromDisk) _cache.clear();

    try {
      files = await store.listIds();
    } catch (error) {
      // An unreadable folder is a status-bar message, not a crash: the app must
      // still come up so the user can point it somewhere else.
      files = const [];
      status = 'Cannot read ${_folder!.storage.label}: $error';
      _cache.clear();
      _rebuildDerived();
      notifyListeners();
      return;
    }
    for (final id in files) {
      if (_cache.containsKey(id)) continue;
      _cache[id] = await _analyse(store, id);
    }
    _cache.removeWhere((id, _) => !files.contains(id));

    _rebuildDerived();
    notifyListeners();
  }

  Future<_FileInfo> _analyse(SessionStore store, String id) async {
    try {
      final loaded = await store.load(id);
      return _FileInfo(
        session: loaded,
        isMetcon: loaded.blocks.any((b) => b is MetconBlock),
        group: _catalogue == null
            ? null
            : MuscleGroup.dominant(_activation.forSession(
                loaded,
                catalogue: _catalogue!,
                mode: WeightingMode.setCount,
              )),
      );
    } catch (error) {
      return _FileInfo(session: null, isMetcon: false, group: null);
    }
  }

  void _rebuildDerived() {
    final byDate = <String, DayInfo>{};
    final latest = <String, ({String date, Session session})>{};

    for (final id in files) {
      final split = _splitId(id);
      if (split == null) continue;
      final (date, cycleDay) = split;
      final info = _cache[id]!;
      byDate[date] = DayInfo(
        id: id,
        date: date,
        cycleDay: cycleDay,
        isMetcon: info.isMetcon,
        group: info.group,
      );
      final loaded = info.session;
      if (loaded != null &&
          (latest[cycleDay] == null || date.compareTo(latest[cycleDay]!.date) > 0)) {
        latest[cycleDay] = (date: date, session: loaded);
      }
    }

    calendar = byDate;
    cycle = _buildCycle(latest);
    cycleSessions = (latest.keys.toList()..sort())
        .map((day) => latest[day]!.session)
        .toList();
    _cycleMapSvg = null;
    _dayMapSvg = null;
  }

  /// `2026-08-06_D2.json` → (`2026-08-06`, `D2`); null when the name does not
  /// carry the convention.
  static (String, String)? _splitId(String id) {
    final base = id.endsWith('.json') ? id.substring(0, id.length - 5) : id;
    final separator = base.indexOf('_');
    if (separator < 0) return null;
    return (base.substring(0, separator), base.substring(separator + 1));
  }

  /// One column per cycle day (using each day's most recent session), rows =
  /// exercises in first-seen order.
  CycleMatrix _buildCycle(Map<String, ({String date, Session session})> latest) {
    final days = latest.keys.toList()..sort();
    final exercises = <String>[];
    final byDay = <String, Set<String>>{};

    for (final day in days) {
      final present = <String>{};
      for (final block in latest[day]!.session.blocks) {
        final names = switch (block) {
          StrengthBlock(:final exercise) => [exercise],
          MetconBlock(:final exercises) => exercises.map((e) => e.name).toList(),
          _ => const <String>[],
        };
        for (final name in names) {
          present.add(name);
          if (!exercises.contains(name)) exercises.add(name);
        }
      }
      byDay[day] = present;
    }

    return CycleMatrix(
      days: days,
      exercises: exercises,
      cells: [
        for (final ex in exercises)
          [for (final day in days) byDay[day]?.contains(ex) ?? false],
      ],
      groups: [for (final ex in exercises) primaryGroups(ex)],
    );
  }

  /// Distinct primary muscle groups of an exercise, in listed order (empty if
  /// not in the catalogue).
  List<MuscleGroup> primaryGroups(String name) {
    final exercise = _catalogue?.resolve(name);
    if (exercise == null) return const [];
    final seen = <MuscleGroup>[];
    for (final muscle in exercise.primaryMuscles) {
      final group = MuscleGroup.of(muscle);
      if (group != null && !seen.contains(group)) seen.add(group);
    }
    return seen;
  }

  String? dayMapSVG() {
    final loaded = session;
    if (loaded == null || _catalogue == null || _mapTemplate == null) {
      return null;
    }
    return _dayMapSvg ??= MuscleMapSVG.colorize(
      _mapTemplate!,
      _activation.forSession(loaded, catalogue: _catalogue!, mode: mode),
    );
  }

  String? cycleMapSVG() {
    if (_catalogue == null || _mapTemplate == null || cycleSessions.isEmpty) {
      return null;
    }
    return _cycleMapSvg ??= MuscleMapSVG.colorize(
      _mapTemplate!,
      _activation.forSessions(
        cycleSessions,
        catalogue: _catalogue!,
        mode: mode,
      ),
    );
  }

  String? exerciseMapSVG(String name) {
    final exercise = _catalogue?.resolve(name);
    if (exercise == null || _mapTemplate == null) return null;
    return _exerciseMapCache[exercise.name] ??= MuscleMapSVG.colorize(
      _mapTemplate!,
      _activation.forExercise(exercise),
    );
  }

  void setMode(WeightingMode next) {
    if (mode == next) return;
    mode = next;
    _dayMapSvg = null;
    _cycleMapSvg = null;
    notifyListeners();
  }

  /// The editor mutates the session in place, so the widget tree has to be told
  /// that the maps it derives from it are now stale.
  void sessionEdited() {
    _dayMapSvg = null;
    notifyListeners();
  }

  Future<void> open(String id) async {
    try {
      session = await _store!.load(id);
      selection = id;
      _loadedId = id;
      _dayMapSvg = null;
      status = 'Loaded $id';
    } catch (error) {
      status = 'Load failed: $error';
    }
    notifyListeners();
  }

  Future<void> save() async {
    final loaded = session;
    if (loaded == null) return;
    try {
      final previous = _loadedId;
      final id = await _store!.save(loaded, previousId: previous);
      _cache.remove(id);
      if (previous != null) _cache.remove(previous);
      _loadedId = id;
      selection = id;
      status = previous != null && previous != id
          ? 'Renamed $previous → $id'
          : 'Saved $id';
      await refresh();
    } catch (error) {
      status = 'Save failed: $error';
      notifyListeners();
    }
  }

  Future<bool> exists(String id) => _store!.exists(id);

  /// The bytes on disk, for tests and for export.
  Future<String> readRaw(String id) => _folder!.storage.read(id);

  Future<void> create(Session created) async {
    try {
      final id = await _store!.save(created);
      _cache.remove(id);
      session = created;
      selection = id;
      _loadedId = id;
      _dayMapSvg = null;
      status = 'Created $id';
      await refresh();
    } catch (error) {
      status = 'Create failed: $error';
      notifyListeners();
    }
  }

  Future<void> delete(String id) async {
    try {
      await _store!.delete(id);
      _cache.remove(id);
      if (selection == id) {
        selection = null;
        session = null;
        _loadedId = null;
        _dayMapSvg = null;
      }
      status = 'Deleted $id';
      await refresh();
    } catch (error) {
      status = 'Delete failed: $error';
      notifyListeners();
    }
  }

  Future<void> pickFolder() async {
    final chosen = await chooseFolder();
    if (chosen == null) return;
    _folder = chosen;
    _store = SessionStore(chosen.storage);
    _cache.clear();
    session = null;
    selection = null;
    _loadedId = null;
    await refresh();
    status = 'Folder: ${chosen.storage.label}';
    notifyListeners();
  }

  Future<void> importSessionFiles() async {
    final picked = await importSessions();
    if (picked.isEmpty) return;
    var written = 0;
    for (final file in picked) {
      try {
        // Decoding first means a malformed pick is rejected at the door rather
        // than landing in the archive as an unreadable file.
        SessionCoding.decode(file.contents);
        await _folder!.storage.write(file.name, file.contents);
        _cache.remove(file.name);
        written++;
      } catch (error) {
        status = 'Skipped ${file.name}: $error';
      }
    }
    await refresh();
    status = 'Imported $written of ${picked.length} file(s)';
    notifyListeners();
  }

  Future<void> exportSessionFiles() async {
    final storage = _folder!.storage;
    final payload = [
      for (final id in files) TransferFile(id, await storage.read(id)),
    ];
    if (payload.isEmpty) {
      status = 'Nothing to export';
      notifyListeners();
      return;
    }
    final destination = await exportSessions(payload);
    status = destination == null ? 'Export cancelled' : 'Exported to $destination';
    notifyListeners();
  }
}

class _FileInfo {
  _FileInfo({
    required this.session,
    required this.isMetcon,
    required this.group,
  });

  /// Null when the file failed to decode — a corrupted file costs one session,
  /// never the archive.
  final Session? session;
  final bool isMetcon;
  final MuscleGroup? group;
}
