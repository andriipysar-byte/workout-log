import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:workout_log/app_model.dart';
import 'package:workout_log/ui/cycle_table.dart';

import 'helpers.dart';

void main() {
  testWidgets('an empty cycle says so instead of drawing an empty grid',
      (tester) async {
    final model = await memoryModel(tester);
    await tester.pumpWidget(
      hostedIn(model, CycleTable(matrix: CycleMatrix())),
    );
    expect(find.text('No sessions in this folder.'), findsOneWidget);
  });

  testWidgets('a populated cycle draws one column per day and one row per '
      'exercise', (tester) async {
    final model = await memoryModel(tester);
    await tester.pumpWidget(hostedIn(
      model,
      CycleTable(
        matrix: CycleMatrix(
          days: const ['A1', 'A2'],
          exercises: const ['присід фронтальний', 'жим лежачи'],
          cells: const [
            [true, false],
            [false, true],
          ],
          groups: const [[], []],
        ),
      ),
    ));

    expect(find.text('Exercise'), findsOneWidget);
    expect(find.text('A1'), findsOneWidget);
    expect(find.text('A2'), findsOneWidget);
    expect(find.text('присід фронтальний'), findsOneWidget);
    expect(find.text('жим лежачи'), findsOneWidget);
  });

  testWidgets('long Ukrainian names ellipsize rather than overflow',
      (tester) async {
    final model = await memoryModel(tester);
    const long = 'прокручування кисті з гантелею на похилій лаві сидячи';
    await tester.pumpWidget(hostedIn(
      model,
      CycleTable(
        matrix: CycleMatrix(
          days: const ['A1'],
          exercises: const [long],
          cells: const [
            [true]
          ],
          groups: const [[]],
        ),
      ),
    ));

    final cell = tester.widget<Text>(find.text(long));
    expect(cell.overflow, TextOverflow.ellipsis);
    expect(cell.maxLines, 1);
    expect(tester.takeException(), isNull);
  });
}
