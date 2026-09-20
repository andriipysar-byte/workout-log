/// Domain core for WorkoutLog (ADR-004).
///
/// This library depends on neither `dart:io` nor Flutter, which is what keeps
/// platform concerns out of the domain; `test/purity_test.dart` enforces it.
library;

export 'src/analytics/cycle_report.dart';
export 'src/analytics/muscle_activation.dart';
export 'src/analytics/muscle_group.dart';
export 'src/analytics/muscle_map_svg.dart';
export 'src/analytics/progress.dart';
export 'src/analytics/session_metrics.dart';
export 'src/coding.dart';
export 'src/cycles/cycle_day.dart';
export 'src/cycles/cycle_generator.dart';
export 'src/cycles/cycle_templates.dart';
export 'src/io/session_storage.dart';
export 'src/io/session_store.dart';
export 'src/models/block.dart';
export 'src/models/cycle.dart';
export 'src/models/exercise.dart';
export 'src/models/session.dart';
export 'src/models/work_set.dart';
export 'src/parsing/notation.dart';
export 'src/validation.dart';
export 'src/wall_clock.dart';
