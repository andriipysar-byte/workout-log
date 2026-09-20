import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../app_model.dart';
import 'calendar_view.dart';
import 'cycle_table.dart';
import 'muscle_map.dart';
import 'session_editor.dart';
import 'theme.dart';

class CycleView extends StatelessWidget {
  const CycleView({super.key});

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    return LayoutBuilder(
      builder: (context, constraints) {
        final table = SizedBox(
          height: constraints.maxHeight * 0.45,
          child: CycleTable(matrix: model.cycle),
        );
        final side = Padding(
          padding: const EdgeInsets.all(8),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const _CycleMuscleMap(),
              const SizedBox(height: 12),
              const CalendarView(),
            ],
          ),
        );

        return ListView(
          children: [
            table,
            const Divider(height: 1),
            if (constraints.maxWidth >= wideLayoutBreakpoint)
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [SizedBox(width: 340, child: side), const Spacer()],
              )
            else
              side,
          ],
        );
      },
    );
  }
}

class _CycleMuscleMap extends StatelessWidget {
  const _CycleMuscleMap();

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            const Icon(Icons.fitness_center, size: 18),
            const SizedBox(width: 6),
            Text(
              'Cycle muscle map',
              style: Theme.of(context).textTheme.titleSmall,
            ),
          ],
        ),
        const SizedBox(height: 6),
        const WeightingModePicker(),
        const SizedBox(height: 6),
        switch (model.cycleMapSVG()) {
          final String svg => DecoratedBox(
              decoration: BoxDecoration(
                color: cardSurface(context),
                borderRadius: BorderRadius.circular(10),
              ),
              child: MuscleMap(svg: svg, height: 260),
            ),
          null => const MuscleMapUnavailable(
              'Muscle map unavailable (no sessions or catalogue missing).',
            ),
        },
      ],
    );
  }
}
