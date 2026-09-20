import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../app_model.dart';
import 'exercise_picker.dart';
import 'muscle_map.dart';
import 'muscle_group_legend.dart';
import 'plan_dialogs.dart';
import 'theme.dart';

/// Cycle planning: the template side of the app, editing `cycles.json` rather
/// than a session file.
class PlanView extends StatefulWidget {
  const PlanView({super.key});

  @override
  State<PlanView> createState() => PlanViewState();
}

class PlanViewState extends State<PlanView> {
  Cycle? _selected;

  Cycle? _resolve(AppModel model) {
    final cycles = model.cycles;
    if (cycles.isEmpty) return _selected = null;
    if (_selected != null && cycles.contains(_selected)) return _selected;
    return _selected = cycles.first;
  }

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    final cycle = _resolve(model);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _CycleBar(
          cycles: model.cycles,
          selected: cycle,
          onSelected: (c) => setState(() => _selected = c),
        ),
        const Divider(height: 1),
        Expanded(
          child: cycle == null
              ? _EmptyPlan(onCreate: () => _create(model))
              : _CycleDetail(cycle: cycle),
        ),
      ],
    );
  }

  Future<void> _create(AppModel model) async {
    final created = await showCycleDialog(context, model);
    if (created != null) setState(() => _selected = created);
  }
}

class _CycleBar extends StatelessWidget {
  const _CycleBar({
    required this.cycles,
    required this.selected,
    required this.onSelected,
  });

  final List<Cycle> cycles;
  final Cycle? selected;
  final ValueChanged<Cycle> onSelected;

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    return Padding(
      padding: const EdgeInsets.all(8),
      child: Wrap(
        spacing: 8,
        runSpacing: 8,
        crossAxisAlignment: WrapCrossAlignment.center,
        children: [
          SizedBox(
            width: 260,
            child: DropdownButtonFormField<Cycle>(
              initialValue: selected,
              isExpanded: true,
              decoration: const InputDecoration(labelText: 'Cycle'),
              items: [
                for (final cycle in cycles)
                  DropdownMenuItem(
                    value: cycle,
                    child: Text(cycle.name, overflow: TextOverflow.ellipsis),
                  ),
              ],
              onChanged: (c) => c == null ? null : onSelected(c),
            ),
          ),
          FilledButton.tonalIcon(
            onPressed: model.canEditPlan
                ? () async {
                    final created = await showCycleDialog(context, model);
                    if (created != null) onSelected(created);
                  }
                : null,
            icon: const Icon(Icons.add),
            label: const Text('New cycle'),
          ),
          FilledButton.tonalIcon(
            onPressed: selected == null || !model.canEditPlan
                ? null
                : () async {
                    final clone =
                        await showCycleDialog(context, model, cloneOf: selected);
                    if (clone != null) onSelected(clone);
                  },
            icon: const Icon(Icons.copy_all),
            label: const Text('Clone'),
          ),
          FilledButton.tonalIcon(
            onPressed: selected == null || !model.canEditPlan
                ? null
                : () => showCycleDialog(context, model, edit: selected),
            icon: const Icon(Icons.edit_outlined),
            label: const Text('Edit'),
          ),
          FilledButton.icon(
            onPressed: model.canEditPlan ? model.saveCycles : null,
            icon: const Icon(Icons.save),
            label: const Text('Save cycles.json'),
          ),
          if (!model.canEditPlan)
            Tooltip(
              message: 'cycles.json is read-only here — it ships with the app '
                  'on this platform (ADR-007).',
              child: Icon(
                Icons.lock_outline,
                size: 18,
                color: Theme.of(context).colorScheme.outline,
              ),
            ),
        ],
      ),
    );
  }
}

class _EmptyPlan extends StatelessWidget {
  const _EmptyPlan({required this.onCreate});

  final VoidCallback onCreate;

  @override
  Widget build(BuildContext context) => Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.event_note, size: 40),
            const SizedBox(height: 8),
            Text(
              'No cycles yet.',
              style: Theme.of(context).textTheme.bodyMedium,
            ),
            const SizedBox(height: 12),
            FilledButton.icon(
              onPressed: onCreate,
              icon: const Icon(Icons.add),
              label: const Text('New cycle'),
            ),
          ],
        ),
      );
}

/// Workouts run left-to-right, one column each, the way a cycle reads on paper.
const _workoutColumnWidth = 380.0;
const _addColumnWidth = 200.0;

class _CycleDetail extends StatefulWidget {
  const _CycleDetail({required this.cycle});

  final Cycle cycle;

  @override
  State<_CycleDetail> createState() => _CycleDetailState();
}

class _CycleDetailState extends State<_CycleDetail> {
  final _columns = ScrollController();

  @override
  void dispose() {
    _columns.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final cycle = widget.cycle;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(12, 8, 12, 0),
          child: _CycleSummary(cycle: cycle),
        ),
        const SizedBox(height: 8),
        Expanded(
          // Always-visible thumb: a horizontal list gives no other hint that
          // there are more workouts past the right edge.
          child: Scrollbar(
            controller: _columns,
            thumbVisibility: true,
            child: ListView.builder(
              controller: _columns,
              scrollDirection: Axis.horizontal,
              padding: const EdgeInsets.fromLTRB(12, 0, 12, 16),
              itemCount: cycle.sessions.length + 1,
              itemBuilder: (context, i) => i == cycle.sessions.length
                  ? _AddWorkoutColumn(cycle: cycle)
                  : WorkoutCard(
                      key: ObjectKey(cycle.sessions[i]),
                      cycle: cycle,
                      workout: cycle.sessions[i],
                      index: i,
                    ),
            ),
          ),
        ),
      ],
    );
  }
}

class _AddWorkoutColumn extends StatelessWidget {
  const _AddWorkoutColumn({required this.cycle});

  final Cycle cycle;

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    final next =
        CycleTemplates.nextDay(cycle.sessions.map((s) => s.cycleDay)).code;
    return SizedBox(
      width: _addColumnWidth,
      child: Align(
        alignment: Alignment.topCenter,
        child: Padding(
          padding: const EdgeInsets.only(top: 12),
          child: FilledButton.icon(
            onPressed: model.canEditPlan ? () => model.addWorkout(cycle) : null,
            icon: const Icon(Icons.add),
            label: Text('Add $next'),
          ),
        ),
      ),
    );
  }
}

class _CycleSummary extends StatelessWidget {
  const _CycleSummary({required this.cycle});

  final Cycle cycle;

  @override
  Widget build(BuildContext context) => Card(
        elevation: 0,
        color: cardSurface(context),
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(cycle.name,
                        style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 2),
                    Text(
                      '${cycle.sessions.length} workouts · '
                      'starts ${cycle.startDate} · '
                      '${cycle.trainingDays.join(", ")}',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ],
                ),
              ),
              const MuscleGroupLegend(),
            ],
          ),
        ),
      );
}

/// One planned workout: its code, the date it lands on, its muscle map, and its
/// blocks.
class WorkoutCard extends StatefulWidget {
  const WorkoutCard({
    required this.cycle,
    required this.workout,
    required this.index,
    super.key,
  });

  final Cycle cycle;
  final CycleSession workout;
  final int index;

  @override
  State<WorkoutCard> createState() => _WorkoutCardState();
}

class _WorkoutCardState extends State<WorkoutCard> {
  bool _showMap = true;

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    final day = CycleDay.tryParse(widget.workout.cycleDay);
    final date = model.plannedDate(widget.cycle, widget.index);
    final group = model.planDominantGroup(widget.workout);

    return SizedBox(
      width: _workoutColumnWidth,
      child: Card(
        elevation: 0,
        color: cardSurface(context),
        margin: const EdgeInsets.only(right: 12),
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _header(context, model, day, date, group),
              const Divider(height: 16),
              // A column is height-bounded by the row, so a long block list
              // scrolls inside its own card rather than pushing the row taller.
              Expanded(
                child: ListView(
                  padding: EdgeInsets.zero,
                  children: [
                    if (_showMap) ...[
                      switch (model.planMapSVG(widget.workout)) {
                        final String svg => MuscleMap(svg: svg, height: 230),
                        null => const MuscleMapUnavailable(
                            'No exercises chosen yet — nothing to map.',
                          ),
                      },
                      const SizedBox(height: 8),
                    ],
                    for (final block in widget.workout.blocks)
                      _BlockRow(
                        key: ObjectKey(block),
                        block: block,
                        onChanged: model.cycleEdited,
                        onRemove: () {
                          widget.workout.blocks.remove(block);
                          model.cycleEdited();
                        },
                        enabled: model.canEditPlan,
                      ),
                    Align(
                      alignment: Alignment.centerLeft,
                      child: TextButton.icon(
                        onPressed: model.canEditPlan
                            ? () async {
                                final type = await showBlockTypeDialog(context);
                                if (type == null) return;
                                widget.workout.blocks
                                    .add(BlockTemplate(type: type));
                                model.cycleEdited();
                              }
                            : null,
                        icon: const Icon(Icons.add, size: 16),
                        label: const Text('Add block'),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _header(
    BuildContext context,
    AppModel model,
    CycleDay? day,
    DateTime? date,
    MuscleGroup? group,
  ) =>
      Row(
        children: [
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
            decoration: BoxDecoration(
              color: group?.color ?? unclassifiedDayColor,
              borderRadius: BorderRadius.circular(8),
            ),
            child: Text(
              widget.workout.cycleDay,
              style: const TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.bold,
              ),
            ),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  day?.kind.label ?? 'custom',
                  style: Theme.of(context).textTheme.titleSmall,
                ),
                Text(
                  [
                    if (date != null) CycleGenerator.isoDate(date),
                    if (widget.workout.weekday != null) widget.workout.weekday!,
                    if (widget.workout.title != null) widget.workout.title!,
                  ].join(' · '),
                  style: Theme.of(context).textTheme.bodySmall,
                  overflow: TextOverflow.ellipsis,
                ),
              ],
            ),
          ),
          IconButton(
            tooltip: _showMap ? 'Hide muscle map' : 'Show muscle map',
            icon: Icon(_showMap ? Icons.visibility : Icons.visibility_off),
            onPressed: () => setState(() => _showMap = !_showMap),
          ),
          // Four separate icon buttons do not fit a column this narrow, so the
          // rarer actions collapse into a menu.
          if (model.canEditPlan) _actions(context, model),
        ],
      );

  Widget _actions(BuildContext context, AppModel model) =>
      PopupMenuButton<_WorkoutAction>(
        tooltip: 'Workout actions',
        icon: const Icon(Icons.more_vert),
        onSelected: (action) => _run(action, context, model),
        itemBuilder: (context) => [
          _item(_WorkoutAction.moveLeft, Icons.arrow_back, 'Move left',
              enabled: widget.index > 0),
          _item(_WorkoutAction.moveRight, Icons.arrow_forward, 'Move right',
              enabled: widget.index < widget.cycle.sessions.length - 1),
          _item(_WorkoutAction.rename, Icons.tag, 'Rename workout'),
          _item(_WorkoutAction.remove, Icons.delete_outline, 'Remove workout'),
        ],
      );

  PopupMenuItem<_WorkoutAction> _item(
    _WorkoutAction action,
    IconData icon,
    String label, {
    bool enabled = true,
  }) =>
      PopupMenuItem(
        value: action,
        enabled: enabled,
        child: Row(
          children: [
            Icon(icon, size: 18),
            const SizedBox(width: 10),
            Text(label),
          ],
        ),
      );

  Future<void> _run(
    _WorkoutAction action,
    BuildContext context,
    AppModel model,
  ) async {
    switch (action) {
      case _WorkoutAction.moveLeft:
        model.moveWorkout(widget.cycle, widget.index, widget.index - 1);
      case _WorkoutAction.moveRight:
        model.moveWorkout(widget.cycle, widget.index, widget.index + 1);
      case _WorkoutAction.rename:
        await showRetitleDialog(context, model, widget.cycle, widget.workout);
      case _WorkoutAction.remove:
        final ok = await confirmRemoveWorkout(context, widget.workout.cycleDay);
        if (ok) model.removeWorkout(widget.cycle, widget.workout);
    }
  }
}

enum _WorkoutAction { moveLeft, moveRight, rename, remove }

class _BlockRow extends StatelessWidget {
  const _BlockRow({
    required this.block,
    required this.onChanged,
    required this.onRemove,
    required this.enabled,
    super.key,
  });

  final BlockTemplate block;
  final VoidCallback onChanged;
  final VoidCallback onRemove;
  final bool enabled;

  IconData get _icon => switch (block.type) {
        'cardio' => Icons.directions_run,
        'strength' => Icons.fitness_center,
        'metcon' => Icons.local_fire_department,
        _ => Icons.air,
      };

  @override
  Widget build(BuildContext context) {
    final model = context.read<AppModel>();
    final subtitle = switch (block.type) {
      'strength' => [
          if (block.role != null) block.role!,
          if (block.setsReps.isNotEmpty) block.setsReps.join('+'),
        ].join(' · '),
      'cardio' => '${block.durationMin?.toInt() ?? '—'} min',
      'metcon' => [
          block.format?.wire ?? 'format —',
          if (block.scheme != null) block.scheme!.join('-'),
          ...block.exercises.map((e) => e.name),
        ].join(' · '),
      _ => block.role ?? '',
    };

    final unresolved = block.type == 'strength' &&
        block.exercise != null &&
        model.catalogue?.resolve(block.exercise!) == null;

    return ListTile(
      dense: true,
      contentPadding: EdgeInsets.zero,
      leading: Icon(_icon, size: 18),
      title: Row(
        children: [
          Flexible(
            child: Text(
              block.type == 'strength'
                  ? (block.exercise?.isNotEmpty == true
                      ? block.exercise!
                      : 'choose exercise…')
                  : block.type,
              overflow: TextOverflow.ellipsis,
              style: TextStyle(
                fontStyle: block.type == 'strength' &&
                        (block.exercise?.isEmpty ?? true)
                    ? FontStyle.italic
                    : null,
                color: block.type == 'strength' &&
                        (block.exercise?.isEmpty ?? true)
                    ? Theme.of(context).colorScheme.outline
                    : null,
              ),
            ),
          ),
          if (unresolved) ...[
            const SizedBox(width: 6),
            Tooltip(
              message: '"${block.exercise}" is not in the catalogue, so it '
                  'cannot reach the muscle map.',
              child: const Icon(Icons.warning_amber,
                  size: 15, color: Colors.orange),
            ),
          ],
        ],
      ),
      subtitle: subtitle.isEmpty
          ? null
          : Text(subtitle, maxLines: 2, overflow: TextOverflow.ellipsis),
      trailing: enabled
          ? Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                if (block.type == 'strength')
                  _CompactIcon(
                    tooltip: 'Choose exercise',
                    icon: Icons.search,
                    onPressed: () async {
                      final chosen = await pickExercise(
                        context,
                        current: block.exercise,
                      );
                      if (chosen == null) return;
                      block.exercise = chosen;
                      onChanged();
                    },
                  ),
                _CompactIcon(
                  tooltip: 'Edit block',
                  icon: Icons.tune,
                  onPressed: () async {
                    await showBlockDialog(context, block);
                    onChanged();
                  },
                ),
                _CompactIcon(
                  tooltip: 'Remove block',
                  icon: Icons.close,
                  onPressed: onRemove,
                ),
              ],
            )
          : null,
    );
  }
}

class _CompactIcon extends StatelessWidget {
  const _CompactIcon({
    required this.tooltip,
    required this.icon,
    required this.onPressed,
  });

  final String tooltip;
  final IconData icon;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) => IconButton(
        tooltip: tooltip,
        icon: Icon(icon, size: 18),
        onPressed: onPressed,
        padding: EdgeInsets.zero,
        visualDensity: VisualDensity.compact,
        constraints: const BoxConstraints.tightFor(width: 30, height: 30),
      );
}
