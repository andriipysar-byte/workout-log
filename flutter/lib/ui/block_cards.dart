import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../app_model.dart';
import 'muscle_map.dart';
import 'theme.dart';

class BlockCard extends StatelessWidget {
  const BlockCard({required this.block, required this.onChanged, super.key});

  final Block block;
  final VoidCallback onChanged;

  @override
  Widget build(BuildContext context) {
    // Bound to a local because Dart promotes locals, not fields — and the
    // exhaustive switch over the sealed hierarchy is the point of the port.
    final block = this.block;
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      color: cardSurface(context),
      elevation: 0,
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: switch (block) {
          CardioBlock() => CardioEditor(block: block, onChanged: onChanged),
          StrengthBlock() => StrengthEditor(block: block, onChanged: onChanged),
          MetconBlock() => MetconCard(block: block),
          CooldownBlock() => CooldownEditor(block: block, onChanged: onChanged),
        },
      ),
    );
  }
}

class BlockHeader extends StatelessWidget {
  const BlockHeader({required this.icon, required this.title, super.key});

  final IconData icon;
  final String title;

  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.only(bottom: 8),
        child: Row(
          children: [
            Icon(icon, size: 18, color: Theme.of(context).colorScheme.primary),
            const SizedBox(width: 6),
            Expanded(
              child: Text(
                title,
                style: Theme.of(context).textTheme.titleSmall,
                overflow: TextOverflow.ellipsis,
              ),
            ),
          ],
        ),
      );
}

/// A text field over a `String?` model field, mapping empty text to null so the
/// key stays out of the file rather than being written as `""`.
class OptionalField extends StatefulWidget {
  const OptionalField({
    required this.label,
    required this.value,
    required this.onChanged,
    this.width,
    this.monospaced = false,
    super.key,
  });

  final String label;
  final String? value;
  final ValueChanged<String?> onChanged;
  final double? width;
  final bool monospaced;

  @override
  State<OptionalField> createState() => _OptionalFieldState();
}

class _OptionalFieldState extends State<OptionalField> {
  late final TextEditingController _controller =
      TextEditingController(text: widget.value ?? '');

  @override
  void didUpdateWidget(OptionalField old) {
    super.didUpdateWidget(old);
    final incoming = widget.value ?? '';
    if (incoming != _controller.text && !_hasFocus) _controller.text = incoming;
  }

  bool _hasFocus = false;

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final field = Focus(
      onFocusChange: (has) => _hasFocus = has,
      child: TextField(
        controller: _controller,
        decoration: InputDecoration(hintText: widget.label),
        style: widget.monospaced
            ? const TextStyle(fontFamily: 'monospace')
            : null,
        onChanged: (text) => widget.onChanged(text.isEmpty ? null : text),
      ),
    );
    return widget.width == null
        ? field
        : SizedBox(width: widget.width, child: field);
  }
}

class NumberField extends StatelessWidget {
  const NumberField({
    required this.label,
    required this.value,
    required this.onChanged,
    this.width,
    super.key,
  });

  final String label;
  final double? value;
  final ValueChanged<double?> onChanged;
  final double? width;

  @override
  Widget build(BuildContext context) => OptionalField(
        label: label,
        width: width,
        value: value == null ? null : _format(value!),
        onChanged: (text) =>
            onChanged(text == null ? null : double.tryParse(text)),
      );

  static String _format(double value) =>
      value == value.roundToDouble() ? value.toInt().toString() : '$value';
}

class CardioEditor extends StatelessWidget {
  const CardioEditor({required this.block, required this.onChanged, super.key});

  final CardioBlock block;
  final VoidCallback onChanged;

  @override
  Widget build(BuildContext context) => Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const BlockHeader(icon: Icons.directions_run, title: 'Cardio'),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              OptionalField(
                label: 'machine',
                width: 200,
                value: block.machine.isEmpty ? null : block.machine,
                onChanged: (text) {
                  block.machine = text ?? '';
                  onChanged();
                },
              ),
              NumberField(
                label: 'min',
                width: 80,
                value: block.durationMin,
                onChanged: (v) {
                  block.durationMin = v;
                  onChanged();
                },
              ),
              NumberField(
                label: 'distance m',
                width: 110,
                value: block.distanceM,
                onChanged: (v) {
                  block.distanceM = v;
                  onChanged();
                },
              ),
              OptionalField(
                label: 'end',
                width: 80,
                value: block.endTime,
                onChanged: (v) {
                  block.endTime = v;
                  onChanged();
                },
              ),
            ],
          ),
        ],
      );
}

class CooldownEditor extends StatelessWidget {
  const CooldownEditor({
    required this.block,
    required this.onChanged,
    super.key,
  });

  final CooldownBlock block;
  final VoidCallback onChanged;

  @override
  Widget build(BuildContext context) => Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const BlockHeader(icon: Icons.air, title: 'Cooldown'),
          Row(
            children: [
              OptionalField(
                label: 'end',
                width: 90,
                value: block.endTime,
                onChanged: (v) {
                  block.endTime = v;
                  onChanged();
                },
              ),
              const SizedBox(width: 8),
              Expanded(
                child: OptionalField(
                  label: 'notes',
                  value: block.notes,
                  onChanged: (v) {
                    block.notes = v;
                    onChanged();
                  },
                ),
              ),
            ],
          ),
        ],
      );
}

/// Read-only by design: metcon rounds and splits are transcribed from paper, and
/// an editor for them is not in scope.
class MetconCard extends StatelessWidget {
  const MetconCard({required this.block, super.key});

  final MetconBlock block;

  @override
  Widget build(BuildContext context) {
    final captionStyle = Theme.of(context).textTheme.bodySmall;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const BlockHeader(icon: Icons.local_fire_department, title: 'Metcon'),
        Text('format: ${block.format?.wire ?? '—'}', style: captionStyle),
        if (block.scheme != null)
          Text('scheme: ${block.scheme!.join('-')}', style: captionStyle),
        for (final exercise in block.exercises)
          Padding(
            padding: const EdgeInsets.only(top: 2),
            child: Row(
              children: [
                Text('•', style: captionStyle),
                const SizedBox(width: 6),
                Flexible(
                  child: Text(exercise.name, overflow: TextOverflow.ellipsis),
                ),
                if (exercise.weightKg != null) ...[
                  const SizedBox(width: 6),
                  Text('(${exercise.weightKg!.toInt()} kg)',
                      style: captionStyle),
                ],
              ],
            ),
          ),
        if (block.rounds != null)
          Padding(
            padding: const EdgeInsets.only(top: 4),
            child: Text(
              'rounds: ${block.rounds!.length}',
              style: captionStyle,
            ),
          ),
        if (block.notes != null)
          Padding(
            padding: const EdgeInsets.only(top: 4),
            child: Text(block.notes!, style: captionStyle),
          ),
      ],
    );
  }
}

class StrengthEditor extends StatefulWidget {
  const StrengthEditor({
    required this.block,
    required this.onChanged,
    super.key,
  });

  final StrengthBlock block;
  final VoidCallback onChanged;

  @override
  State<StrengthEditor> createState() => _StrengthEditorState();
}

class _StrengthEditorState extends State<StrengthEditor> {
  final _notation = TextEditingController();
  bool _showMap = false;

  ParsedSets get _parsed => Notation.parseStrengthSets(_notation.text);

  @override
  void dispose() {
    _notation.dispose();
    super.dispose();
  }

  void _apply() {
    final parsed = _parsed;
    if (parsed.sets.isEmpty) return;
    setState(() {
      widget.block.sets = parsed.sets;
      _notation.clear();
    });
    widget.onChanged();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final parsed = _parsed;
    final model = context.read<AppModel>();

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        BlockHeader(icon: Icons.fitness_center, title: widget.block.exercise),
        if (widget.block.notes != null)
          Text(widget.block.notes!, style: theme.textTheme.bodySmall),
        Padding(
          padding: const EdgeInsets.symmetric(vertical: 6),
          child: widget.block.sets.isEmpty
              ? Text('no sets yet', style: theme.textTheme.bodySmall)
              : Text(
                  widget.block.sets.map(setSummary).join('   '),
                  style: const TextStyle(fontFamily: 'monospace'),
                ),
        ),
        Row(
          children: [
            Expanded(
              child: TextField(
                controller: _notation,
                style: const TextStyle(fontFamily: 'monospace'),
                decoration: const InputDecoration(
                  hintText: '6 × [70, 80, 90, 100, 110]',
                ),
                onChanged: (_) => setState(() {}),
                onSubmitted: (_) => _apply(),
              ),
            ),
            const SizedBox(width: 8),
            FilledButton(
              onPressed: parsed.sets.isEmpty ? null : _apply,
              child: const Text('Apply'),
            ),
          ],
        ),
        if (_notation.text.isNotEmpty) ...[
          const SizedBox(height: 4),
          Text(
            '→ ${parsed.sets.isEmpty ? '—' : parsed.sets.map(setSummary).join('  ')}',
            style: theme.textTheme.bodySmall?.copyWith(
              color: parsed.sets.isEmpty ? Colors.red : Colors.green.shade700,
            ),
          ),
          for (final warning in parsed.warnings)
            Row(
              children: [
                const Icon(Icons.warning_amber, size: 13, color: Colors.orange),
                const SizedBox(width: 4),
                Expanded(
                  child: Text(
                    warning,
                    style: theme.textTheme.labelSmall
                        ?.copyWith(color: Colors.orange.shade800),
                  ),
                ),
              ],
            ),
        ],
        // Built only when expanded, so a long session does not colourize one SVG
        // per exercise up front.
        ExpansionTile(
          title: Text('Muscle map', style: theme.textTheme.bodySmall),
          tilePadding: EdgeInsets.zero,
          onExpansionChanged: (open) => setState(() => _showMap = open),
          children: [
            if (_showMap)
              switch (model.exerciseMapSVG(widget.block.exercise)) {
                final String svg => MuscleMap(svg: svg, height: 200),
                null => MuscleMapUnavailable(
                    '"${widget.block.exercise}" not in catalogue — no map.',
                  ),
              },
          ],
        ),
      ],
    );
  }
}

/// A set as one monospaced token: cluster chains, timed holds, then
/// `reps×weight` with `bw` for bodyweight and a trailing `*` for back-offs.
String setSummary(WorkSet set) {
  final cluster = set.cluster;
  if (cluster != null) {
    final total = set.totalReps;
    return cluster.join('+') + (total == null ? '' : ' ($total)');
  }
  final seconds = set.durationSec;
  if (seconds != null) return '${seconds.toInt()}c';
  final reps = set.reps == null ? '' : '${set.reps}×';
  final weight = set.weightKg == null ? 'bw' : '${set.weightKg!.toInt()}';
  final backoff = set.isBackoff == true ? '*' : '';
  return '$reps$weight$backoff';
}
