import 'package:test/test.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'fixtures.dart';

void main() {
  late String template;
  late Catalogue catalogue;

  setUpAll(() {
    template = loadMapTemplate();
    catalogue = loadCatalogue();
  });

  test('the template covers every muscle the catalogue uses', () {
    final used = {
      for (final e in catalogue.exercises)
        ...[...e.primaryMuscles, ...e.secondaryMuscles],
    };
    final inTemplate = RegExp(r'data-muscle="([a-z_]+)"')
        .allMatches(template)
        .map((m) => m[1]!)
        .toSet();
    expect(used.difference(inTemplate).toList()..sort(), isEmpty);
  });

  test('peak and unworked muscles get the ramp endpoints', () {
    final svg = MuscleMapSVG.colorize(template, {'quads': 1.0, 'chest': 0.4});
    expect(svg, contains('data-muscle="quads" style="fill:#0d366b"'));
    expect(svg, contains('data-muscle="forearms" style="fill:#e8e8e3"'));
  });

  test('every tagged element is given exactly one inline fill', () {
    final tagged = RegExp(r'data-muscle="[a-z_]+"').allMatches(template).length;
    final svg = MuscleMapSVG.colorize(template, const {});
    expect(RegExp(r'style="fill:#[0-9a-f]{6}"').allMatches(svg).length, tagged);
  });

  test('the ramp endpoints and midpoint are exact hexes', () {
    expect(MuscleMapSVG.color(0), '#e8e8e3');
    expect(MuscleMapSVG.color(-1), '#e8e8e3');
    expect(MuscleMapSVG.color(1), '#0d366b');
    expect(MuscleMapSVG.color(2), '#0d366b', reason: 'scores clamp at 1');
    expect(MuscleMapSVG.color(0.000001), '#86b6ef');
    expect(MuscleMapSVG.color(0.5), '#4a76ad');
  });

  test('colorize leaves the rest of the template byte-identical', () {
    final svg = MuscleMapSVG.colorize(template, const {});
    expect(svg.replaceAll(RegExp(r' style="fill:#[0-9a-f]{6}"'), ''), template);
  });
}
