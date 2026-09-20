/// Recolours an SVG template: every element tagged `data-muscle="<token>"` gets an
/// inline `fill` from its activation score. A `zeroColor` neutral marks unworked
/// muscles distinctly from "barely worked".
abstract final class MuscleMapSVG {
  static const lowColor = '#86b6ef';
  static const highColor = '#0d366b';
  static const zeroColor = '#e8e8e3';

  static final _muscleAttribute = RegExp(r'data-muscle="([a-z_]+)"');

  static String colorize(
    String template,
    Map<String, double> scores, {
    String low = lowColor,
    String high = highColor,
    String zero = zeroColor,
  }) {
    final out = StringBuffer();
    var cursor = 0;
    for (final match in _muscleAttribute.allMatches(template)) {
      final token = match[1]!;
      final fill = color(scores[token] ?? 0, low: low, high: high, zero: zero);
      out
        ..write(template.substring(cursor, match.end))
        ..write(' style="fill:$fill"');
      cursor = match.end;
    }
    out.write(template.substring(cursor));
    return out.toString();
  }

  /// Blend the sequential ramp by score; score ≤ 0 → the neutral zero colour.
  static String color(
    double score, {
    String low = lowColor,
    String high = highColor,
    String zero = zeroColor,
  }) {
    if (score <= 0) return zero;
    final t = score.clamp(0.0, 1.0);
    final (r1, g1, b1) = _rgb(low);
    final (r2, g2, b2) = _rgb(high);
    return '#${_hex(_lerp(r1, r2, t))}'
        '${_hex(_lerp(g1, g2, t))}'
        '${_hex(_lerp(b1, b2, t))}';
  }

  static int _lerp(int a, int b, double t) => (a + (b - a) * t).round();

  static String _hex(int value) => value.toRadixString(16).padLeft(2, '0');

  static (int, int, int) _rgb(String hex) {
    final h = hex.startsWith('#') ? hex.substring(1) : hex;
    final v = int.tryParse(h, radix: 16) ?? 0;
    return ((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
  }
}
