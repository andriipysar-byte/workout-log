import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';

/// The colouring is done in the core (ADR-004) and handed here as a finished
/// SVG string, which is also what makes the map work on all six platforms —
/// the macOS build embedded a WKWebView to do this.
class MuscleMap extends StatelessWidget {
  /// A null [height] lets the map fill its parent instead of a fixed box.
  const MuscleMap({
    required this.svg,
    this.height = 280,
    this.alignment = Alignment.center,
    super.key,
  });

  final String svg;
  final double? height;

  /// Where the figure sits when the box is taller than its 500×640 aspect —
  /// centred leaves a gap under the heading in a full-height column.
  final Alignment alignment;

  @override
  Widget build(BuildContext context) => SizedBox(
        height: height,
        // No spinner while the SVG decodes: it is fast enough that one would
        // only flicker, and an indeterminate animation means a widget test can
        // never reach a settled frame.
        child: SvgPicture.string(svg, fit: BoxFit.contain, alignment: alignment),
      );
}

class MuscleMapUnavailable extends StatelessWidget {
  const MuscleMapUnavailable(this.reason, {super.key});

  final String reason;

  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.symmetric(vertical: 8),
        child: Text(
          reason,
          style: Theme.of(context).textTheme.bodySmall,
        ),
      );
}
