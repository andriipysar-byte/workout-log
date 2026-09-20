import 'package:flutter/material.dart';
import 'package:workout_log_core/workout_log_core.dart';

import '../app_model.dart';

/// Create a session: blank, or expanded from a `cycles.json` template.
///
/// Returns the session to write, or null if the user backed out — including
/// when they decline to overwrite an existing file.
Future<Session?> showNewSessionDialog(
  BuildContext context,
  AppModel model,
) =>
    showDialog<Session>(
      context: context,
      builder: (_) => _NewSessionDialog(model: model),
    );

class _NewSessionDialog extends StatefulWidget {
  const _NewSessionDialog({required this.model});

  final AppModel model;

  @override
  State<_NewSessionDialog> createState() => _NewSessionDialogState();
}

class _NewSessionDialogState extends State<_NewSessionDialog> {
  late final _date = TextEditingController(
    text: CycleGenerator.isoDate(DateTime.now()),
  );
  final _cycleDay = TextEditingController();

  Cycle? _cycle;
  CycleSession? _template;
  String? _error;

  @override
  void initState() {
    super.initState();
    _cycle = widget.model.cycles.firstOrNull;
  }

  @override
  void dispose() {
    _date.dispose();
    _cycleDay.dispose();
    super.dispose();
  }

  /// Picking a template also moves the date and cycle day to that template's
  /// scheduled slot, which is the answer the user wants nine times in ten.
  void _selectTemplate(CycleSession? template) {
    setState(() {
      _template = template;
      _error = null;
      if (template == null) return;
      _cycleDay.text = template.cycleDay;
      final cycle = _cycle;
      if (cycle == null) return;
      final index = cycle.sessions.indexOf(template);
      try {
        final dates = CycleGenerator.trainingDates(
          DateTime.parse(cycle.startDate),
          cycle.trainingDays,
          cycle.sessions.length,
        );
        _date.text = CycleGenerator.isoDate(dates[index]);
      } on Object {
        // A cycle whose dates cannot be derived still allows a manual date.
      }
    });
  }

  Future<void> _create() async {
    final date = _date.text.trim();
    final cycleDay = _cycleDay.text.trim();
    if (!RegExp(r'^\d{4}-\d{2}-\d{2}$').hasMatch(date)) {
      setState(() => _error = 'Date must be YYYY-MM-DD.');
      return;
    }
    if (cycleDay.isEmpty) {
      setState(() => _error = 'Cycle day is required.');
      return;
    }

    Session session;
    final template = _template;
    if (template == null) {
      session = Session(date: date, cycleDay: cycleDay);
    } else {
      try {
        session = CycleGenerator.sessionFromTemplate(
          template,
          DateTime.parse(date),
          enforceWeekday: false,
        )..cycleDay = cycleDay;
      } on CycleGeneratorException catch (error) {
        setState(() => _error = '$error');
        return;
      }
    }

    final id = SessionStore.idFor(session);
    if (await widget.model.exists(id)) {
      if (!mounted) return;
      final overwrite = await showDialog<bool>(
        context: context,
        builder: (_) => AlertDialog(
          title: const Text('Session already exists'),
          content: Text('$id is already in this folder. Overwrite it?'),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(context, true),
              child: const Text('Overwrite'),
            ),
          ],
        ),
      );
      if (overwrite != true) return;
    }

    if (!mounted) return;
    Navigator.pop(context, session);
  }

  @override
  Widget build(BuildContext context) {
    final cycles = widget.model.cycles;
    return AlertDialog(
      title: const Text('New session'),
      content: SizedBox(
        width: 420,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            if (cycles.isNotEmpty) ...[
              DropdownButtonFormField<Cycle>(
                initialValue: _cycle,
                // The catalogue's names are Ukrainian and routinely wider than
                // the dialog; expanding lets the label ellipsize instead of
                // overflowing the row.
                isExpanded: true,
                decoration: const InputDecoration(labelText: 'Cycle'),
                items: [
                  for (final cycle in cycles)
                    DropdownMenuItem(value: cycle, child: Text(cycle.name)),
                ],
                onChanged: (cycle) => setState(() {
                  _cycle = cycle;
                  _template = null;
                }),
              ),
              const SizedBox(height: 10),
              DropdownButtonFormField<CycleSession?>(
                initialValue: _template,
                isExpanded: true,
                decoration: const InputDecoration(labelText: 'Template'),
                items: [
                  const DropdownMenuItem(
                    child: Text('Blank session'),
                  ),
                  for (final template in _cycle?.sessions ?? const [])
                    DropdownMenuItem(
                      value: template,
                      child: Text(
                        '${template.cycleDay} · ${template.title ?? 'session'}',
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                ],
                onChanged: _selectTemplate,
              ),
              const SizedBox(height: 10),
            ],
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _date,
                    decoration: const InputDecoration(
                      labelText: 'Date',
                      hintText: 'YYYY-MM-DD',
                    ),
                  ),
                ),
                const SizedBox(width: 10),
                SizedBox(
                  width: 110,
                  child: TextField(
                    controller: _cycleDay,
                    decoration: const InputDecoration(
                      labelText: 'Cycle day',
                      hintText: 'A1',
                    ),
                  ),
                ),
              ],
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
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
        FilledButton(onPressed: _create, child: const Text('Create')),
      ],
    );
  }
}

extension<T> on List<T> {
  T? get firstOrNull => isEmpty ? null : first;
}
