import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:vector_graphics_codec/vector_graphics_codec.dart';
import 'package:vector_graphics_compiler/vector_graphics_compiler.dart' as vg;
import 'package:workout_log_core/workout_log_core.dart';

/// Risk check for the template: `flutter_svg` supports CSS only partly, so the
/// styling was inlined as presentation attributes. This runs the same encode
/// that `SvgPicture.string` runs at load time and reads back the paints, so a
/// regression in either the template or `colorize` fails here rather than
/// silently rendering a grey figure.
void main() {
  late String template;

  setUpAll(() => template = File('assets/muscle-map.svg').readAsStringSync());

  Set<int> paintColors(String svg) {
    // Optimizer flags match flutter_svg's own loader, which disables all three
    // so the encode stays pure Dart (they need the native PathOps library).
    final bytes = vg.encodeSvg(
      xml: svg,
      debugName: 'muscle-map',
      enableClippingOptimizer: false,
      enableMaskingOptimizer: false,
      enableOverdrawOptimizer: false,
    );
    final listener = _PaintCollector();
    const VectorGraphicsCodec().decode(
      ByteData.sublistView(bytes),
      listener,
    );
    return listener.colors;
  }

  test('the uncoloured template compiles with its default fills', () {
    final colors = paintColors(template);
    expect(colors, contains(0xffe8e8e3), reason: 'unworked muscle fill');
    expect(colors, contains(0xfff4f4f0), reason: 'silhouette fill');
    expect(colors, contains(0xff2c3437), reason: 'outline stroke');
  });

  test('colorize fills reach the renderer, overriding the defaults', () {
    final colors =
        paintColors(MuscleMapSVG.colorize(template, {'quads': 1.0, 'chest': 0.5}));
    expect(colors, contains(0xff0d366b), reason: 'peak muscle');
    expect(colors, contains(0xff4a76ad), reason: 'mid-ramp muscle');
    expect(colors, contains(0xffe8e8e3), reason: 'unworked muscles');
  });

  test('a fully unworked map is entirely the zero colour', () {
    final colors = paintColors(MuscleMapSVG.colorize(template, const {}));
    expect(colors, contains(0xffe8e8e3));
    expect(colors, isNot(contains(0xff0d366b)));
  });
}

class _PaintCollector implements VectorGraphicsCodecListener {
  final Set<int> colors = {};

  @override
  void onPaintObject({
    required int color,
    required int? strokeCap,
    required int? strokeJoin,
    required int blendMode,
    required double? strokeMiterLimit,
    required double? strokeWidth,
    required int paintStyle,
    required int id,
    required int? shaderId,
  }) {
    colors.add(color);
  }

  @override
  void noSuchMethod(Invocation invocation) {}
}
