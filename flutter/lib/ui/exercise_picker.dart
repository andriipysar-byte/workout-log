import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../app_model.dart';
import 'theme.dart';

/// Choose an exercise from the catalogue, or add one that is not in it yet.
///
/// Returns the canonical name, so the plan always stores something the muscle
/// map can resolve.
Future<String?> pickExercise(BuildContext context, {String? current}) =>
    showDialog<String>(
      context: context,
      builder: (_) => _ExercisePicker(current: current),
    );

class _ExercisePicker extends StatefulWidget {
  const _ExercisePicker({this.current});

  final String? current;

  @override
  State<_ExercisePicker> createState() => _ExercisePickerState();
}

class _ExercisePickerState extends State<_ExercisePicker> {
  final _query = TextEditingController();

  @override
  void dispose() {
    _query.dispose();
    super.dispose();
  }

  List<Exercise> _matches(Catalogue catalogue) {
    final query = _query.text.trim().toLowerCase();
    if (query.isEmpty) return catalogue.exercises;
    return catalogue.exercises
        .where((e) =>
            e.name.toLowerCase().contains(query) ||
            e.aliases.any((a) => a.toLowerCase().contains(query)))
        .toList();
  }

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    final catalogue = model.catalogue;
    final matches = catalogue == null ? <Exercise>[] : _matches(catalogue);

    return AlertDialog(
      title: const Text('Choose exercise'),
      content: SizedBox(
        width: 460,
        height: 420,
        child: Column(
          children: [
            TextField(
              controller: _query,
              autofocus: true,
              decoration: const InputDecoration(
                hintText: 'search name or alias',
                prefixIcon: Icon(Icons.search),
              ),
              onChanged: (_) => setState(() {}),
            ),
            const SizedBox(height: 8),
            Expanded(
              child: matches.isEmpty
                  ? Center(
                      child: Text(
                        catalogue == null
                            ? 'Catalogue unavailable.'
                            : 'Nothing matches — use “New exercise”.',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    )
                  : ListView.builder(
                      itemCount: matches.length,
                      itemBuilder: (context, index) {
                        final exercise = matches[index];
                        final groups = model.primaryGroups(exercise.name);
                        return ListTile(
                          dense: true,
                          selected: exercise.name == widget.current,
                          leading: _GroupDot(groups: groups),
                          title: Text(exercise.name,
                              overflow: TextOverflow.ellipsis),
                          subtitle: Text(
                            exercise.primaryMuscles.join(', '),
                            overflow: TextOverflow.ellipsis,
                          ),
                          onTap: () => Navigator.pop(context, exercise.name),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
        TextButton(
          onPressed: model.canEditPlan
              ? () async {
                  final created = await showNewExerciseDialog(
                    context,
                    model,
                    suggestedName: _query.text.trim(),
                  );
                  if (created != null && context.mounted) {
                    Navigator.pop(context, created);
                  }
                }
              : null,
          child: const Text('New exercise'),
        ),
      ],
    );
  }
}

class _GroupDot extends StatelessWidget {
  const _GroupDot({required this.groups});

  final List<MuscleGroup> groups;

  @override
  Widget build(BuildContext context) => Container(
        width: 12,
        height: 12,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color: groups.isEmpty
              ? Theme.of(context).colorScheme.outlineVariant
              : groups.first.color,
        ),
      );
}

/// Add an exercise to `exercises.json`. Muscles are required: without them the
/// exercise would resolve but contribute nothing to any muscle map.
Future<String?> showNewExerciseDialog(
  BuildContext context,
  AppModel model, {
  String suggestedName = '',
}) =>
    showDialog<String>(
      context: context,
      builder: (_) => _NewExerciseDialog(model: model, initialName: suggestedName),
    );

class _NewExerciseDialog extends StatefulWidget {
  const _NewExerciseDialog({required this.model, required this.initialName});

  final AppModel model;
  final String initialName;

  @override
  State<_NewExerciseDialog> createState() => _NewExerciseDialogState();
}

class _NewExerciseDialogState extends State<_NewExerciseDialog> {
  late final _name = TextEditingController(text: widget.initialName);
  final _aliases = TextEditingController();
  final _primary = <String>{};
  final _secondary = <String>{};
  MovementPattern? _pattern;
  Modality? _modality;
  ExerciseCategory _category = ExerciseCategory.strength;
  String? _error;

  @override
  void dispose() {
    _name.dispose();
    _aliases.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    final name = _name.text.trim();
    if (name.isEmpty) {
      setState(() => _error = 'A name is required.');
      return;
    }
    if (widget.model.catalogue?.resolve(name) != null) {
      setState(() => _error = '"$name" already resolves to an exercise.');
      return;
    }
    if (_primary.isEmpty) {
      setState(() => _error = 'Pick at least one primary muscle.');
      return;
    }
    await widget.model.addExercise(
      Exercise(
        name: name,
        aliases: _aliases.text
            .split(',')
            .map((a) => a.trim())
            .where((a) => a.isNotEmpty)
            .toList(),
        pattern: _pattern,
        modality: _modality,
        category: _category,
        primaryMuscles: _primary.toList()..sort(),
        secondaryMuscles: _secondary.toList()..sort(),
      ),
    );
    if (mounted) Navigator.pop(context, name);
  }

  @override
  Widget build(BuildContext context) {
    final muscles = widget.model.catalogue?.knownMuscles ?? const <String>[];
    return AlertDialog(
      title: const Text('New exercise'),
      content: SizedBox(
        width: 520,
        child: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              TextField(
                controller: _name,
                decoration: const InputDecoration(
                  labelText: 'Canonical name',
                  hintText: 'присід фронтальний',
                ),
              ),
              const SizedBox(height: 10),
              TextField(
                controller: _aliases,
                decoration: const InputDecoration(
                  labelText: 'Aliases',
                  hintText: 'comma separated, optional',
                ),
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  Expanded(
                    child: DropdownButtonFormField<ExerciseCategory>(
                      initialValue: _category,
                      isExpanded: true,
                      decoration: const InputDecoration(labelText: 'Category'),
                      items: [
                        for (final c in ExerciseCategory.values)
                          DropdownMenuItem(value: c, child: Text(c.name)),
                      ],
                      onChanged: (c) =>
                          setState(() => _category = c ?? _category),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: DropdownButtonFormField<MovementPattern?>(
                      initialValue: _pattern,
                      isExpanded: true,
                      decoration: const InputDecoration(labelText: 'Pattern'),
                      items: [
                        const DropdownMenuItem(child: Text('—')),
                        for (final p in MovementPattern.values)
                          DropdownMenuItem(value: p, child: Text(p.name)),
                      ],
                      onChanged: (p) => setState(() => _pattern = p),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: DropdownButtonFormField<Modality?>(
                      initialValue: _modality,
                      isExpanded: true,
                      decoration: const InputDecoration(labelText: 'Modality'),
                      items: [
                        const DropdownMenuItem(child: Text('—')),
                        for (final m in Modality.values)
                          DropdownMenuItem(value: m, child: Text(m.name)),
                      ],
                      onChanged: (m) => setState(() => _modality = m),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              _MusclePicker(
                title: 'Primary muscles',
                muscles: muscles,
                selected: _primary,
                onChanged: () => setState(() {}),
              ),
              const SizedBox(height: 8),
              _MusclePicker(
                title: 'Secondary muscles',
                muscles: muscles,
                selected: _secondary,
                onChanged: () => setState(() {}),
              ),
              if (_error != null) ...[
                const SizedBox(height: 10),
                Text(
                  _error!,
                  style: TextStyle(color: Theme.of(context).colorScheme.error),
                ),
              ],
            ],
          ),
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
        FilledButton(onPressed: _save, child: const Text('Add')),
      ],
    );
  }
}

class _MusclePicker extends StatelessWidget {
  const _MusclePicker({
    required this.title,
    required this.muscles,
    required this.selected,
    required this.onChanged,
  });

  final String title;
  final List<String> muscles;
  final Set<String> selected;
  final VoidCallback onChanged;

  @override
  Widget build(BuildContext context) => Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(title, style: Theme.of(context).textTheme.labelMedium),
          const SizedBox(height: 4),
          Wrap(
            spacing: 6,
            runSpacing: 2,
            children: [
              for (final muscle in muscles)
                FilterChip(
                  label: Text(muscle, style: const TextStyle(fontSize: 11)),
                  visualDensity: VisualDensity.compact,
                  selected: selected.contains(muscle),
                  selectedColor:
                      MuscleGroup.of(muscle)?.color.withValues(alpha: 0.35),
                  onSelected: (on) {
                    on ? selected.add(muscle) : selected.remove(muscle);
                    onChanged();
                  },
                ),
            ],
          ),
        ],
      );
}
