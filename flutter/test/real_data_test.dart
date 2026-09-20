import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:workout_log_core/workout_log_core.dart';
import 'package:workout_log/ui/cycle_table.dart';

import 'helpers.dart';

/// Drives the real shell over the real `data/` folder — the same check the
/// macOS run performs, minus the pixels, so a regression fails in CI rather
/// than on launch.
void main() {
  late Map<String, String> archive;

  setUpAll(() {
    final dir = Directory('../data');
    archive = {
      for (final file in dir.listSync().whereType<File>())
        if (file.path.endsWith('.json'))
          file.uri.pathSegments.last: file.readAsStringSync(),
    };
  });

  testWidgets('the session list shows every file in data/', (tester) async {
    final model = await memoryModel(tester, seed: archive);
    await tester.pumpWidget(appUnder(model));
    await tester.pumpAndSettle();

    expect(model.files, hasLength(archive.length));
    expect(model.files, isNotEmpty);
    await tester.tap(find.byTooltip('Open navigation menu'));
    await tester.pumpAndSettle();
    expect(find.text('2026-06-23_C2'), findsOneWidget);
  });

  testWidgets('opening a real session renders its blocks and muscle map',
      (tester) async {
    // A real session is nine blocks tall; the default 800x600 surface would let
    // the ListView skip building the ones past the fold.
    tester.view.physicalSize = const Size(1400, 3200);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    final model = await memoryModel(tester, seed: archive);
    await tester.pumpWidget(appUnder(model));
    await model.open('2026-06-23_C2.json');
    await tester.pumpAndSettle();

    expect(find.text('Cardio'), findsOneWidget);
    expect(find.text('Cooldown'), findsOneWidget);
    expect(find.text('присід фронтальний'), findsWidgets);
    // The ramp from the paper log, rendered as the monospaced set summary.
    expect(find.text('6×70   6×80   6×90   6×100   3×110'), findsOneWidget);
    expect(model.dayMapSVG(), isNotNull);
    expect(tester.takeException(), isNull);
  });

  testWidgets('the cycle screen builds a matrix and a calendar from data/',
      (tester) async {
    final model = await memoryModel(tester, seed: archive);
    await tester.pumpWidget(appUnder(model));
    await tester.pumpAndSettle();

    await tester.tap(find.byTooltip('Open navigation menu'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Cycle'));
    await tester.pumpAndSettle();

    expect(find.byType(CycleTable), findsOneWidget);
    expect(find.text('No sessions in this folder.'), findsNothing);
    expect(model.cycle.days, isNotEmpty);
    expect(model.cycle.exercises, contains('присід фронтальний'));
    expect(model.cycleMapSVG(), isNotNull);
    expect(tester.takeException(), isNull);
  });

  testWidgets('every session in data/ carries a dominant muscle group',
      (tester) async {
    final model = await memoryModel(tester, seed: archive);
    await tester.pumpWidget(appUnder(model));
    await tester.pumpAndSettle();

    final dated = model.calendar.values.where((d) => d.group != null);
    expect(dated, isNotEmpty);
    expect(model.calendar.keys, contains('2026-06-23'));
  });

  testWidgets('re-saving a real session does not change its meaning',
      (tester) async {
    final model = await memoryModel(tester, seed: archive);
    await tester.pumpWidget(appUnder(model));

    for (final id in archive.keys) {
      final before = SessionCoding.decode(archive[id]!);
      await model.open(id);
      await model.save();
      final after = SessionCoding.decode(await model.readRaw(id));
      expect(after, before, reason: id);
    }
  });
}
