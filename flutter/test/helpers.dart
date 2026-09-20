import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:provider/provider.dart';
import 'package:workout_log_core/workout_log_core.dart';
import 'package:workout_log/app_model.dart';
import 'package:workout_log/platform/session_folder.dart';
import 'package:workout_log/ui/root_view.dart';

/// An app model over an in-memory archive, so widget tests need no platform
/// channels and no real folder.
///
/// `start` is run through `runAsync` because it loads the catalogue and the SVG
/// from the asset bundle — real file I/O, which the fake-async zone a
/// `testWidgets` body runs in would never complete.
Future<AppModel> memoryModel(
  WidgetTester tester, {
  Map<String, String> seed = const {},
}) async {
  final model = AppModel();
  await tester.runAsync(
    () => model.start(
      folder: SessionFolder(
        storage: MemoryStorage(seed: {...seed}, label: 'test archive'),
        canChooseFolder: false,
        isBrowserCopy: false,
      ),
    ),
  );
  return model;
}

Widget hostedIn(AppModel model, Widget child) => ChangeNotifierProvider.value(
      value: model,
      child: MaterialApp(home: Scaffold(body: child)),
    );

/// The whole shell under test, for the flows that go through the toolbar.
Widget appUnder(AppModel model) => ChangeNotifierProvider.value(
      value: model,
      child: const MaterialApp(home: RootView()),
    );

Session sampleSession({String date = '2026-07-21', String cycleDay = 'A1'}) =>
    Session(
      date: date,
      cycleDay: cycleDay,
      blocks: [
        CardioBlock(machine: 'гребля', durationMin: 10),
        StrengthBlock(
          exercise: 'присід фронтальний',
          sets: [WorkSet(reps: 6, weightKg: 70)],
        ),
      ],
    );
