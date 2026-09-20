import '../coding.dart';
import '../models/block.dart';
import '../models/exercise.dart';
import '../models/session.dart';
import '../models/work_set.dart';
import 'session_metrics.dart';

/// A finding worth a training decision, tagged with the principle it comes from.
/// The system reports; the programming decisions stay the athlete's.
class TrainingAlert {
  const TrainingAlert({
    required this.principle,
    required this.message,
    this.sessions = const [],
  });

  /// `P3`, `P5`, `P8`, `P10` — see `docs/01-training-principles.md`.
  final String principle;
  final String message;
  final List<String> sessions;

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{
      'principle': principle,
      'message': message,
    };
    if (sessions.isNotEmpty) json['sessions'] = sessions;
    return json;
  }
}

/// How often a movement pattern was trained — the frequency variety is bought
/// with (P8). Below roughly one slot per cycle, linear progression is dead.
class PatternFrequency {
  const PatternFrequency({
    required this.pattern,
    required this.slots,
    required this.sessions,
    required this.perCycle,
  });

  final MovementPattern pattern;
  final int slots;
  final int sessions;
  final double perCycle;

  Map<String, dynamic> toJson() => {
        'pattern': pattern.name,
        'slots': slots,
        'sessions': sessions,
        'per_cycle': perCycle,
      };
}

/// Strength and conditioning side by side, never fused into one score: fatigue
/// is additive and the point is to see when both rise together (P7).
class LoadPoint {
  const LoadPoint({
    required this.date,
    required this.cycleDay,
    required this.kind,
    required this.tonnageKg,
    required this.sets,
    this.metconSec,
    this.workMin,
  });

  final String date;
  final String cycleDay;
  final Kind kind;
  final double tonnageKg;
  final int sets;
  final double? metconSec;
  final double? workMin;

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{
      'date': date,
      'cycle_day': cycleDay,
      'kind': kind.name,
      'tonnage_kg': tonnageKg,
      'sets': sets,
    };
    put(json, 'metcon_sec', metconSec);
    put(json, 'work_min', workMin);
    return json;
  }
}

/// What a stretch of sessions says as a block rather than one at a time: band
/// distribution, pattern frequency, combined load, and the alerts those raise.
class TrainingReport {
  TrainingReport({
    required this.from,
    required this.to,
    required this.days,
    required this.cycleLengthDays,
    required this.sessionCount,
    required this.kinds,
    required this.bandSets,
    required this.bandSlots,
    required this.patterns,
    required this.load,
    required this.timeDomains,
    required this.alerts,
    required this.unknownExercises,
  });

  /// [volumeSlotShareThreshold] guards P5, the failure that actually happened:
  /// a block with every slot in the volume range left no light session to
  /// recover against and ended in an involuntary deload. P4 rations volume work
  /// to 2-3 slots per cycle, which is where the default comes from.
  factory TrainingReport.of(
    List<Session> sessions, {
    required Catalogue catalogue,
    int cycleLengthDays = 12,
    double volumeSlotShareThreshold = 0.3,
  }) {
    final ordered = [...sessions]..sort((a, b) => a.date.compareTo(b.date));
    final bandSets = <RepBand, int>{};
    final bandSlots = <RepBand, int>{};
    final patternSlots = <MovementPattern, int>{};
    final patternSessions = <MovementPattern, Set<String>>{};
    final timeDomains = <TimeDomain, int>{};
    final kinds = <Kind, int>{};
    final load = <LoadPoint>[];
    final unknown = <String>{};
    final alerts = <TrainingAlert>[];
    final explosiveOffenders = <String>{};
    final decayOffenders = <String>{};

    for (final session in ordered) {
      final metrics = SessionMetrics.of(session);
      kinds[session.kind] = (kinds[session.kind] ?? 0) + 1;
      metrics.bandSets.forEach((band, n) => bandSets[band] = (bandSets[band] ?? 0) + n);
      metrics.bandSlots.forEach((band, n) => bandSlots[band] = (bandSlots[band] ?? 0) + n);
      for (final metcon in metrics.metcons) {
        final domain = metcon.domain;
        if (domain != null) timeDomains[domain] = (timeDomains[domain] ?? 0) + 1;
      }
      load.add(LoadPoint(
        date: session.date,
        cycleDay: session.cycleDay,
        kind: session.kind,
        tonnageKg: metrics.tonnageKg,
        sets: metrics.sets,
        metconSec: metrics.metcons.isEmpty
            ? null
            : metrics.metcons.fold<double>(
                0, (sum, m) => sum + (m.totalSec ?? 0)),
        workMin: metrics.density.workMin > 0 ? metrics.density.workMin : null,
      ));

      for (final block in session.blocks) {
        final names = switch (block) {
          StrengthBlock(:final exercise) => [exercise],
          MetconBlock() => [for (final e in block.exercises) e.name],
          _ => const <String>[],
        };
        for (final name in names) {
          final resolved = catalogue.resolve(name);
          if (resolved == null) {
            if (name.trim().isNotEmpty) unknown.add(name);
            continue;
          }
          final pattern = resolved.pattern;
          if (pattern == null) continue;
          if (block is StrengthBlock) {
            patternSlots[pattern] = (patternSlots[pattern] ?? 0) + 1;
          }
          patternSessions.putIfAbsent(pattern, () => {}).add(session.date);
        }

        if (block is StrengthBlock) {
          final resolved = catalogue.resolve(block.exercise);
          if (resolved != null && resolved.isExplosive) {
            final label = '${session.date} ${block.exercise}';
            if (block.sets.any((s) => (s.recordedReps ?? 0) > 3)) {
              explosiveOffenders.add(label);
            }
            if (_repsCollapse(block.sets)) decayOffenders.add(label);
          }
        }
      }
    }

    final from = ordered.isEmpty ? null : ordered.first.date;
    final to = ordered.isEmpty ? null : ordered.last.date;
    final days = _daysBetween(from, to);
    final cycles = days == null ? null : days / cycleLengthDays;

    final patterns = [
      for (final pattern in MovementPattern.values)
        if ((patternSlots[pattern] ?? 0) > 0 ||
            (patternSessions[pattern]?.isNotEmpty ?? false))
          PatternFrequency(
            pattern: pattern,
            slots: patternSlots[pattern] ?? 0,
            sessions: patternSessions[pattern]?.length ?? 0,
            perCycle: cycles == null || cycles <= 0
                ? 0
                : _round((patternSlots[pattern] ?? 0) / cycles),
          ),
    ];

    final totalSlots = bandSlots.values.fold<int>(0, (a, b) => a + b);
    final volumeSlots = bandSlots[RepBand.volume] ?? 0;
    if (totalSlots > 0 && volumeSlots / totalSlots > volumeSlotShareThreshold) {
      alerts.add(TrainingAlert(
        principle: 'P5',
        message: 'volume (7+) slots are '
            '${(volumeSlots / totalSlots * 100).round()}% of $totalSlots slots, '
            'over the ${(volumeSlotShareThreshold * 100).round()}% guard — the '
            'state that produced the involuntary deload',
      ));
    }
    if (explosiveOffenders.isNotEmpty) {
      alerts.add(TrainingAlert(
        principle: 'P3',
        message: 'explosive lifts logged above 3 reps — bar speed decays and the '
            'lift is trained in the wrong zone',
        sessions: explosiveOffenders.toList()..sort(),
      ));
    }
    if (decayOffenders.isNotEmpty) {
      alerts.add(TrainingAlert(
        principle: 'P3',
        message: 'reps collapse across the ramp on an explosive lift '
            '(the 4 / 2 / 1 pattern)',
        sessions: decayOffenders.toList()..sort(),
      ));
    }
    if (cycles != null && cycles >= 1) {
      final starved = [
        for (final p in patterns)
          if (p.perCycle < 1) '${p.pattern.name} (${p.perCycle}/cycle)',
      ];
      if (starved.isNotEmpty) {
        alerts.add(TrainingAlert(
          principle: 'P8',
          message: 'trained below once per $cycleLengthDays-day cycle, so linear '
              'progression on them is dead: ${starved.join(', ')}',
        ));
      }
    }
    if (cycles != null &&
        cycles >= 2 &&
        (kinds[Kind.deload] ?? 0) == 0 &&
        (kinds[Kind.retest] ?? 0) == 0) {
      alerts.add(TrainingAlert(
        principle: 'P10',
        message: 'no deload or retest in ${days!} days — deloads are hygiene, '
            'and progress between anchors is the unit of evaluation',
      ));
    }

    return TrainingReport(
      from: from,
      to: to,
      days: days,
      cycleLengthDays: cycleLengthDays,
      sessionCount: ordered.length,
      kinds: kinds,
      bandSets: bandSets,
      bandSlots: bandSlots,
      patterns: patterns,
      load: load,
      timeDomains: timeDomains,
      alerts: alerts,
      unknownExercises: unknown.toList()..sort(),
    );
  }

  final String? from;
  final String? to;
  final int? days;
  final int cycleLengthDays;
  final int sessionCount;
  final Map<Kind, int> kinds;
  final Map<RepBand, int> bandSets;
  final Map<RepBand, int> bandSlots;
  final List<PatternFrequency> patterns;
  final List<LoadPoint> load;
  final Map<TimeDomain, int> timeDomains;
  final List<TrainingAlert> alerts;

  /// Names no catalogue entry resolves: surfaced, never silently dropped (ADR-006).
  final List<String> unknownExercises;

  double get totalTonnageKg =>
      _round(load.fold<double>(0, (sum, p) => sum + p.tonnageKg));

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    put(json, 'from', from);
    put(json, 'to', to);
    put(json, 'days', days);
    json['sessions'] = sessionCount;
    json['kinds'] = {
      for (final kind in Kind.values)
        if ((kinds[kind] ?? 0) > 0) kind.name: kinds[kind]!,
    };
    json['rep_bands'] = {
      'sets': {
        for (final band in RepBand.values)
          if ((bandSets[band] ?? 0) > 0) band.name: bandSets[band]!,
      },
      'slots': {
        for (final band in RepBand.values)
          if ((bandSlots[band] ?? 0) > 0) band.name: bandSlots[band]!,
      },
    };
    json['patterns'] = [for (final p in patterns) p.toJson()];
    json['time_domains'] = {
      for (final domain in TimeDomain.values)
        if ((timeDomains[domain] ?? 0) > 0) domain.name: timeDomains[domain]!,
    };
    json['total_tonnage_kg'] = totalTonnageKg;
    json['load'] = [for (final p in load) p.toJson()];
    json['alerts'] = [for (final a in alerts) a.toJson()];
    if (unknownExercises.isNotEmpty) {
      json['unknown_exercises'] = unknownExercises;
    }
    return json;
  }
}

/// The 4 / 2 / 1 shape: three or more recorded sets whose reps only ever fall.
bool _repsCollapse(List<WorkSet> sets) {
  final reps = [
    for (final set in sets)
      if ((set.recordedReps ?? 0) > 0) set.recordedReps!,
  ];
  if (reps.length < 3) return false;
  for (var i = 1; i < reps.length; i++) {
    if (reps[i] >= reps[i - 1]) return false;
  }
  return true;
}

int? _daysBetween(String? from, String? to) {
  if (from == null || to == null) return null;
  final start = DateTime.tryParse(from);
  final end = DateTime.tryParse(to);
  if (start == null || end == null) return null;
  return end.difference(start).inDays + 1;
}

double _round(double value) => (value * 10).round() / 10;
