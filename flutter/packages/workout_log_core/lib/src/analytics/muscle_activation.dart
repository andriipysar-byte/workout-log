import 'dart:math' as math;

import '../models/block.dart';
import '../models/exercise.dart';
import '../models/session.dart';
import '../models/work_set.dart';

/// The three modes deliberately surface different hottest muscles; the UI offers all three.
enum WeightingMode {
  setCount('set_count', 'Sets'), // one point per working set — robust across modalities
  repVolume('rep_volume', 'Reps'), // sets × reps — favours high-rep accessory work
  tonnage('tonnage', 'Tonnage'); // sets × reps × weight — 0 for bodyweight/timed work

  const WeightingMode(this.wire, this.label);

  final String wire;
  final String label;

  static WeightingMode? fromWire(String value) =>
      WeightingMode.values.where((m) => m.wire == value).firstOrNull;
}

/// Pure domain logic (ADR-004): produces `[muscle: score]` maps a renderer colours,
/// normalized so the most-worked muscle is 1.0.
class MuscleActivation {
  const MuscleActivation({this.primaryWeight = 1.0, this.secondaryWeight = 0.5});

  final double primaryWeight;
  final double secondaryWeight;

  /// Single exercise: primary muscles at 1.0, secondary at 0.5.
  Map<String, double> forExercise(Exercise exercise) {
    final raw = <String, double>{};
    for (final m in exercise.primaryMuscles) {
      raw[m] = math.max(raw[m] ?? 0, primaryWeight);
    }
    for (final m in exercise.secondaryMuscles) {
      raw[m] = math.max(raw[m] ?? 0, secondaryWeight);
    }
    return _normalize(raw);
  }

  /// Unknown exercise names are skipped (warn-not-block: they contribute nothing).
  Map<String, double> forSession(
    Session session, {
    required Catalogue catalogue,
    required WeightingMode mode,
  }) {
    final raw = <String, double>{};
    _accumulate(session, raw, catalogue, mode);
    return _normalize(raw);
  }

  /// Whole-cycle map: raw volumes sum across sessions, normalized once so a busy day
  /// can't drown a light one the way summing per-session (already-normalized) maps would.
  Map<String, double> forSessions(
    List<Session> sessions, {
    required Catalogue catalogue,
    required WeightingMode mode,
  }) {
    final raw = <String, double>{};
    for (final session in sessions) {
      _accumulate(session, raw, catalogue, mode);
    }
    return _normalize(raw);
  }

  void _accumulate(
    Session session,
    Map<String, double> raw,
    Catalogue catalogue,
    WeightingMode mode,
  ) {
    void add(Exercise exercise, double volume) {
      if (volume <= 0) return;
      for (final m in exercise.primaryMuscles) {
        raw[m] = (raw[m] ?? 0) + volume * primaryWeight;
      }
      for (final m in exercise.secondaryMuscles) {
        raw[m] = (raw[m] ?? 0) + volume * secondaryWeight;
      }
    }

    for (final block in session.blocks) {
      switch (block) {
        case StrengthBlock(:final exercise, :final sets):
          final resolved = catalogue.resolve(exercise);
          if (resolved == null) continue;
          add(resolved, _strengthVolume(sets, mode));
        case MetconBlock():
          for (final entry in block.exercises) {
            final resolved = catalogue.resolve(entry.name);
            if (resolved == null) continue;
            add(resolved, _metconVolume(block, entry, mode));
          }
        case CardioBlock():
        case CooldownBlock():
          continue;
      }
    }
  }

  double _strengthVolume(List<WorkSet> sets, WeightingMode mode) => switch (mode) {
        WeightingMode.setCount => sets.length.toDouble(),
        WeightingMode.repVolume =>
          sets.fold(0, (sum, s) => sum + _reps(s).toDouble()),
        WeightingMode.tonnage =>
          sets.fold(0, (sum, s) => sum + _reps(s) * (s.weightKg ?? 0)),
      };

  double _metconVolume(
    MetconBlock block,
    MetconExercise exercise,
    WeightingMode mode,
  ) {
    final scheme = exercise.repsOverride ?? block.scheme ?? const <int>[];
    final rounds = block.rounds?.length ?? scheme.length;
    final schemeReps = scheme.fold<int>(0, (a, b) => a + b);
    return switch (mode) {
      WeightingMode.setCount =>
        math.max(rounds, scheme.isEmpty ? 1 : scheme.length).toDouble(),
      WeightingMode.repVolume => schemeReps.toDouble(),
      WeightingMode.tonnage => schemeReps * (exercise.weightKg ?? 0),
    };
  }

  int _reps(WorkSet set) => set.recordedReps ?? 0;

  Map<String, double> _normalize(Map<String, double> raw) {
    if (raw.isEmpty) return raw;
    final peak = raw.values.reduce(math.max);
    if (peak <= 0) return raw;
    return raw.map((k, v) => MapEntry(k, v / peak));
  }
}
