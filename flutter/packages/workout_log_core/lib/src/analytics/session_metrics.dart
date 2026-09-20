import '../coding.dart';
import '../models/block.dart';
import '../models/session.dart';
import '../models/work_set.dart';
import '../wall_clock.dart';

/// Which energy system a metcon taxed, by how long it lasted (P9).
///
/// Classified on total duration, which is all the file records. An interval
/// workout of short efforts therefore lands in the domain its *clock* says, not
/// the one its work intervals say — those live in the block's notes.
enum TimeDomain {
  alactic, // <= 5 min
  glycolytic, // 5-20 min
  aerobic; // > 20 min

  static TimeDomain? forSeconds(double? seconds) => switch (seconds) {
        null => null,
        <= 0 => null,
        <= 300 => TimeDomain.alactic,
        <= 1200 => TimeDomain.glycolytic,
        _ => TimeDomain.aerobic,
      };
}

/// Work time against clock time, from the bracket timestamps (P1).
class SessionDensity {
  const SessionDensity({
    required this.workMin,
    required this.transitionMin,
    this.totalMin,
  });

  final double workMin;
  final double transitionMin;

  /// Null when the session has no start time or nothing that ends: density is
  /// derived from what was written down, never guessed.
  final double? totalMin;

  double? get workShare {
    final total = totalMin;
    if (total == null || total <= 0) return null;
    return _round(workMin / total, 3);
  }

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{
      'work_min': _round(workMin),
      'transition_min': _round(transitionMin),
    };
    put(json, 'total_min', _round(totalMin));
    put(json, 'work_share', workShare);
    return json;
  }
}

class BlockMetrics {
  const BlockMetrics({
    required this.index,
    required this.type,
    this.label,
    this.durationMin,
    this.transitionMin,
    this.sets = 0,
    this.backoffSets = 0,
    this.reps = 0,
    this.tonnageKg = 0,
    this.band,
  });

  final int index;
  final String type;

  /// The exercise, the machine, or the metcon's format — whatever names the block.
  final String? label;
  final double? durationMin;
  final double? transitionMin;
  final int sets;
  final int backoffSets;
  final int reps;
  final double tonnageKg;

  /// The slot's own rep band (P6): the band most of its sets sit in.
  final RepBand? band;

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'index': index, 'type': type};
    put(json, 'label', label);
    put(json, 'duration_min', _round(durationMin));
    put(json, 'transition_min', _round(transitionMin));
    if (sets > 0) json['sets'] = sets;
    if (backoffSets > 0) json['backoff_sets'] = backoffSets;
    if (reps > 0) json['reps'] = reps;
    if (tonnageKg > 0) json['tonnage_kg'] = _round(tonnageKg);
    put(json, 'rep_band', band?.name);
    return json;
  }
}

/// One round of a metcon with its split differenced out of the cumulative
/// clock the paper log records.
class RoundSplit {
  const RoundSplit({
    required this.round,
    this.reps,
    this.cumulativeSec,
    this.splitSec,
    this.heartRate,
  });

  final int round;
  final int? reps;
  final double? cumulativeSec;
  final double? splitSec;
  final int? heartRate;

  double? get secondsPerRep {
    final split = splitSec;
    final count = reps;
    if (split == null || count == null || count <= 0) return null;
    return _round(split / count, 2);
  }

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'round': round};
    put(json, 'reps', reps);
    put(json, 'cumulative_sec', _round(cumulativeSec));
    put(json, 'split_sec', _round(splitSec));
    put(json, 'sec_per_rep', secondsPerRep);
    put(json, 'heart_rate', heartRate);
    return json;
  }
}

class MetconMetrics {
  MetconMetrics({
    required this.blockIndex,
    this.format,
    this.scheme,
    this.exercises = const [],
    this.totalSec,
    this.rounds = const [],
  });

  final int blockIndex;
  final String? format;
  final List<int>? scheme;
  final List<String> exercises;
  final double? totalSec;
  final List<RoundSplit> rounds;

  TimeDomain? get domain => TimeDomain.forSeconds(totalSec);

  /// How much slower the last round ran than the first, per rep, as a percentage
  /// — the pacing question a 21-15-9 is asked (positive = faded).
  double? get paceDecayPercent {
    final paces = [
      for (final round in rounds)
        if (round.secondsPerRep != null) round.secondsPerRep!,
    ];
    if (paces.length < 2 || paces.first <= 0) return null;
    return _round((paces.last - paces.first) / paces.first * 100, 1);
  }

  int? get peakHeartRate {
    final rates = [
      for (final round in rounds)
        if (round.heartRate != null) round.heartRate!,
    ];
    if (rates.isEmpty) return null;
    return rates.reduce((a, b) => a > b ? a : b);
  }

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{'block_index': blockIndex};
    put(json, 'format', format);
    put(json, 'scheme', scheme);
    if (exercises.isNotEmpty) json['exercises'] = exercises;
    put(json, 'total_sec', _round(totalSec));
    put(json, 'time_domain', domain?.name);
    put(json, 'pace_decay_percent', paceDecayPercent);
    put(json, 'peak_heart_rate', peakHeartRate);
    if (rounds.isNotEmpty) {
      json['rounds'] = [for (final r in rounds) r.toJson()];
    }
    return json;
  }
}

/// Everything one session says about itself once the derivations are run:
/// tonnage, rep bands, density and metcon splits. Nothing here is stored — the
/// file holds only what was observed (ADR-001).
class SessionMetrics {
  SessionMetrics({
    required this.date,
    required this.cycleDay,
    required this.kind,
    required this.blocks,
    required this.metcons,
    required this.density,
    required this.tonnageKg,
    required this.sets,
    required this.backoffSets,
    required this.reps,
    required this.bandSets,
    required this.bandSlots,
  });

  factory SessionMetrics.of(Session session) {
    final blocks = <BlockMetrics>[];
    final metcons = <MetconMetrics>[];
    final bandSets = <RepBand, int>{};
    final bandSlots = <RepBand, int>{};
    var tonnage = 0.0;
    var sets = 0;
    var backoffSets = 0;
    var reps = 0;
    var workMin = 0.0;
    var transitionMin = 0.0;

    final sessionStart = WallClock.minutes(session.startTime);
    int? cursor = sessionStart;
    int? lastEnd;

    for (var index = 0; index < session.blocks.length; index++) {
      final block = session.blocks[index];
      final start = WallClock.minutes(_startTimeOf(block)) ?? cursor;
      final end = WallClock.minutes(_endTimeOf(block));

      double? duration;
      if (start != null && end != null && end >= start) {
        duration = (end - start).toDouble();
      } else if (block is CardioBlock) {
        duration = block.durationMin;
      }

      double? transition;
      if (cursor != null && start != null && start > cursor) {
        transition = (start - cursor).toDouble();
      }

      if (duration != null) workMin += duration;
      if (transition != null) transitionMin += transition;

      if (end != null) {
        cursor = end;
        lastEnd = end;
      } else if (start != null && duration != null) {
        cursor = start + duration.round();
        lastEnd = cursor;
      }

      switch (block) {
        case StrengthBlock(:final exercise, sets: final blockSets):
          final counts = <RepBand, int>{};
          var blockReps = 0;
          var blockTonnage = 0.0;
          var blockBackoff = 0;
          for (final set in blockSets) {
            final band = set.band;
            if (band != null) {
              counts[band] = (counts[band] ?? 0) + 1;
              bandSets[band] = (bandSets[band] ?? 0) + 1;
            }
            blockReps += set.recordedReps ?? 0;
            blockTonnage += set.tonnageKg;
            if (set.isBackoff ?? false) blockBackoff++;
          }
          final slotBand = _dominantBand(counts);
          if (slotBand != null) {
            bandSlots[slotBand] = (bandSlots[slotBand] ?? 0) + 1;
          }
          sets += blockSets.length;
          backoffSets += blockBackoff;
          reps += blockReps;
          tonnage += blockTonnage;
          blocks.add(BlockMetrics(
            index: index,
            type: block.type,
            label: exercise,
            durationMin: duration,
            transitionMin: transition,
            sets: blockSets.length,
            backoffSets: blockBackoff,
            reps: blockReps,
            tonnageKg: blockTonnage,
            band: slotBand,
          ));
        case MetconBlock():
          final metcon = _metconMetrics(index, block, start, end);
          metcons.add(metcon);
          blocks.add(BlockMetrics(
            index: index,
            type: block.type,
            label: block.format?.wire ?? 'metcon',
            durationMin: duration ??
                (metcon.totalSec == null ? null : metcon.totalSec! / 60),
            transitionMin: transition,
          ));
        case CardioBlock(:final machine):
          blocks.add(BlockMetrics(
            index: index,
            type: block.type,
            label: machine.isEmpty ? null : machine,
            durationMin: duration,
            transitionMin: transition,
          ));
        case CooldownBlock():
          blocks.add(BlockMetrics(
            index: index,
            type: block.type,
            durationMin: duration,
            transitionMin: transition,
          ));
      }
    }

    final totalMin = (sessionStart != null && lastEnd != null)
        ? (lastEnd - sessionStart).toDouble()
        : null;

    return SessionMetrics(
      date: session.date,
      cycleDay: session.cycleDay,
      kind: session.kind,
      blocks: blocks,
      metcons: metcons,
      density: SessionDensity(
        workMin: workMin,
        transitionMin: transitionMin,
        totalMin: totalMin,
      ),
      tonnageKg: tonnage,
      sets: sets,
      backoffSets: backoffSets,
      reps: reps,
      bandSets: bandSets,
      bandSlots: bandSlots,
    );
  }

  final String date;
  final String cycleDay;
  final Kind kind;
  final List<BlockMetrics> blocks;
  final List<MetconMetrics> metcons;
  final SessionDensity density;
  final double tonnageKg;
  final int sets;
  final int backoffSets;
  final int reps;

  /// Sets per band, and slots (strength blocks) per band. Both matter: P6 tracks
  /// sets, P4's "2-3 volume slots per cycle" counts slots.
  final Map<RepBand, int> bandSets;
  final Map<RepBand, int> bandSlots;

  /// The ramp's top set is the progress signal, back-offs are volume (P2), so
  /// they are never averaged together.
  int get topSets => sets - backoffSets;

  Map<String, dynamic> toJson() => {
        'date': date,
        'cycle_day': cycleDay,
        'kind': kind.name,
        'strength': {
          'tonnage_kg': _round(tonnageKg),
          'sets': sets,
          'top_sets': topSets,
          'backoff_sets': backoffSets,
          'reps': reps,
          'rep_band_sets': _bandJson(bandSets),
          'rep_band_slots': _bandJson(bandSlots),
        },
        'density': density.toJson(),
        'blocks': [for (final b in blocks) b.toJson()],
        if (metcons.isNotEmpty)
          'metcons': [for (final m in metcons) m.toJson()],
      };
}

Map<String, int> _bandJson(Map<RepBand, int> counts) => {
      for (final band in RepBand.values)
        if ((counts[band] ?? 0) > 0) band.name: counts[band]!,
    };

/// Ties go to the heavier band: a slot that is half sixes and half eights is
/// not evidence for the volume dose P4 rations.
RepBand? _dominantBand(Map<RepBand, int> counts) {
  RepBand? best;
  var bestCount = 0;
  for (final band in RepBand.values) {
    final count = counts[band] ?? 0;
    if (count > bestCount) {
      best = band;
      bestCount = count;
    }
  }
  return best;
}

MetconMetrics _metconMetrics(
  int index,
  MetconBlock block,
  int? startMin,
  int? endMin,
) {
  final rounds = <RoundSplit>[];
  double? previous;
  for (final round in block.rounds ?? const <MetconRound>[]) {
    final cumulative = round.splitCumulativeSec;
    double? split;
    if (cumulative != null) {
      split = previous == null ? cumulative : cumulative - previous;
      previous = cumulative;
    }
    rounds.add(RoundSplit(
      round: round.round,
      reps: round.reps ?? _schemeReps(block, round.round),
      cumulativeSec: cumulative,
      splitSec: split,
      heartRate: round.heartRate,
    ));
  }

  final fromRounds = rounds.isEmpty ? null : rounds.last.cumulativeSec;
  final fromClock = (startMin != null && endMin != null && endMin >= startMin)
      ? (endMin - startMin) * 60.0
      : null;

  return MetconMetrics(
    blockIndex: index,
    format: block.format?.wire,
    scheme: block.scheme,
    exercises: [for (final e in block.exercises) e.name],
    totalSec: fromRounds ?? fromClock,
    rounds: rounds,
  );
}

/// The reps a round carries when the round itself does not say: the scheme
/// position it matches (21-15-9 → round 2 is 15).
int? _schemeReps(MetconBlock block, int round) {
  final scheme = block.scheme;
  if (scheme == null || round < 1 || round > scheme.length) return null;
  return scheme[round - 1];
}

String? _startTimeOf(Block block) => switch (block) {
      StrengthBlock(:final startTime) => startTime,
      MetconBlock(:final startTime) => startTime,
      CardioBlock() || CooldownBlock() => null,
    };

String? _endTimeOf(Block block) => switch (block) {
      CardioBlock(:final endTime) => endTime,
      StrengthBlock(:final endTime) => endTime,
      MetconBlock(:final endTime) => endTime,
      CooldownBlock(:final endTime) => endTime,
    };

double? _round(double? value, [int digits = 1]) {
  if (value == null) return null;
  final factor = [1, 10, 100, 1000][digits];
  return (value * factor).round() / factor;
}
