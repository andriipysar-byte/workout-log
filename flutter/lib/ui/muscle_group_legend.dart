import 'package:flutter/material.dart';
import 'package:workout_log_core/workout_log_core.dart';

import 'theme.dart';

class MuscleGroupLegend extends StatelessWidget {
  const MuscleGroupLegend({super.key});

  @override
  Widget build(BuildContext context) => Wrap(
        spacing: 10,
        runSpacing: 4,
        children: [
          for (final group in MuscleGroup.values)
            Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  width: 11,
                  height: 11,
                  decoration: BoxDecoration(
                    color: group.color,
                    borderRadius: BorderRadius.circular(3),
                  ),
                ),
                const SizedBox(width: 5),
                Text(group.label, style: Theme.of(context).textTheme.labelSmall),
              ],
            ),
        ],
      );
}
