import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../app_model.dart';
import 'block_cards.dart';
import 'muscle_map.dart';
import 'theme.dart';

class SessionEditor extends StatelessWidget {
  const SessionEditor({required this.session, super.key});

  final Session session;

  @override
  Widget build(BuildContext context) {
    final model = context.read<AppModel>();
    return ListView(
      padding: const EdgeInsets.all(20),
      children: [
        _Header(session: session, onChanged: model.sessionEdited),
        const Divider(height: 24),
        for (final block in session.blocks)
          BlockCard(
            key: ObjectKey(block),
            block: block,
            onChanged: model.sessionEdited,
          ),
        const Divider(height: 24),
        const DayMuscleMap(),
      ],
    );
  }
}

class _Header extends StatelessWidget {
  const _Header({required this.session, required this.onChanged});

  final Session session;
  final VoidCallback onChanged;

  @override
  Widget build(BuildContext context) => Wrap(
        spacing: 16,
        runSpacing: 8,
        children: [
          _labeled(
            context,
            'Date',
            OptionalField(
              label: 'YYYY-MM-DD',
              width: 150,
              value: session.date,
              onChanged: (v) {
                session.date = v ?? '';
                onChanged();
              },
            ),
          ),
          _labeled(
            context,
            'Cycle day',
            OptionalField(
              label: 'A1',
              width: 100,
              value: session.cycleDay,
              onChanged: (v) {
                session.cycleDay = v ?? '';
                onChanged();
              },
            ),
          ),
          _labeled(
            context,
            'Start',
            OptionalField(
              label: 'H:MM',
              width: 100,
              value: session.startTime,
              onChanged: (v) {
                session.startTime = v;
                onChanged();
              },
            ),
          ),
          _labeled(
            context,
            'Kind',
            SizedBox(
              width: 130,
              child: DropdownButtonFormField<Kind>(
                initialValue: session.kind,
                isExpanded: true,
                items: [
                  for (final kind in Kind.values)
                    DropdownMenuItem(value: kind, child: Text(kind.name)),
                ],
                onChanged: (kind) {
                  if (kind == null) return;
                  session.kind = kind;
                  onChanged();
                },
              ),
            ),
          ),
          _labeled(
            context,
            'Bodyweight',
            NumberField(
              label: 'kg',
              width: 100,
              value: session.bodyweightKg,
              onChanged: (v) {
                session.bodyweightKg = v;
                onChanged();
              },
            ),
          ),
          _labeled(
            context,
            'Notes',
            OptionalField(
              label: '',
              width: 260,
              value: session.notes,
              onChanged: (v) {
                session.notes = v;
                onChanged();
              },
            ),
          ),
        ],
      );

  Widget _labeled(BuildContext context, String title, Widget field) => Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(title, style: Theme.of(context).textTheme.labelSmall),
          const SizedBox(width: 6),
          field,
        ],
      );
}

class DayMuscleMap extends StatelessWidget {
  const DayMuscleMap({super.key});

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
            Text('Muscle map', style: Theme.of(context).textTheme.titleSmall),
            const Spacer(),
            const WeightingModePicker(),
          ],
        ),
        const SizedBox(height: 8),
        switch (model.dayMapSVG()) {
          final String svg => DecoratedBox(
              decoration: BoxDecoration(
                color: cardSurface(context),
                borderRadius: BorderRadius.circular(10),
              ),
              child: MuscleMap(svg: svg),
            ),
          null => const MuscleMapUnavailable(
              'Muscle map unavailable (catalogue or template not found).',
            ),
        },
      ],
    );
  }
}

class WeightingModePicker extends StatelessWidget {
  const WeightingModePicker({super.key});

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    return Tooltip(
      message: 'How each exercise is weighted into the map',
      child: SegmentedButton<WeightingMode>(
        showSelectedIcon: false,
        style: const ButtonStyle(visualDensity: VisualDensity.compact),
        segments: [
          for (final mode in WeightingMode.values)
            ButtonSegment(value: mode, label: Text(mode.label)),
        ],
        selected: {model.mode},
        onSelectionChanged: (selected) => model.setMode(selected.first),
      ),
    );
  }
}
