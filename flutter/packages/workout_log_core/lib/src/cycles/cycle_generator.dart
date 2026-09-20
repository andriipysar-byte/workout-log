import '../models/block.dart';
import '../models/cycle.dart';
import '../models/session.dart';
import '../models/work_set.dart';

class CycleGeneratorException implements Exception {
  CycleGeneratorException(this.message);

  final String message;

  @override
  String toString() => message;
}

/// Expands a cycle template onto the real calendar, producing one schema-valid
/// session stub per template session. The planning-only `role` and `sets_reps`
/// fields are consumed here and never reach the output.
abstract final class CycleGenerator {
  static const weekdayAbbreviations = [
    'Mon',
    'Tue',
    'Wed',
    'Thu',
    'Fri',
    'Sat',
    'Sun',
  ];

  /// The first `count` dates on/after `start` whose weekday is a training day.
  static List<DateTime> trainingDates(
    DateTime start,
    List<String> trainingDays,
    int count,
  ) {
    final valid = trainingDays.where(weekdayAbbreviations.contains).toSet();
    if (valid.isEmpty && count > 0) {
      throw CycleGeneratorException(
        'no recognised training days in $trainingDays '
        '(expected any of $weekdayAbbreviations)',
      );
    }
    final out = <DateTime>[];
    var day = DateTime(start.year, start.month, start.day);
    while (out.length < count) {
      if (valid.contains(weekdayAbbreviations[day.weekday - 1])) out.add(day);
      // Stepping via the constructor rather than `Duration(days: 1)` keeps this
      // on calendar days: adding 24h across a DST boundary lands on the wrong one.
      day = DateTime(day.year, day.month, day.day + 1);
    }
    return out;
  }

  static List<Session> generate(Cycle cycle) {
    final start = DateTime.parse(cycle.startDate);
    final dates = trainingDates(
      start,
      cycle.trainingDays,
      cycle.sessions.length,
    );
    return [
      for (var i = 0; i < cycle.sessions.length; i++)
        sessionFromTemplate(cycle.sessions[i], dates[i]),
    ];
  }

  /// `enforceWeekday` is what separates the two callers: expanding a whole cycle
  /// must fail loudly when the calendar has drifted, but a person creating one
  /// session on a day that suits them is not an error.
  static Session sessionFromTemplate(
    CycleSession template,
    DateTime when, {
    bool enforceWeekday = true,
  }) {
    final actual = weekdayAbbreviations[when.weekday - 1];
    final expected = template.weekday;
    if (enforceWeekday && expected != null && expected != actual) {
      throw CycleGeneratorException(
        '${template.cycleDay}: calendar says $actual but template says $expected',
      );
    }
    return Session(
      date: isoDate(when),
      cycleDay: template.cycleDay,
      notes: (template.sessionNotes?.isEmpty ?? true)
          ? null
          : template.sessionNotes,
      blocks: [for (final b in template.blocks) blockFromTemplate(b)],
    );
  }

  static Block blockFromTemplate(BlockTemplate template) {
    final notes =
        (template.notes?.isEmpty ?? true) ? null : template.notes;
    switch (template.type) {
      case 'cardio':
        return CardioBlock(
          machine: template.machine ?? '',
          durationMin: template.durationMin,
        );
      case 'strength':
        final exercise = template.exercise;
        if (exercise == null || exercise.isEmpty) {
          throw CycleGeneratorException(
            'a strength block in this cycle has no exercise yet',
          );
        }
        return StrengthBlock(
          exercise: exercise,
          sets: [for (final reps in template.setsReps) WorkSet(reps: reps)],
          notes: notes,
        );
      case 'metcon':
        return MetconBlock(
          format: template.format,
          scheme: template.scheme,
          exercises: [for (final e in template.exercises) e],
          notes: notes,
        );
      case 'cooldown':
        return CooldownBlock();
      default:
        throw CycleGeneratorException(
          'unknown block type: "${template.type}"',
        );
    }
  }

  /// A session built from a template for display only, skipping blocks that are
  /// still blank.
  ///
  /// Separate from [generate] on purpose: writing a half-finished plan to a
  /// session file should fail loudly, but showing the user a muscle map of the
  /// plan they are part-way through writing should not.
  static Session preview(CycleSession template, {DateTime? on}) {
    final when = on ?? DateTime(2000);
    final blocks = <Block>[];
    for (final block in template.blocks) {
      try {
        final built = blockFromTemplate(block);
        // A planned main lift carries notes but no `sets_reps`, so by volume it
        // would score zero and vanish from the day's muscle map — the one
        // exercise the day is built around. In a plan, choosing an exercise is
        // itself the commitment, so it counts as one set.
        if (built is StrengthBlock && built.sets.isEmpty) {
          built.sets = [WorkSet()];
        }
        blocks.add(built);
      } on CycleGeneratorException {
        continue;
      }
    }
    return Session(
      date: isoDate(when),
      cycleDay: template.cycleDay,
      blocks: blocks,
    );
  }

  static String isoDate(DateTime when) =>
      '${when.year.toString().padLeft(4, '0')}-'
      '${when.month.toString().padLeft(2, '0')}-'
      '${when.day.toString().padLeft(2, '0')}';
}
