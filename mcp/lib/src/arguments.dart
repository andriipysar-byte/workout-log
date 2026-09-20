import 'dart:convert';

import 'package:dart_mcp/server.dart';

/// A bad argument or a refused write: reported back as tool output rather than
/// as a protocol error, so the model can read what went wrong and correct it.
class ToolFailure implements Exception {
  ToolFailure(this.message);

  final String message;

  @override
  String toString() => message;
}

extension ToolArguments on CallToolRequest {
  Map<String, Object?> get args => arguments ?? const {};

  String requireString(String key) {
    final value = string(key);
    if (value == null) throw ToolFailure('"$key" is required');
    return value;
  }

  String? string(String key) {
    final value = args[key];
    if (value == null) return null;
    if (value is! String) throw ToolFailure('"$key" must be a string');
    final trimmed = value.trim();
    return trimmed.isEmpty ? null : trimmed;
  }

  /// Dates are compared as text everywhere (ISO 8601 sorts lexicographically),
  /// so the format is checked once, here, rather than trusted downstream.
  String? date(String key) {
    final value = string(key);
    if (value == null) return null;
    if (!RegExp(r'^\d{4}-\d{2}-\d{2}$').hasMatch(value) ||
        DateTime.tryParse(value) == null) {
      throw ToolFailure('"$key" must be an ISO date (YYYY-MM-DD), got "$value"');
    }
    return value;
  }

  int? integer(String key) {
    final value = args[key];
    if (value == null) return null;
    if (value is int) return value;
    if (value is num && value == value.roundToDouble()) return value.toInt();
    if (value is String) {
      final parsed = int.tryParse(value);
      if (parsed != null) return parsed;
    }
    throw ToolFailure('"$key" must be a whole number');
  }

  double? number(String key) {
    final value = args[key];
    if (value == null) return null;
    if (value is num) return value.toDouble();
    if (value is String) {
      final parsed = double.tryParse(value);
      if (parsed != null) return parsed;
    }
    throw ToolFailure('"$key" must be a number');
  }

  bool flag(String key, {bool fallback = false}) {
    final value = args[key];
    if (value == null) return fallback;
    if (value is bool) return value;
    throw ToolFailure('"$key" must be true or false');
  }

  List<String> strings(String key) {
    final value = args[key];
    if (value == null) return const [];
    if (value is! List) throw ToolFailure('"$key" must be a list of strings');
    return [
      for (final entry in value)
        if (entry is String) entry.trim() else throw ToolFailure('"$key" must be a list of strings'),
    ];
  }

  Map<String, dynamic> requireObject(String key) {
    final value = args[key];
    if (value is! Map) throw ToolFailure('"$key" must be a JSON object');
    return Map<String, dynamic>.from(value);
  }

  /// One of [values], or [fallback] when absent.
  T enumeration<T>(String key, Map<String, T> values, T fallback) {
    final value = string(key);
    if (value == null) return fallback;
    final match = values[value];
    if (match == null) {
      throw ToolFailure(
        '"$key" must be one of ${values.keys.join(', ')}, got "$value"',
      );
    }
    return match;
  }
}

const _encoder = JsonEncoder.withIndent('  ');

/// Tool results travel as pretty JSON text.
///
/// Integral doubles are narrowed on the way out for the same reason the on-disk
/// encoder does it: `70` rather than `70.0` is what the log says, and the model
/// echoes what it reads back into the next write.
String jsonText(Object? value) => _encoder.convert(_plain(value));

Object? _plain(Object? value) {
  if (value is Map) {
    return {
      for (final entry in value.entries) '${entry.key}': _plain(entry.value),
    };
  }
  if (value is List) return value.map(_plain).toList();
  if (value is double && value.isFinite && value == value.roundToDouble()) {
    return value.toInt();
  }
  return value;
}
