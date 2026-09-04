import Foundation

/// Recolours an SVG template: every element tagged `data-muscle="<token>"` gets an
/// inline `fill` from its activation score. A `zeroColor` neutral marks unworked
/// muscles distinctly from "barely worked".
public enum MuscleMapSVG {

    public static func colorize(
        template: String,
        scores: [String: Double],
        lowColor: String = "#86b6ef",
        highColor: String = "#0d366b",
        zeroColor: String = "#e8e8e3"
    ) -> String {
        let regex = try! NSRegularExpression(pattern: #"data-muscle="([a-z_]+)""#)
        let ns = template as NSString
        let matches = regex.matches(in: template, range: NSRange(location: 0, length: ns.length))
        let out = NSMutableString(string: template)
        // Splice from the back so earlier insertions don't shift later ranges.
        for m in matches.reversed() {
            let token = ns.substring(with: m.range(at: 1))
            let fill = color(for: scores[token] ?? 0, low: lowColor, high: highColor, zero: zeroColor)
            out.insert(" style=\"fill:\(fill)\"", at: m.range.location + m.range.length)
        }
        return out as String
    }

    /// Blend the sequential ramp by score; score ≤ 0 → the neutral zero colour.
    public static func color(for score: Double, low: String, high: String, zero: String) -> String {
        guard score > 0 else { return zero }
        let t = min(max(score, 0), 1)
        let (r1, g1, b1) = rgb(low)
        let (r2, g2, b2) = rgb(high)
        let r = lerp(r1, r2, t), g = lerp(g1, g2, t), b = lerp(b1, b2, t)
        return String(format: "#%02x%02x%02x", r, g, b)
    }

    private static func lerp(_ a: Int, _ b: Int, _ t: Double) -> Int {
        Int((Double(a) + (Double(b) - Double(a)) * t).rounded())
    }

    private static func rgb(_ hex: String) -> (Int, Int, Int) {
        var h = hex
        if h.hasPrefix("#") { h.removeFirst() }
        let v = Int(h, radix: 16) ?? 0
        return ((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff)
    }
}
