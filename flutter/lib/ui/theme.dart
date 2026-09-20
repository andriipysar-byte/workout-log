import 'package:flutter/material.dart';
import 'package:workout_log_core/workout_log_core.dart';

/// Below this the sidebar cannot sit beside the detail pane, so it becomes a
/// drawer — this is what makes the phone and browser targets real.
const wideLayoutBreakpoint = 840.0;

const sidebarWidth = 240.0;

/// The calendar's badge colour for a day whose dominant muscle group is unknown.
const unclassifiedDayColor = Color(0xff2a78d6);

Color colorFromHex(String hex) {
  final normalized = hex.startsWith('#') ? hex.substring(1) : hex;
  return Color(0xff000000 | (int.tryParse(normalized, radix: 16) ?? 0));
}

extension MuscleGroupColor on MuscleGroup {
  Color get color => colorFromHex(hex);
}

/// A light wash of an exercise's primary muscle-group colour(s); transparent
/// when the exercise is not in the catalogue.
LinearGradient groupGradient(List<MuscleGroup> groups, double base) {
  final colors = switch (groups.length) {
    0 => const [Colors.transparent, Colors.transparent],
    1 => [
        groups[0].color.withValues(alpha: base * 1.4),
        groups[0].color.withValues(alpha: base * 0.5),
      ],
    _ => [for (final g in groups) g.color.withValues(alpha: base)],
  };
  return LinearGradient(
    colors: colors,
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
  );
}

ThemeData buildTheme(Brightness brightness) {
  final scheme = ColorScheme.fromSeed(
    seedColor: unclassifiedDayColor,
    brightness: brightness,
  );
  return ThemeData(
    colorScheme: scheme,
    useMaterial3: true,
    visualDensity: VisualDensity.compact,
    inputDecorationTheme: const InputDecorationTheme(
      isDense: true,
      border: OutlineInputBorder(),
      contentPadding: EdgeInsets.symmetric(horizontal: 8, vertical: 8),
    ),
  );
}

/// The AppKit `controlBackgroundColor` the cards used on macOS.
Color cardSurface(BuildContext context) =>
    Theme.of(context).colorScheme.surfaceContainerHighest;
