import 'package:flutter/material.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../app_model.dart';

/// Create a cycle, clone one, or edit an existing one's header.
///
/// `cloneOf` and `edit` are mutually exclusive; passing neither creates a new
/// empty cycle.
Future<Cycle?> showCycleDialog(
  BuildContext context,
  AppModel model, {
  Cycle? cloneOf,
  Cycle? edit,
}) =>
    showDialog<Cycle>(
      context: context,
      builder: (_) => _CycleDialog(model: model, cloneOf: cloneOf, edit: edit),
    );

class _CycleDialog extends StatefulWidget {
  const _CycleDialog({required this.model, this.cloneOf, this.edit});

  final AppModel model;
  final Cycle? cloneOf;
  final Cycle? edit;

  @override
  State<_CycleDialog> createState() => _CycleDialogState();
}

class _CycleDialogState extends State<_CycleDialog> {
  late final _id = TextEditingController(text: _initialId);
  late final _name = TextEditingController(text: _initialName);
  late final _startDate = TextEditingController(
    text: widget.edit?.startDate ??
        widget.cloneOf?.startDate ??
        CycleGenerator.isoDate(DateTime.now()),
  );
  late final Set<String> _days = {
    ...?widget.edit?.trainingDays,
    ...?widget.cloneOf?.trainingDays,
    if (widget.edit == null && widget.cloneOf == null) ...['Tue', 'Thu', 'Sun'],
  };
  String? _error;

  String get _initialId => switch ((widget.edit, widget.cloneOf)) {
        (final Cycle edit, _) => edit.id,
        (_, final Cycle clone) => '${clone.id}-copy',
        _ => '',
      };

  String get _initialName => switch ((widget.edit, widget.cloneOf)) {
        (final Cycle edit, _) => edit.name,
        (_, final Cycle clone) => '${clone.name} (copy)',
        _ => '',
      };

  bool get _isEdit => widget.edit != null;

  @override
  void dispose() {
    _id.dispose();
    _name.dispose();
    _startDate.dispose();
    super.dispose();
  }

  void _submit() {
    final id = _id.text.trim();
    final name = _name.text.trim();
    final startDate = _startDate.text.trim();

    if (id.isEmpty) return setState(() => _error = 'An id is required.');
    if (name.isEmpty) return setState(() => _error = 'A name is required.');
    if (!RegExp(r'^\d{4}-\d{2}-\d{2}$').hasMatch(startDate)) {
      return setState(() => _error = 'Start date must be YYYY-MM-DD.');
    }
    if (_days.isEmpty) {
      return setState(() => _error = 'Pick at least one training day.');
    }
    final clashes = _isEdit ? id != widget.edit!.id : true;
    if (clashes && widget.model.cycleIdTaken(id)) {
      return setState(() => _error = 'The id "$id" is already used.');
    }

    // Keep training days in calendar order, so the generated dates read in the
    // order a week actually happens.
    final ordered = [
      for (final day in CycleGenerator.weekdayAbbreviations)
        if (_days.contains(day)) day,
    ];

    final Cycle result;
    if (_isEdit) {
      result = widget.edit!
        ..id = id
        ..name = name
        ..startDate = startDate
        ..trainingDays = ordered;
      widget.model.cycleEdited();
    } else if (widget.cloneOf != null) {
      result = widget.model.cloneCycle(widget.cloneOf!, id: id, name: name)
        ..startDate = startDate
        ..trainingDays = ordered;
    } else {
      result = widget.model.createCycle(id: id, name: name)
        ..startDate = startDate
        ..trainingDays = ordered;
    }
    Navigator.pop(context, result);
  }

  @override
  Widget build(BuildContext context) => AlertDialog(
        title: Text(switch ((widget.edit, widget.cloneOf)) {
          (final Cycle _, _) => 'Edit cycle',
          (_, final Cycle _) => 'Clone cycle',
          _ => 'New cycle',
        }),
        content: SizedBox(
          width: 520,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              TextField(
                controller: _name,
                decoration: const InputDecoration(
                  labelText: 'Name',
                  hintText: '8-session hybrid cycle',
                ),
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  Expanded(
                    child: TextField(
                      controller: _id,
                      decoration: const InputDecoration(
                        labelText: 'Id',
                        hintText: 'hybrid-8',
                      ),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: TextField(
                      controller: _startDate,
                      decoration: const InputDecoration(
                        labelText: 'Start date',
                        hintText: 'YYYY-MM-DD',
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 14),
              Text('Training days',
                  style: Theme.of(context).textTheme.labelMedium),
              const SizedBox(height: 4),
              Wrap(
                spacing: 6,
                children: [
                  for (final day in CycleGenerator.weekdayAbbreviations)
                    FilterChip(
                      label: Text(day),
                      selected: _days.contains(day),
                      visualDensity: VisualDensity.compact,
                      onSelected: (on) => setState(
                        () => on ? _days.add(day) : _days.remove(day),
                      ),
                    ),
                ],
              ),
              if (_error != null) ...[
                const SizedBox(height: 12),
                Text(
                  _error!,
                  style: TextStyle(color: Theme.of(context).colorScheme.error),
                ),
              ],
            ],
          ),
        ),
        actions: [
          if (_isEdit)
            TextButton(
              onPressed: () async {
                final confirmed = await _confirm(
                  context,
                  title: 'Remove cycle',
                  message:
                      'Remove "${widget.edit!.name}" and its ${widget.edit!.sessions.length} '
                      'workouts? Nothing is written until you save cycles.json.',
                  action: 'Remove',
                );
                if (!confirmed || !context.mounted) return;
                widget.model.deleteCycle(widget.edit!);
                Navigator.pop(context);
              },
              child: const Text('Remove'),
            ),
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: _submit,
            child: Text(_isEdit ? 'Apply' : 'Create'),
          ),
        ],
      );
}

/// Change a workout's code. The letter and number are picked rather than typed,
/// which is what keeps the A1/A2 convention intact across a cycle.
Future<void> showRetitleDialog(
  BuildContext context,
  AppModel model,
  Cycle cycle,
  CycleSession workout,
) async {
  final current = CycleDay.tryParse(workout.cycleDay);
  var letter = current?.letter ?? 'A';
  var kind = current?.kind ?? DayKind.conditioning;

  final chosen = await showDialog<CycleDay>(
    context: context,
    builder: (context) => StatefulBuilder(
      builder: (context, setState) => AlertDialog(
        title: const Text('Rename workout'),
        content: SizedBox(
          width: 380,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Muscle group', style: Theme.of(context).textTheme.labelMedium),
              const SizedBox(height: 4),
              Wrap(
                spacing: 6,
                children: [
                  for (final option in 'ABCDEF'.split(''))
                    ChoiceChip(
                      label: Text(option),
                      selected: letter == option,
                      onSelected: (_) => setState(() => letter = option),
                    ),
                ],
              ),
              const SizedBox(height: 14),
              Text('Day type', style: Theme.of(context).textTheme.labelMedium),
              const SizedBox(height: 4),
              RadioGroup<DayKind>(
                groupValue: kind,
                onChanged: (value) => setState(() => kind = value ?? kind),
                child: Column(
                  children: [
                    for (final option in DayKind.values)
                      RadioListTile<DayKind>(
                        dense: true,
                        contentPadding: EdgeInsets.zero,
                        value: option,
                        title: Text('${option.number} — ${option.label}'),
                      ),
                  ],
                ),
              ),
              const SizedBox(height: 8),
              Text(
                'Becomes $letter${kind.number}',
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, CycleDay(letter, kind)),
            child: const Text('Rename'),
          ),
        ],
      ),
    ),
  );

  if (chosen != null) model.retitleWorkout(cycle, workout, chosen);
}

Future<String?> showBlockTypeDialog(BuildContext context) => showDialog<String>(
      context: context,
      builder: (context) => SimpleDialog(
        title: const Text('Add block'),
        children: [
          for (final type in const ['cardio', 'strength', 'metcon', 'cooldown'])
            SimpleDialogOption(
              onPressed: () => Navigator.pop(context, type),
              child: Text(type),
            ),
        ],
      ),
    );

/// Edit the fields of one planned block. Which fields exist depends on the
/// block type, so the form is built per type rather than showing empty rows.
Future<void> showBlockDialog(BuildContext context, BlockTemplate block) =>
    showDialog<void>(
      context: context,
      builder: (_) => _BlockDialog(block: block),
    );

class _BlockDialog extends StatefulWidget {
  const _BlockDialog({required this.block});

  final BlockTemplate block;

  @override
  State<_BlockDialog> createState() => _BlockDialogState();
}

class _BlockDialogState extends State<_BlockDialog> {
  late final _role = TextEditingController(text: widget.block.role ?? '');
  late final _notes = TextEditingController(text: widget.block.notes ?? '');
  late final _machine = TextEditingController(text: widget.block.machine ?? '');
  late final _duration = TextEditingController(
    text: widget.block.durationMin?.toInt().toString() ?? '',
  );
  late final _setsReps =
      TextEditingController(text: widget.block.setsReps.join(', '));
  late final _scheme =
      TextEditingController(text: widget.block.scheme?.join(', ') ?? '');
  late final List<_MovementDraft> _movements = [
    for (final exercise in widget.block.exercises) _MovementDraft(exercise),
  ];
  late MetconFormat? _format = widget.block.format;

  @override
  void dispose() {
    for (final controller in [
      _role,
      _notes,
      _machine,
      _duration,
      _setsReps,
      _scheme,
    ]) {
      controller.dispose();
    }
    for (final movement in _movements) {
      movement.dispose();
    }
    super.dispose();
  }

  static List<int> _numbers(String text) => text
      .split(RegExp(r'[,\s+]+'))
      .map((t) => int.tryParse(t.trim()))
      .whereType<int>()
      .toList();

  String? _orNull(TextEditingController c) =>
      c.text.trim().isEmpty ? null : c.text.trim();

  void _apply() {
    final block = widget.block
      ..role = _orNull(_role)
      ..notes = _orNull(_notes);

    switch (block.type) {
      case 'cardio':
        block
          ..machine = _machine.text.trim()
          ..durationMin = double.tryParse(_duration.text.trim());
      case 'strength':
        block.setsReps = _numbers(_setsReps.text);
      case 'metcon':
        final scheme = _numbers(_scheme.text);
        block
          ..format = _format
          ..scheme = scheme.isEmpty ? null : scheme
          ..exercises = [
            for (final movement in _movements)
              if (movement.name.text.trim().isNotEmpty) movement.toExercise(),
          ];
    }
    Navigator.pop(context);
  }

  static const _repsWidth = 96.0;
  static const _removeWidth = 36.0;

  Widget _movementHeader(BuildContext context) {
    final caption = Theme.of(context).textTheme.bodySmall;
    return Padding(
      padding: const EdgeInsets.only(bottom: 4),
      child: Row(
        children: [
          Expanded(flex: 4, child: Text('Movement', style: caption)),
          const SizedBox(width: 6),
          Expanded(flex: 3, child: Text('Load', style: caption)),
          const SizedBox(width: 6),
          SizedBox(width: _repsWidth, child: Text('Reps', style: caption)),
          const SizedBox(width: _removeWidth),
        ],
      ),
    );
  }

  Widget _movementRow(int i) => Padding(
        padding: const EdgeInsets.only(bottom: 6),
        child: Row(
          children: [
            Expanded(
              flex: 4,
              child: TextField(
                controller: _movements[i].name,
                decoration: const InputDecoration(hintText: 'трастери'),
              ),
            ),
            const SizedBox(width: 6),
            Expanded(
              flex: 3,
              child: TextField(
                controller: _movements[i].load,
                decoration: const InputDecoration(hintText: '24kg+24kg'),
              ),
            ),
            const SizedBox(width: 6),
            SizedBox(
              width: _repsWidth,
              child: TextField(
                controller: _movements[i].reps,
                // Empty means "follows the workout's scheme", which is why this
                // is blank rather than prefilled from it.
                decoration: const InputDecoration(hintText: 'scheme'),
              ),
            ),
            SizedBox(
              width: _removeWidth,
              child: IconButton(
                tooltip: 'Remove movement',
                icon: const Icon(Icons.close, size: 18),
                padding: EdgeInsets.zero,
                onPressed: () =>
                    setState(() => _movements.removeAt(i).dispose()),
              ),
            ),
          ],
        ),
      );

  @override
  Widget build(BuildContext context) => AlertDialog(
        title: Text('Edit ${widget.block.type} block'),
        content: SizedBox(
          width: 520,
          child: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                TextField(
                  controller: _role,
                  decoration: const InputDecoration(
                    labelText: 'Role',
                    hintText: 'warmup, main, accessory, grip…',
                  ),
                ),
                const SizedBox(height: 10),
                if (widget.block.type == 'cardio') ...[
                  TextField(
                    controller: _machine,
                    decoration: const InputDecoration(labelText: 'Machine'),
                  ),
                  const SizedBox(height: 10),
                  TextField(
                    controller: _duration,
                    decoration:
                        const InputDecoration(labelText: 'Duration (min)'),
                  ),
                  const SizedBox(height: 10),
                ],
                if (widget.block.type == 'strength') ...[
                  TextField(
                    controller: _setsReps,
                    decoration: const InputDecoration(
                      labelText: 'Planned reps per set',
                      hintText: '6, 6, 6, 6, 6',
                    ),
                  ),
                  const SizedBox(height: 10),
                ],
                if (widget.block.type == 'metcon') ...[
                  DropdownButtonFormField<MetconFormat?>(
                    initialValue: _format,
                    isExpanded: true,
                    decoration: const InputDecoration(labelText: 'Format'),
                    items: [
                      const DropdownMenuItem(child: Text('—')),
                      for (final format in MetconFormat.values)
                        DropdownMenuItem(
                          value: format,
                          child: Text(format.wire),
                        ),
                    ],
                    onChanged: (value) => setState(() => _format = value),
                  ),
                  const SizedBox(height: 10),
                  TextField(
                    controller: _scheme,
                    decoration: const InputDecoration(
                      labelText: 'Scheme',
                      hintText: '21, 15, 9',
                    ),
                  ),
                  const SizedBox(height: 10),
                  _movementHeader(context),
                  for (var i = 0; i < _movements.length; i++)
                    _movementRow(i),
                  Align(
                    alignment: Alignment.centerLeft,
                    child: TextButton.icon(
                      onPressed: () => setState(
                        () => _movements.add(_MovementDraft(null)),
                      ),
                      icon: const Icon(Icons.add, size: 16),
                      label: const Text('Add movement'),
                    ),
                  ),
                  const SizedBox(height: 10),
                ],
                TextField(
                  controller: _notes,
                  maxLines: 3,
                  decoration: const InputDecoration(labelText: 'Notes'),
                ),
              ],
            ),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          FilledButton(onPressed: _apply, child: const Text('Apply')),
        ],
      );
}

Future<bool> confirmRemoveWorkout(BuildContext context, String code) => _confirm(
      context,
      title: 'Remove workout',
      message: 'Remove $code from this cycle?',
      action: 'Remove',
    );

Future<bool> _confirm(
  BuildContext context, {
  required String title,
  required String message,
  required String action,
}) async =>
    await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(title),
        content: Text(message),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: Text(action),
          ),
        ],
      ),
    ) ??
    false;


class _MovementDraft {
  _MovementDraft(MetconExercise? source)
      : name = TextEditingController(text: source?.name ?? ''),
        load = TextEditingController(text: source?.load ?? ''),
        reps = TextEditingController(
          text: source?.repsOverride?.join(', ') ?? '',
        ),
        weightKg = source?.weightKg;

  final TextEditingController name;
  final TextEditingController load;
  final TextEditingController reps;

  /// Carried through untouched: the numeric load tonnage reads, which this
  /// dialog does not expose. Rebuilding movements from their names alone used
  /// to drop it.
  final double? weightKg;

  MetconExercise toExercise() {
    final override = _BlockDialogState._numbers(reps.text);
    return MetconExercise(
      name: name.text.trim(),
      load: load.text.trim().isEmpty ? null : load.text.trim(),
      weightKg: weightKg,
      repsOverride: override.isEmpty ? null : override,
    );
  }

  void dispose() {
    name.dispose();
    load.dispose();
    reps.dispose();
  }
}
