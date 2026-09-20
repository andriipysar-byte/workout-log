import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../app_model.dart';
import 'muscle_group_legend.dart';
import 'theme.dart';

class CalendarView extends StatefulWidget {
  const CalendarView({super.key});

  @override
  State<CalendarView> createState() => _CalendarViewState();
}

class _CalendarViewState extends State<CalendarView> {
  DateTime? _anchor;

  /// Opens on the month of the most recent session so data is visible immediately.
  DateTime _resolveAnchor(AppModel model) {
    if (_anchor != null) return _anchor!;
    final latest = model.calendar.keys.toList()..sort();
    final parsed = latest.isEmpty ? null : DateTime.tryParse(latest.last);
    return _anchor = parsed ?? DateTime.now();
  }

  void _shiftMonth(int delta) => setState(() {
        final anchor = _anchor ?? DateTime.now();
        _anchor = DateTime(anchor.year, anchor.month + delta);
      });

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    final anchor = _resolveAnchor(model);
    final localizations = MaterialLocalizations.of(context);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Row(
          children: [
            IconButton(
              icon: const Icon(Icons.chevron_left),
              onPressed: () => _shiftMonth(-1),
            ),
            Expanded(
              child: Text(
                localizations.formatMonthYear(anchor),
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.titleSmall,
              ),
            ),
            IconButton(
              icon: const Icon(Icons.chevron_right),
              onPressed: () => _shiftMonth(1),
            ),
          ],
        ),
        _WeekdayRow(localizations: localizations),
        const SizedBox(height: 4),
        _MonthGrid(anchor: anchor, model: model, localizations: localizations),
        const SizedBox(height: 10),
        const MuscleGroupLegend(),
      ],
    );
  }
}

class _WeekdayRow extends StatelessWidget {
  const _WeekdayRow({required this.localizations});

  final MaterialLocalizations localizations;

  @override
  Widget build(BuildContext context) => Row(
        children: [
          for (var i = 0; i < 7; i++)
            Expanded(
              child: Text(
                localizations.narrowWeekdays[
                    (localizations.firstDayOfWeekIndex + i) % 7],
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.labelSmall,
              ),
            ),
        ],
      );
}

class _MonthGrid extends StatelessWidget {
  const _MonthGrid({
    required this.anchor,
    required this.model,
    required this.localizations,
  });

  final DateTime anchor;
  final AppModel model;
  final MaterialLocalizations localizations;

  @override
  Widget build(BuildContext context) {
    final monthStart = DateTime(anchor.year, anchor.month);
    final daysInMonth = DateTime(anchor.year, anchor.month + 1, 0).day;

    // Flutter's firstDayOfWeekIndex is 0-based from Sunday while DateTime.weekday
    // is 1 = Monday, so both are normalised to "days after Sunday" before the
    // offset is taken.
    final firstWeekdayFromSunday = monthStart.weekday % 7;
    final leading =
        (firstWeekdayFromSunday - localizations.firstDayOfWeekIndex + 7) % 7;

    return GridView.count(
      crossAxisCount: 7,
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      mainAxisSpacing: 4,
      crossAxisSpacing: 4,
      childAspectRatio: 1.05,
      children: [
        for (var i = 0; i < leading; i++) const SizedBox.shrink(),
        for (var day = 1; day <= daysInMonth; day++)
          _DayCell(
            date: DateTime(anchor.year, anchor.month, day),
            model: model,
          ),
      ],
    );
  }
}

class _DayCell extends StatelessWidget {
  const _DayCell({required this.date, required this.model});

  final DateTime date;
  final AppModel model;

  static String _key(DateTime date) =>
      '${date.year.toString().padLeft(4, '0')}-'
      '${date.month.toString().padLeft(2, '0')}-'
      '${date.day.toString().padLeft(2, '0')}';

  @override
  Widget build(BuildContext context) {
    final info = model.calendar[_key(date)];
    final selected = info != null && model.selection == info.id;
    final scheme = Theme.of(context).colorScheme;

    return InkWell(
      onTap: info == null ? null : () => model.open(info.id),
      borderRadius: BorderRadius.circular(6),
      child: Container(
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(6),
          color: selected
              ? scheme.primary.withValues(alpha: 0.18)
              : Colors.transparent,
          border: Border.all(
            color: selected ? scheme.primary : Colors.transparent,
          ),
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text(
              '${date.day}',
              style: Theme.of(context).textTheme.labelSmall?.copyWith(
                    color: info == null ? scheme.outline : scheme.onSurface,
                  ),
            ),
            if (info != null)
              Container(
                margin: const EdgeInsets.only(top: 1),
                padding:
                    const EdgeInsets.symmetric(horizontal: 5, vertical: 1),
                decoration: BoxDecoration(
                  color: info.group?.color ?? unclassifiedDayColor,
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Text(
                  info.cycleDay,
                  maxLines: 1,
                  overflow: TextOverflow.clip,
                  style: const TextStyle(
                    fontSize: 10,
                    fontWeight: FontWeight.bold,
                    color: Colors.white,
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }
}
