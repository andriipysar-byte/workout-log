import 'package:flutter/material.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../app_model.dart';
import 'theme.dart';

class CycleTable extends StatelessWidget {
  const CycleTable({required this.matrix, super.key});

  static const _nameWidth = 240.0;
  static const _dayWidth = 46.0;

  final CycleMatrix matrix;

  @override
  Widget build(BuildContext context) {
    if (matrix.isEmpty) {
      return Center(
        child: Text(
          'No sessions in this folder.',
          style: Theme.of(context).textTheme.bodyMedium,
        ),
      );
    }

    return SingleChildScrollView(
      scrollDirection: Axis.horizontal,
      child: SingleChildScrollView(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                _headerCell(context, 'Exercise', _nameWidth, Alignment.centerLeft),
                for (final day in matrix.days)
                  _headerCell(context, day, _dayWidth, Alignment.center),
              ],
            ),
            for (var row = 0; row < matrix.exercises.length; row++)
              Row(
                children: [
                  _nameCell(context, matrix.exercises[row], matrix.groups[row]),
                  for (var col = 0; col < matrix.days.length; col++)
                    _markCell(
                      context,
                      present: matrix.cells[row][col],
                      groups: matrix.groups[row],
                    ),
                ],
              ),
          ],
        ),
      ),
    );
  }

  BoxDecoration _cellBorder(BuildContext context, {Gradient? gradient}) =>
      BoxDecoration(
        gradient: gradient,
        border: Border(
          bottom: BorderSide(
            color: Theme.of(context).dividerColor,
            width: 0.5,
          ),
        ),
      );

  Widget _headerCell(
    BuildContext context,
    String text,
    double width,
    Alignment align,
  ) =>
      Container(
        width: width,
        alignment: align,
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
        decoration: _cellBorder(context).copyWith(color: cardSurface(context)),
        child: Text(
          text,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: Theme.of(context)
              .textTheme
              .labelSmall
              ?.copyWith(fontWeight: FontWeight.bold),
        ),
      );

  /// The catalogue's names are Ukrainian and often longer than the column, so
  /// they ellipsize with the full name behind a tooltip.
  Widget _nameCell(
    BuildContext context,
    String text,
    List<MuscleGroup> groups,
  ) =>
      Container(
        width: _nameWidth,
        alignment: Alignment.centerLeft,
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
        decoration: _cellBorder(context, gradient: groupGradient(groups, 0.22)),
        child: Tooltip(
          message: text,
          child: Text(text, maxLines: 1, overflow: TextOverflow.ellipsis),
        ),
      );

  Widget _markCell(
    BuildContext context, {
    required bool present,
    required List<MuscleGroup> groups,
  }) =>
      Container(
        width: _dayWidth,
        height: 33,
        alignment: Alignment.center,
        decoration: _cellBorder(
          context,
          gradient: present ? groupGradient(groups, 0.16) : null,
        ),
        child: present
            ? Container(
                width: 9,
                height: 9,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: groups.isEmpty
                      ? Theme.of(context).colorScheme.primary
                      : groups.first.color,
                ),
              )
            : null,
      );
}
