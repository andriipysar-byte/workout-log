import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:workout_log_core/workout_log_core.dart';
import 'package:workout_log/ui/block_cards.dart';

import 'helpers.dart';

void main() {
  testWidgets('the preview is green when the line parses', (tester) async {
    final model = await memoryModel(tester);
    final block = StrengthBlock(exercise: 'присід фронтальний');
    await tester.pumpWidget(hostedIn(
      model,
      StrengthEditor(block: block, onChanged: () {}),
    ));

    await tester.enterText(find.byType(TextField), '6 × [70, 80]');
    await tester.pump();

    final preview = tester.widget<Text>(find.textContaining('→ '));
    expect(preview.data, contains('6×70'));
    expect(preview.style?.color, Colors.green.shade700);
    expect(find.byIcon(Icons.warning_amber), findsNothing);
  });

  testWidgets('the preview turns red when the line does not parse',
      (tester) async {
    final model = await memoryModel(tester);
    final block = StrengthBlock(exercise: 'присід фронтальний');
    await tester.pumpWidget(hostedIn(
      model,
      StrengthEditor(block: block, onChanged: () {}),
    ));

    await tester.enterText(find.byType(TextField), 'нічого корисного');
    await tester.pump();

    final preview = tester.widget<Text>(find.textContaining('→ '));
    expect(preview.data, '→ —');
    expect(preview.style?.color, Colors.red);
  });

  testWidgets('a partly-parseable line shows sets and a warning row',
      (tester) async {
    final model = await memoryModel(tester);
    final block = StrengthBlock(exercise: 'присід фронтальний');
    await tester.pumpWidget(hostedIn(
      model,
      StrengthEditor(block: block, onChanged: () {}),
    ));

    await tester.enterText(find.byType(TextField), '6 × [70, банан]');
    await tester.pump();

    expect(find.byIcon(Icons.warning_amber), findsOneWidget);
    expect(find.textContaining('банан'), findsWidgets);
  });

  testWidgets('Apply writes the parsed sets into the block', (tester) async {
    final model = await memoryModel(tester);
    final block = StrengthBlock(exercise: 'присід фронтальний');
    var edits = 0;
    await tester.pumpWidget(hostedIn(
      model,
      StrengthEditor(block: block, onChanged: () => edits++),
    ));

    await tester.enterText(find.byType(TextField), '6 × [70, 80] + 6 × [30]');
    await tester.pump();
    await tester.tap(find.text('Apply'));
    await tester.pump();

    expect(block.sets.map((s) => s.weightKg), [70, 80, 30]);
    expect(block.sets.last.isBackoff, isTrue);
    expect(edits, 1);
    expect(find.text('6×70   6×80   6×30*'), findsOneWidget);
  });

  testWidgets('Apply is disabled while nothing parses', (tester) async {
    final model = await memoryModel(tester);
    await tester.pumpWidget(hostedIn(
      model,
      StrengthEditor(
        block: StrengthBlock(exercise: 'присід фронтальний'),
        onChanged: () {},
      ),
    ));

    expect(
      tester.widget<FilledButton>(find.byType(FilledButton)).onPressed,
      isNull,
    );
    await tester.enterText(find.byType(TextField), '6 × [70]');
    await tester.pump();
    expect(
      tester.widget<FilledButton>(find.byType(FilledButton)).onPressed,
      isNotNull,
    );
  });

  test('the set summary covers clusters, holds, bodyweight and back-offs', () {
    expect(setSummary(WorkSet(cluster: [5, 5, 4], totalReps: 14)), '5+5+4 (14)');
    expect(setSummary(WorkSet(cluster: [5, 5])), '5+5');
    expect(setSummary(WorkSet(durationSec: 54)), '54c');
    expect(setSummary(WorkSet(reps: 6, weightKg: 70)), '6×70');
    expect(setSummary(WorkSet(reps: 6)), '6×bw');
    expect(setSummary(WorkSet(reps: 6, weightKg: 30, isBackoff: true)), '6×30*');
  });
}
