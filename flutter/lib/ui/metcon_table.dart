import 'package:flutter/material.dart';
import 'package:workout_log_core/workout_log_core.dart';

/// The whiteboard view of a metcon — one row per movement, one column per
/// round:
///
///     wod: for_time
///       трастери (24kg+24kg)   21  15   9
///       бьорпі                 21  15   9
///
/// A movement with its own `reps_override` prints that instead of the
/// workout's scheme, which is the whole reason the rounds are columns rather
/// than one shared line.
class MetconTable extends StatelessWidget {
  const MetconTable({
    required this.format,
    required this.scheme,
    required this.exercises,
    this.showHeader = true,
    super.key,
  });

  final MetconFormat? format;
  final List<int>? scheme;
  final List<MetconExercise> exercises;
  final bool showHeader;

  static const _roundColumnWidth = 32.0;

  int get _rounds => exercises.fold<int>(
        0,
        (widest, e) {
          final length = e.schemeWithin(scheme).length;
          return length > widest ? length : widest;
        },
      );

  @override
  Widget build(BuildContext context) {
    final caption = Theme.of(context).textTheme.bodySmall;
    final rounds = _rounds;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        if (showHeader)
          Text(
            'wod: ${format?.wire ?? '—'}',
            style: caption?.copyWith(fontWeight: FontWeight.w600),
          ),
        if (exercises.isEmpty)
          Text(
            'no movements yet',
            style: caption?.copyWith(fontStyle: FontStyle.italic),
          )
        else
          Table(
            columnWidths: {
              0: const FlexColumnWidth(),
              for (var i = 1; i <= rounds; i++)
                i: const FixedColumnWidth(_roundColumnWidth),
            },
            defaultVerticalAlignment: TableCellVerticalAlignment.middle,
            children: [
              for (final exercise in exercises)
                TableRow(
                  children: [
                    _movement(context, exercise),
                    for (var round = 0; round < rounds; round++)
                      _reps(context, exercise.schemeWithin(scheme), round),
                  ],
                ),
            ],
          ),
      ],
    );
  }

  Widget _movement(BuildContext context, MetconExercise exercise) {
    final theme = Theme.of(context);
    final prescription = exercise.prescription;
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Text.rich(
        TextSpan(
          children: [
            TextSpan(text: exercise.name),
            if (prescription != null)
              TextSpan(
                text: '  ($prescription)',
                style: TextStyle(color: theme.colorScheme.outline),
              ),
          ],
        ),
        style: theme.textTheme.bodySmall,
        maxLines: 2,
        overflow: TextOverflow.ellipsis,
      ),
    );
  }

  Widget _reps(BuildContext context, List<int> own, int round) => Padding(
        padding: const EdgeInsets.symmetric(vertical: 2),
        child: Text(
          round < own.length ? '${own[round]}' : '',
          textAlign: TextAlign.right,
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
                fontFeatures: const [FontFeature.tabularFigures()],
              ),
        ),
      );
}
