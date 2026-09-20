import 'dart:convert';

import 'package:collection/collection.dart';

import 'models/exercise.dart';
import 'models/session.dart';

/// Pretty-printed with sorted keys so on-disk files produce clean, stable git diffs.
abstract final class SessionCoding {
  static const _encoder = JsonEncoder.withIndent('  ');

  static Session decode(String text) =>
      Session.fromJson(jsonDecode(text) as Map<String, dynamic>);

  static String encode(Session session) => encodeJson(session.toJson());

  static Catalogue decodeCatalogue(String text) =>
      Catalogue.fromJson(jsonDecode(text) as Map<String, dynamic>);

  /// Swift's `JSONEncoder` sorted keys and wrote no trailing newline; Dart's keeps
  /// insertion order and we append one, so files stay POSIX- and git-friendly.
  static String encodeJson(Map<String, dynamic> json) =>
      '${_encoder.convert(canonicalJson(json))}\n';
}

/// Sorts every nested map by key and narrows integral doubles to ints.
///
/// The narrowing matters: Swift wrote `Double(70)` as `70`, Dart would write
/// `70.0`, so without it every weight in `data/` would gain a `.0` on first save.
Object? canonicalJson(Object? value) {
  if (value is Map) {
    final keys = value.keys.cast<String>().toList()..sort();
    return <String, dynamic>{for (final k in keys) k: canonicalJson(value[k])};
  }
  if (value is List) return value.map(canonicalJson).toList();
  if (value is double && value.isFinite && value == value.roundToDouble()) {
    return value.toInt();
  }
  return value;
}

Map<String, dynamic> jsonMap(String text) =>
    jsonDecode(text) as Map<String, dynamic>;

/// Optional fields stay absent rather than `null`, matching `encodeIfPresent`.
void put(Map<String, dynamic> json, String key, Object? value) {
  if (value != null) json[key] = value;
}

double? asDouble(Object? value) => (value as num?)?.toDouble();

int? asInt(Object? value) => (value as num?)?.toInt();

List<int>? asIntList(Object? value) => value == null
    ? null
    : [for (final e in value as List) (e as num).toInt()];

List<String> asStringList(Object? value) => value == null
    ? const []
    : [for (final e in value as List) e as String];

/// Structural equality over `toJson()`.
///
/// The models are lossless projections of the files (ADR-001), so JSON equality
/// *is* model equality — and this avoids hand-maintaining `==`/`hashCode` for a
/// dozen classes whose only job is to round-trip.
mixin JsonEquality {
  static const _deep = DeepCollectionEquality();

  Map<String, dynamic> toJson();

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is JsonEquality &&
          other.runtimeType == runtimeType &&
          _deep.equals(toJson(), other.toJson());

  @override
  int get hashCode => _deep.hash(toJson());
}
