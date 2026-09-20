import 'package:collection/collection.dart';

import '../models/work_set.dart';

/// Warn-never-block: a line we can't fully parse still yields the sets it can.
class ParsedSets {
  const ParsedSets({this.sets = const [], this.warnings = const []});

  final List<WorkSet> sets;
  final List<String> warnings;

  @override
  bool operator ==(Object other) =>
      other is ParsedSets &&
      const ListEquality<WorkSet>().equals(sets, other.sets) &&
      const ListEquality<String>().equals(warnings, other.warnings);

  @override
  int get hashCode => Object.hash(
        const ListEquality<WorkSet>().hash(sets),
        const ListEquality<String>().hash(warnings),
      );
}

/// Parses the terse paper-log notation into model sets. Pure (no UI, ADR-004);
/// see `docs/03-log-notation.md` for the full grammar.
///
/// Supported strength forms:
///   6 × [70, 80, 90, 100, 110]          fixed reps × ascending weights → 5 sets
///   6 × [70, 80] + 6 × [30]             trailing groups are back-off sets
///   6 × [30, 60, 80(4), 86(2), 90(1)]   per-set rep override in parentheses
///   5+5+4+3+3 (20)                      cluster set: chain + total in parens
///   4 × [54c, 40c, 36c, 42c]            timed holds (c/с = seconds) → duration sets
///   {60c, 70c, 30c, 35c}                bare bracketed list, no count prefix
abstract final class Notation {
  /// The Cyrillic `х`/`Х` and `с`/`С` are distinct code points from their Latin
  /// lookalikes; the paper log uses both, so both must be accepted.
  static const _multipliers = {'×', 'x', 'X', 'х', 'Х', '*'};
  static const _secondsSuffix = {'c', 'C', 'с', 'С'};

  static final _trailingParenNumber = RegExp(r'\(\s*\d+\s*\)\s*$');
  static final _nonDigit = RegExp(r'[^0-9]');
  static final _bracketTrim = RegExp(r'^[\[\]{} \t]+|[\[\]{} \t]+$');

  static ParsedSets parseStrengthSets(String raw) {
    final warnings = <String>[];
    final line = raw.trim();
    if (line.isEmpty) return const ParsedSets();

    final hasMultiplier = line.runes.any(_isMultiplier);
    final hasBracket = line.contains('[') || line.contains('{');

    if (!hasMultiplier && !hasBracket && line.contains('+')) {
      return _parseCluster(line, warnings);
    }

    // First group = ramp/straight sets, later top-level '+' groups = back-offs.
    final sets = <WorkSet>[];
    final groups = _splitTopLevel(line, '+');
    for (var i = 0; i < groups.length; i++) {
      sets.addAll(_parseGroup(groups[i], i > 0, warnings));
    }
    if (sets.isEmpty && warnings.isEmpty) {
      warnings.add('could not parse: $line');
    }
    return ParsedSets(sets: sets, warnings: warnings);
  }

  static ParsedSets _parseCluster(String line, List<String> warnings) {
    var body = line;
    int? total;
    final match = _trailingParenNumber.firstMatch(body);
    if (match != null) {
      total = int.parse(match[0]!.replaceAll(_nonDigit, ''));
      body = body.substring(0, match.start) + body.substring(match.end);
    }
    final parts = body
        .split('+')
        .where((p) => p.isNotEmpty)
        .map((p) => p.trim())
        .toList();
    final reps = parts.map(int.tryParse).whereType<int>().toList();
    if (reps.isEmpty || reps.length != parts.length) {
      warnings.add('could not parse cluster: $line');
      return ParsedSets(warnings: warnings);
    }
    final set = WorkSet(
      cluster: reps,
      totalReps: total ?? reps.reduce((a, b) => a + b),
    );
    return ParsedSets(sets: [set], warnings: warnings);
  }

  static List<WorkSet> _parseGroup(
    String group,
    bool isBackoff,
    List<String> warnings,
  ) {
    final runes = group.runes.toList();
    int? count;
    var listPart = group;

    final multiplierIndex = runes.indexWhere(_isMultiplier);
    if (multiplierIndex >= 0) {
      final countText = String.fromCharCodes(runes.take(multiplierIndex)).trim();
      final parsed = int.tryParse(countText);
      if (parsed != null) {
        count = parsed;
      } else {
        warnings.add('unrecognised rep count "$countText" in "$group"');
      }
      listPart = String.fromCharCodes(runes.skip(multiplierIndex + 1));
    }

    listPart = listPart.replaceAll(_bracketTrim, '');
    final items = listPart
        .split(',')
        .where((s) => s.isNotEmpty)
        .map((s) => s.trim())
        .where((s) => s.isNotEmpty)
        .toList();

    if (items.isEmpty) {
      warnings.add('no values in "$group"');
      return [];
    }

    final out = <WorkSet>[];
    for (final item in items) {
      final seconds = _parseDuration(item);
      if (seconds != null) {
        out.add(WorkSet(
          durationSec: seconds,
          isBackoff: isBackoff ? true : null,
        ));
        continue;
      }
      final weight = _parseWeight(item);
      if (weight != null) {
        out.add(WorkSet(
          weightKg: weight.$1,
          reps: weight.$2 ?? count,
          isBackoff: isBackoff ? true : null,
        ));
      } else {
        warnings.add('unparseable value "$item" in "$group"');
      }
    }
    return out;
  }

  /// "54c" / "54с" (Latin or Cyrillic suffix) → 54 seconds; null if not a duration token.
  static double? _parseDuration(String token) {
    if (token.isEmpty) return null;
    final last = String.fromCharCode(token.runes.last);
    if (!_secondsSuffix.contains(last)) return null;
    final number = token
        .substring(0, token.length - last.length)
        .trim()
        .replaceAll(',', '.');
    return double.tryParse(number);
  }

  /// "90" or "90(2)" or "47.5" → (weight, optional rep override).
  static (double, int?)? _parseWeight(String token) {
    var text = token;
    int? reps;
    final match = _trailingParenNumber.firstMatch(text);
    if (match != null) {
      reps = int.parse(match[0]!.replaceAll(_nonDigit, ''));
      text = text.substring(0, match.start) + text.substring(match.end);
    }
    text = text.trim().replaceAll(',', '.');
    final weight = double.tryParse(text);
    return weight == null ? null : (weight, reps);
  }

  /// Split on `separator` at bracket depth 0 (so weights inside [...] stay intact).
  static List<String> _splitTopLevel(String input, String separator) {
    final result = <String>[];
    final current = StringBuffer();
    var depth = 0;
    for (final rune in input.runes) {
      final ch = String.fromCharCode(rune);
      if (ch == '[' || ch == '{') {
        depth += 1;
        current.write(ch);
      } else if (ch == ']' || ch == '}') {
        depth = depth > 0 ? depth - 1 : 0;
        current.write(ch);
      } else if (ch == separator && depth == 0) {
        result.add(current.toString());
        current.clear();
      } else {
        current.write(ch);
      }
    }
    result.add(current.toString());
    return result.map((s) => s.trim()).where((s) => s.isNotEmpty).toList();
  }

  static bool _isMultiplier(int rune) =>
      _multipliers.contains(String.fromCharCode(rune));
}
