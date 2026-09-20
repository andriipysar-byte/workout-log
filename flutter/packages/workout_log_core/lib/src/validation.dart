import 'cycles/cycle_day.dart';
import 'models/block.dart';
import 'models/exercise.dart';
import 'models/session.dart';
import 'wall_clock.dart';

enum IssueSeverity { error, warning }

/// One problem found in a session, addressed to a JSON path so a caller can say
/// exactly which block it means.
class ValidationIssue {
  const ValidationIssue(this.severity, this.path, this.message);

  const ValidationIssue.error(String path, String message)
      : this(IssueSeverity.error, path, message);

  const ValidationIssue.warning(String path, String message)
      : this(IssueSeverity.warning, path, message);

  final IssueSeverity severity;
  final String path;
  final String message;

  Map<String, dynamic> toJson() => {
        'severity': severity.name,
        'path': path,
        'message': message,
      };

  @override
  String toString() => '${severity.name}: $path — $message';
}

/// Checks a session against `session.schema.json` and against what the log
/// means, which the schema cannot express.
///
/// Warn, never block (ADR-006): a half-written plan is a normal state of a file
/// and must stay writable, so only what would corrupt the archive is an error.
abstract final class SessionValidator {
  static final _date = RegExp(r'^\d{4}-\d{2}-\d{2}$');

  static List<ValidationIssue> validate(
    Session session, {
    Catalogue? catalogue,
  }) {
    final issues = <ValidationIssue>[];

    if (!_date.hasMatch(session.date) ||
        DateTime.tryParse(session.date) == null) {
      issues.add(ValidationIssue.error(
        'date',
        '"${session.date}" is not an ISO date (YYYY-MM-DD)',
      ));
    }
    if (session.cycleDay.trim().isEmpty) {
      issues.add(const ValidationIssue.error('cycle_day', 'is empty'));
    } else if (CycleDay.tryParse(session.cycleDay) == null) {
      issues.add(ValidationIssue.warning(
        'cycle_day',
        '"${session.cycleDay}" does not follow the A1…F2 convention',
      ));
    }
    if (!WallClock.isValid(session.startTime)) {
      issues.add(ValidationIssue.error(
        'start_time',
        '"${session.startTime}" is not a wall-clock time (HH:MM)',
      ));
    }
    if ((session.bodyweightKg ?? 1) <= 0) {
      issues.add(const ValidationIssue.error('bodyweight_kg', 'must be positive'));
    }
    if (session.blocks.isEmpty) {
      issues.add(const ValidationIssue.warning('blocks', 'the session is empty'));
    }

    var previousEnd = WallClock.minutes(session.startTime);
    for (var i = 0; i < session.blocks.length; i++) {
      final block = session.blocks[i];
      final path = 'blocks[$i]';
      final end = _checkTimes(block, path, previousEnd, issues);
      previousEnd = end ?? previousEnd;

      switch (block) {
        case CardioBlock(:final machine, :final durationMin, :final distanceM):
          if (machine.trim().isEmpty) {
            issues.add(ValidationIssue.warning(
              '$path.machine',
              'no machine recorded',
            ));
          }
          if ((durationMin ?? 1) <= 0) {
            issues.add(ValidationIssue.error(
              '$path.duration_min',
              'must be positive',
            ));
          }
          if ((distanceM ?? 1) <= 0) {
            issues.add(ValidationIssue.error(
              '$path.distance_m',
              'must be positive',
            ));
          }
        case StrengthBlock(:final exercise, :final sets):
          if (exercise.trim().isEmpty) {
            issues.add(ValidationIssue.error('$path.exercise', 'is empty'));
          } else {
            _checkKnown(exercise, '$path.exercise', catalogue, issues);
          }
          if (sets.isEmpty) {
            issues.add(ValidationIssue.warning(
              '$path.sets',
              'no sets recorded for "$exercise"',
            ));
          }
          for (var s = 0; s < sets.length; s++) {
            final set = sets[s];
            final setPath = '$path.sets[$s]';
            if ((set.weightKg ?? 0) < 0) {
              issues.add(ValidationIssue.error('$setPath.weight_kg', 'is negative'));
            }
            if ((set.reps ?? 1) <= 0) {
              issues.add(ValidationIssue.error('$setPath.reps', 'must be positive'));
            }
            if ((set.durationSec ?? 1) <= 0) {
              issues.add(ValidationIssue.error(
                '$setPath.duration_sec',
                'must be positive',
              ));
            }
            final cluster = set.cluster;
            if (cluster != null && cluster.any((r) => r <= 0)) {
              issues.add(ValidationIssue.error(
                '$setPath.cluster',
                'every rep in the chain must be positive',
              ));
            }
            if (cluster != null && set.totalReps != null) {
              final sum = cluster.fold<int>(0, (a, b) => a + b);
              if (sum != set.totalReps) {
                issues.add(ValidationIssue.warning(
                  '$setPath.total_reps',
                  '${set.totalReps} does not match the chain sum $sum',
                ));
              }
            }
          }
        case MetconBlock(:final scheme, :final exercises, :final rounds):
          if (exercises.isEmpty) {
            issues.add(ValidationIssue.warning(
              '$path.exercises',
              'the metcon has no movements',
            ));
          }
          for (var e = 0; e < exercises.length; e++) {
            _checkKnown(
              exercises[e].name,
              '$path.exercises[$e].name',
              catalogue,
              issues,
            );
          }
          if (scheme != null && rounds != null && rounds.length > scheme.length) {
            issues.add(ValidationIssue.warning(
              '$path.rounds',
              '${rounds.length} rounds recorded against a '
                  '${scheme.length}-round scheme',
            ));
          }
          double? previousSplit;
          for (var r = 0; r < (rounds?.length ?? 0); r++) {
            final round = rounds![r];
            final split = round.splitCumulativeSec;
            if (split != null && previousSplit != null && split < previousSplit) {
              issues.add(ValidationIssue.warning(
                '$path.rounds[$r].split_cumulative_sec',
                'goes backwards: $split after $previousSplit — splits are '
                    'cumulative, not per round',
              ));
            }
            if (split != null) previousSplit = split;
            if (round.round != r + 1) {
              issues.add(ValidationIssue.warning(
                '$path.rounds[$r].round',
                'is ${round.round} at position ${r + 1}',
              ));
            }
          }
        case CooldownBlock():
          break;
      }
    }
    return issues;
  }

  static bool hasErrors(List<ValidationIssue> issues) =>
      issues.any((i) => i.severity == IssueSeverity.error);

  static int? _checkTimes(
    Block block,
    String path,
    int? previousEnd,
    List<ValidationIssue> issues,
  ) {
    String? start;
    String? end;
    switch (block) {
      case StrengthBlock():
        start = block.startTime;
        end = block.endTime;
      case MetconBlock():
        start = block.startTime;
        end = block.endTime;
      case CardioBlock():
        end = block.endTime;
      case CooldownBlock():
        end = block.endTime;
    }
    for (final (key, value) in [('start_time', start), ('end_time', end)]) {
      if (!WallClock.isValid(value)) {
        issues.add(ValidationIssue.error(
          '$path.$key',
          '"$value" is not a wall-clock time (HH:MM)',
        ));
      }
    }
    final startMin = WallClock.minutes(start);
    final endMin = WallClock.minutes(end);
    if (startMin != null && endMin != null && endMin < startMin) {
      issues.add(ValidationIssue.warning(
        '$path.end_time',
        'ends before it starts',
      ));
    }
    final firstMin = startMin ?? endMin;
    if (previousEnd != null && firstMin != null && firstMin < previousEnd) {
      issues.add(ValidationIssue.warning(
        path,
        'starts before the previous block ended '
            '(${WallClock.format(firstMin)} after ${WallClock.format(previousEnd)})',
      ));
    }
    return endMin;
  }

  static void _checkKnown(
    String name,
    String path,
    Catalogue? catalogue,
    List<ValidationIssue> issues,
  ) {
    if (catalogue == null || name.trim().isEmpty) return;
    if (catalogue.resolve(name) != null) return;
    issues.add(ValidationIssue.warning(
      path,
      '"$name" is in no catalogue entry — add it to exercises.json or it '
          'contributes nothing to the muscle map',
    ));
  }
}
