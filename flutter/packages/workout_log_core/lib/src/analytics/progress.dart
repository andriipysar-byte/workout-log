import '../coding.dart';
import '../models/block.dart';
import '../models/exercise.dart';
import '../models/session.dart';
import '../models/work_set.dart';

/// Estimated one-rep max: a common scale for comparing a six against a triple.
/// A trend line, never a truth — both formulas drift badly above ~10 reps.
abstract final class OneRepMax {
  static double? epley(double? weightKg, int? reps) {
    if (weightKg == null || reps == null || reps < 1 || weightKg <= 0) {
      return null;
    }
    return reps == 1 ? weightKg : weightKg * (1 + reps / 30);
  }

  static double? brzycki(double? weightKg, int? reps) {
    if (weightKg == null || reps == null || reps < 1 || reps >= 37 ||
        weightKg <= 0) {
      return null;
    }
    return weightKg * 36 / (37 - reps);
  }

  /// The mean of the two: they bracket the real number from either side, and
  /// picking one would import its bias into every chart.
  static double? estimate(double? weightKg, int? reps) {
    final e = epley(weightKg, reps);
    final b = brzycki(weightKg, reps);
    if (e == null) return null;
    if (b == null) return _round(e);
    return _round((e + b) / 2);
  }
}

/// One session's contribution to one (variant, rep band) track.
class ProgressPoint {
  ProgressPoint({
    required this.date,
    required this.cycleDay,
    required this.kind,
    required this.weightKg,
    required this.reps,
    this.rir,
    this.barSpeed,
    this.backoffSets = 0,
    this.backoffTonnageKg = 0,
    this.sets = 0,
  });

  final String date;
  final String cycleDay;
  final Kind kind;

  /// The top set: heaviest non-back-off set of the slot (P2).
  final double? weightKg;
  final int? reps;
  final double? rir;
  final BarSpeed? barSpeed;

  /// Back-off work is reported beside the top set, never averaged into it (P2).
  final int backoffSets;
  final double backoffTonnageKg;
  final int sets;

  double? get estimated1rmKg => OneRepMax.estimate(weightKg, reps);

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'date': date, 'cycle_day': cycleDay};
    if (kind != Kind.training) json['kind'] = kind.name;
    put(json, 'weight_kg', weightKg);
    put(json, 'reps', reps);
    put(json, 'estimated_1rm_kg', estimated1rmKg);
    put(json, 'rir', rir);
    put(json, 'bar_speed', barSpeed?.name);
    json['sets'] = sets;
    if (backoffSets > 0) {
      json['backoff_sets'] = backoffSets;
      json['backoff_tonnage_kg'] = _round(backoffTonnageKg);
    }
    return json;
  }
}

/// A single progress track. Rep bands are independent tracks on the same lift
/// (P6), so a stall in one is visibly separate from the others.
class ProgressSeries {
  ProgressSeries({
    required this.exercise,
    required this.band,
    required this.points,
  });

  final String exercise;
  final RepBand? band;
  final List<ProgressPoint> points;

  /// Change in estimated 1RM from the first point that has one to the last.
  double? get trendKg {
    final estimates = [
      for (final p in points)
        if (p.estimated1rmKg != null) p.estimated1rmKg!,
    ];
    if (estimates.length < 2) return null;
    return _round(estimates.last - estimates.first);
  }

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'exercise': exercise};
    put(json, 'rep_band', band?.name);
    put(json, 'trend_1rm_kg', trendKg);
    json['points'] = [for (final p in points) p.toJson()];
    return json;
  }
}

class ProgressReport {
  ProgressReport({
    required this.query,
    required this.series,
    this.resolved,
    this.pattern,
    this.variants = const [],
  });

  final String query;

  /// The canonical name the query resolved to, or null when the catalogue does
  /// not know it — the series are still built from the sessions (warn, never block).
  final String? resolved;
  final MovementPattern? pattern;

  /// Every exercise included: one name, or the whole pattern when asked (P8).
  final List<String> variants;
  final List<ProgressSeries> series;

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'query': query};
    put(json, 'resolved', resolved);
    put(json, 'pattern', pattern?.name);
    if (variants.length > 1) json['variants'] = variants;
    json['series'] = [for (final s in series) s.toJson()];
    return json;
  }
}

abstract final class ExerciseProgress {
  /// Best set over time for one lift, sliced by rep band.
  ///
  /// `byPattern` widens the question from "this lift" to "this pattern" — the
  /// conjugate view P8 asks for, where a front squat and an overhead squat are
  /// two variants of one thing rather than two sparse, hopeless trend lines.
  static ProgressReport report(
    List<Session> sessions, {
    required String exercise,
    Catalogue? catalogue,
    bool byPattern = false,
  }) {
    final resolved = catalogue?.resolve(exercise);
    final pattern = byPattern ? resolved?.pattern : null;

    bool matches(String name) {
      if (pattern != null) {
        return catalogue?.resolve(name)?.pattern == pattern;
      }
      final canonical = catalogue?.resolve(name);
      if (canonical != null && resolved != null) {
        return canonical.name == resolved.name;
      }
      return name.trim().toLowerCase() == exercise.trim().toLowerCase();
    }

    final tracks = <(String, RepBand?), List<ProgressPoint>>{};
    final variants = <String>{};
    final ordered = [...sessions]..sort((a, b) => a.date.compareTo(b.date));

    for (final session in ordered) {
      for (final block in session.blocks) {
        if (block is! StrengthBlock || !matches(block.exercise)) continue;
        final name = catalogue?.resolve(block.exercise)?.name ?? block.exercise;
        variants.add(name);
        for (final entry in _slotPoints(session, name, block).entries) {
          tracks.putIfAbsent((name, entry.key), () => []).add(entry.value);
        }
      }
    }

    final keys = tracks.keys.toList()
      ..sort((a, b) {
        final byName = a.$1.compareTo(b.$1);
        if (byName != 0) return byName;
        return (a.$2?.index ?? -1).compareTo(b.$2?.index ?? -1);
      });

    return ProgressReport(
      query: exercise,
      resolved: resolved?.name,
      pattern: pattern ?? resolved?.pattern,
      variants: variants.toList()..sort(),
      series: [
        for (final key in keys)
          ProgressSeries(
            exercise: key.$1,
            band: key.$2,
            points: tracks[key]!,
          ),
      ],
    );
  }

  /// One slot can contribute to several bands at once — a ramp of sixes topped
  /// off with a triple is a point on both tracks, each with its own top set.
  static Map<RepBand?, ProgressPoint> _slotPoints(
    Session session,
    String exercise,
    StrengthBlock block,
  ) {
    final byBand = <RepBand?, List<WorkSet>>{};
    for (final set in block.sets) {
      byBand.putIfAbsent(set.band, () => []).add(set);
    }
    return {
      for (final entry in byBand.entries)
        entry.key: _pointFor(session, entry.value),
    };
  }

  static ProgressPoint _pointFor(Session session, List<WorkSet> sets) {
    WorkSet? top;
    var backoffSets = 0;
    var backoffTonnage = 0.0;
    for (final set in sets) {
      if (set.isBackoff ?? false) {
        backoffSets++;
        backoffTonnage += set.tonnageKg;
        continue;
      }
      if (top == null || (set.weightKg ?? 0) >= (top.weightKg ?? 0)) top = set;
    }
    // A slot that is nothing but back-offs still has a heaviest set; reporting
    // no top set at all would drop the session from the track entirely.
    top ??= sets.reduce((a, b) => (b.weightKg ?? 0) >= (a.weightKg ?? 0) ? b : a);
    return ProgressPoint(
      date: session.date,
      cycleDay: session.cycleDay,
      kind: session.kind,
      weightKg: top.weightKg,
      reps: top.recordedReps,
      rir: top.rir,
      barSpeed: top.barSpeed,
      backoffSets: backoffSets,
      backoffTonnageKg: backoffTonnage,
      sets: sets.length,
    );
  }
}

double? _round(double? value) =>
    value == null ? null : (value * 10).round() / 10;
