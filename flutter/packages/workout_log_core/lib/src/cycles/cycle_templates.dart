import '../models/cycle.dart';
import 'cycle_day.dart';

/// The block skeleton a new workout starts from, chosen by its number.
///
/// Lifted from the shape of the hybrid-8 cycle rather than invented: a `1` day
/// is a conditioning day (short warm-up, explosive lift, metcon) and a `2` day
/// is a heavy day (longer warm-up, hyperextension, a main lift, accessories).
/// Every field stays editable afterwards.
abstract final class CycleTemplates {
  static List<BlockTemplate> blocksFor(DayKind kind) => switch (kind) {
        DayKind.conditioning => [
            BlockTemplate(type: 'cardio', role: 'warmup', machine: '', durationMin: 10),
            BlockTemplate(type: 'strength', role: 'explosive'),
            BlockTemplate(type: 'metcon', role: 'metcon'),
            BlockTemplate(type: 'strength', role: 'accessory', setsReps: [6, 6, 6, 6]),
            BlockTemplate(type: 'strength', role: 'grip', setsReps: [8, 8, 8, 8]),
            BlockTemplate(type: 'cooldown', role: 'cooldown'),
          ],
        DayKind.heavy => [
            BlockTemplate(type: 'cardio', role: 'warmup', machine: '', durationMin: 15),
            BlockTemplate(
              type: 'strength',
              role: 'warmup',
              exercise: 'гіперекстензія',
              setsReps: [12, 12],
            ),
            BlockTemplate(type: 'strength', role: 'main'),
            BlockTemplate(type: 'strength', role: 'accessory', setsReps: [6, 6, 6, 6, 6]),
            BlockTemplate(type: 'strength', role: 'accessory', setsReps: [6, 6, 6, 6, 6]),
            BlockTemplate(type: 'strength', role: 'grip', setsReps: [8, 8, 8, 8]),
            BlockTemplate(type: 'cooldown', role: 'cooldown'),
          ],
      };

  /// A fresh workout for `day`, pre-filled and slotted after `previous`.
  static CycleSession session(CycleDay day, {int? week, String? weekday}) =>
      CycleSession(
        cycleDay: day.code,
        week: week,
        weekday: weekday,
        type: day.kind == DayKind.conditioning ? 'metcon' : 'heavy',
        blocks: CycleTemplates.blocksFor(day.kind),
      );

  /// The code a new workout should get: one past the highest that follows the
  /// convention, or `A1` when the cycle is empty or entirely free-form.
  static CycleDay nextDay(Iterable<String> existingCodes) {
    final parsed = existingCodes
        .map(CycleDay.tryParse)
        .whereType<CycleDay>()
        .toList()
      ..sort();
    return parsed.isEmpty ? const CycleDay('A', DayKind.conditioning) : parsed.last.next;
  }
}
