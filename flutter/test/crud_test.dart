import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:workout_log_core/workout_log_core.dart';
import 'package:workout_log/ui/new_session_dialog.dart';

import 'helpers.dart';

void main() {
  testWidgets('create writes a new file and selects it', (tester) async {
    final model = await memoryModel(tester);
    await tester.pumpWidget(hostedIn(model, const SizedBox()));

    await model.create(sampleSession());
    await tester.pump();

    expect(model.files, ['2026-07-21_A1.json']);
    expect(model.selection, '2026-07-21_A1.json');
    expect(model.status, 'Created 2026-07-21_A1.json');
  });

  testWidgets('delete removes the file and clears the selection',
      (tester) async {
    final model = await memoryModel(
      tester,
      seed: {'2026-07-21_A1.json': SessionCoding.encode(sampleSession())},
    );
    await tester.pumpWidget(hostedIn(model, const SizedBox()));

    await model.open('2026-07-21_A1.json');
    await model.delete('2026-07-21_A1.json');
    await tester.pump();

    expect(model.files, isEmpty);
    expect(model.selection, isNull);
    expect(model.session, isNull);
    expect(model.status, 'Deleted 2026-07-21_A1.json');
  });

  testWidgets('editing the date header moves the file instead of orphaning it',
      (tester) async {
    final model = await memoryModel(
      tester,
      seed: {'2026-07-21_A1.json': SessionCoding.encode(sampleSession())},
    );
    await tester.pumpWidget(hostedIn(model, const SizedBox()));

    await model.open('2026-07-21_A1.json');
    model.session!.date = '2026-07-22';
    await model.save();
    await tester.pump();

    expect(model.files, ['2026-07-22_A1.json']);
    expect(model.selection, '2026-07-22_A1.json');
    expect(model.status, 'Renamed 2026-07-21_A1.json → 2026-07-22_A1.json');
  });

  testWidgets('saving without a header edit reports a plain save',
      (tester) async {
    final model = await memoryModel(
      tester,
      seed: {'2026-07-21_A1.json': SessionCoding.encode(sampleSession())},
    );
    await tester.pumpWidget(hostedIn(model, const SizedBox()));

    await model.open('2026-07-21_A1.json');
    await model.save();
    await tester.pump();

    expect(model.files, ['2026-07-21_A1.json']);
    expect(model.status, 'Saved 2026-07-21_A1.json');
  });

  testWidgets('the dialog creates a blank session from date and cycle day',
      (tester) async {
    final model = await memoryModel(tester);
    Session? created;
    await tester.pumpWidget(hostedIn(
      model,
      Builder(
        builder: (context) => TextButton(
          onPressed: () async =>
              created = await showNewSessionDialog(context, model),
          child: const Text('open'),
        ),
      ),
    ));

    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();

    await tester.enterText(
        find.widgetWithText(TextField, 'YYYY-MM-DD'), '2026-09-01');
    await tester.enterText(find.widgetWithText(TextField, 'A1'), 'B2');
    await tester.tap(find.text('Create'));
    await tester.pumpAndSettle();

    expect(created, isNotNull);
    expect(SessionStore.idFor(created!), '2026-09-01_B2.json');
    expect(created!.blocks, isEmpty);
  });

  testWidgets('the dialog expands a cycle template into blocks', (tester) async {
    final model = await memoryModel(tester);
    Session? created;
    await tester.pumpWidget(hostedIn(
      model,
      Builder(
        builder: (context) => TextButton(
          onPressed: () async =>
              created = await showNewSessionDialog(context, model),
          child: const Text('open'),
        ),
      ),
    ));

    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();

    await tester.tap(find.byType(DropdownButtonFormField<CycleSession?>));
    await tester.pumpAndSettle();
    await tester.tap(find.textContaining('A1 ·').last);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Create'));
    await tester.pumpAndSettle();

    expect(created, isNotNull);
    expect(created!.cycleDay, 'A1');
    expect(created!.date, '2026-07-21', reason: 'the template\'s own slot');
    expect(created!.blocks, isNotEmpty);
    final encoded = SessionCoding.encode(created!);
    expect(encoded, isNot(contains('sets_reps')));
    expect(encoded, isNot(contains('"role"')));
  });

  testWidgets('a filename collision offers overwrite or cancel', (tester) async {
    final model = await memoryModel(
      tester,
      seed: {'2026-07-21_A1.json': SessionCoding.encode(sampleSession())},
    );
    Session? created;
    await tester.pumpWidget(hostedIn(
      model,
      Builder(
        builder: (context) => TextButton(
          onPressed: () async =>
              created = await showNewSessionDialog(context, model),
          child: const Text('open'),
        ),
      ),
    ));

    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();
    await tester.enterText(
        find.widgetWithText(TextField, 'YYYY-MM-DD'), '2026-07-21');
    await tester.enterText(find.widgetWithText(TextField, 'A1'), 'A1');
    await tester.tap(find.text('Create'));
    await tester.pumpAndSettle();

    expect(find.text('Session already exists'), findsOneWidget);
    await tester.tap(find.text('Cancel').last);
    await tester.pumpAndSettle();
    expect(created, isNull, reason: 'declining the overwrite creates nothing');
    expect(find.text('Session already exists'), findsNothing);
  });

  testWidgets('a malformed date is rejected in the dialog', (tester) async {
    final model = await memoryModel(tester);
    await tester.pumpWidget(hostedIn(
      model,
      Builder(
        builder: (context) => TextButton(
          onPressed: () => showNewSessionDialog(context, model),
          child: const Text('open'),
        ),
      ),
    ));

    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();
    await tester.enterText(
        find.widgetWithText(TextField, 'YYYY-MM-DD'), 'tomorrow');
    await tester.enterText(find.widgetWithText(TextField, 'A1'), 'A1');
    await tester.tap(find.text('Create'));
    await tester.pumpAndSettle();

    expect(find.text('Date must be YYYY-MM-DD.'), findsOneWidget);
  });

  testWidgets('deleting from the shell asks first', (tester) async {
    final model = await memoryModel(
      tester,
      seed: {'2026-07-21_A1.json': SessionCoding.encode(sampleSession())},
    );
    await tester.pumpWidget(hostedIn(model, const SizedBox()));
    await model.open('2026-07-21_A1.json');

    await tester.pumpWidget(appUnder(model));
    await tester.pumpAndSettle();

    await tester.tap(find.byTooltip('Delete session'));
    await tester.pumpAndSettle();
    expect(find.text('Delete session'), findsWidgets);

    await tester.tap(find.text('Cancel'));
    await tester.pumpAndSettle();
    expect(model.files, ['2026-07-21_A1.json'], reason: 'cancel deletes nothing');
  });
}
