/// The `HH:MM` bracket timestamps from the paper log, as minutes since midnight.
///
/// Times are wall clock without a date, so they are compared inside one session
/// only; a session that crosses midnight is not something the log records.
abstract final class WallClock {
  static final _pattern = RegExp(r'^(\d{1,2}):(\d{2})$');

  static int? minutes(String? text) {
    if (text == null) return null;
    final match = _pattern.firstMatch(text.trim());
    if (match == null) return null;
    final hour = int.parse(match[1]!);
    final minute = int.parse(match[2]!);
    if (hour > 23 || minute > 59) return null;
    return hour * 60 + minute;
  }

  static bool isValid(String? text) => text == null || minutes(text) != null;

  static String format(int minutes) =>
      '${(minutes ~/ 60).toString().padLeft(2, '0')}:'
      '${(minutes % 60).toString().padLeft(2, '0')}';
}
