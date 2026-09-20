import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';

import '../app_model.dart';
import 'cycle_view.dart';
import 'new_session_dialog.dart';
import 'plan_view.dart';
import 'session_editor.dart';
import 'theme.dart';

enum SidebarTab {
  list('List'),
  cycle('Cycle'),
  plan('Plan');

  const SidebarTab(this.label);

  final String label;
}

class RootView extends StatefulWidget {
  const RootView({super.key});

  @override
  State<RootView> createState() => _RootViewState();
}

class _RootViewState extends State<RootView> {
  SidebarTab _tab = SidebarTab.list;

  Future<void> _newSession(AppModel model) async {
    final created = await showNewSessionDialog(context, model);
    if (created == null) return;
    await model.create(created);
    if (mounted) setState(() => _tab = SidebarTab.list);
  }

  Future<void> _deleteSession(AppModel model) async {
    final id = model.selection;
    if (id == null) return;
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Delete session'),
        content: Text('Delete $id? This removes the file.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (confirmed == true) await model.delete(id);
  }

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();

    // Both modifiers are bound so the shortcut is ⌘S on Apple platforms and
    // Ctrl-S on Windows and Linux.
    return Shortcuts(
      shortcuts: const {
        SingleActivator(LogicalKeyboardKey.keyS, meta: true): _SaveIntent(),
        SingleActivator(LogicalKeyboardKey.keyS, control: true): _SaveIntent(),
      },
      child: Actions(
        actions: {
          _SaveIntent: CallbackAction<_SaveIntent>(
            onInvoke: (_) => model.session == null ? null : model.save(),
          ),
        },
        child: Focus(
          autofocus: true,
          child: LayoutBuilder(
            builder: (context, constraints) {
              final wide = constraints.maxWidth >= wideLayoutBreakpoint;
              final sidebar = _Sidebar(
                tab: _tab,
                onTabChanged: (tab) => setState(() => _tab = tab),
                onNew: () => _newSession(model),
              );

              return Scaffold(
                appBar: AppBar(
                  title: Text(switch (_tab) {
                    SidebarTab.cycle => 'Cycle',
                    SidebarTab.plan => 'Plan',
                    SidebarTab.list => 'Sessions',
                  }),
                  actions: [
                    if (model.canChooseFolder)
                      IconButton(
                        tooltip: 'Choose session folder',
                        icon: const Icon(Icons.folder_open),
                        onPressed: model.pickFolder,
                      ),
                    IconButton(
                      tooltip: 'Import session files',
                      icon: const Icon(Icons.file_upload_outlined),
                      onPressed: model.importSessionFiles,
                    ),
                    IconButton(
                      tooltip: 'Export all sessions',
                      icon: const Icon(Icons.file_download_outlined),
                      onPressed: model.exportSessionFiles,
                    ),
                    IconButton(
                      tooltip: 'Reload folder',
                      icon: const Icon(Icons.refresh),
                      onPressed: () => model.refresh(fromDisk: true),
                    ),
                    if (model.selection != null && _tab != SidebarTab.plan)
                      IconButton(
                        tooltip: 'Delete session',
                        icon: const Icon(Icons.delete_outline),
                        onPressed: () => _deleteSession(model),
                      ),
                    if (model.session != null && _tab != SidebarTab.plan)
                      IconButton(
                        tooltip: 'Save (⌘S)',
                        icon: const Icon(Icons.save),
                        onPressed: model.save,
                      ),
                  ],
                ),
                drawer: wide ? null : Drawer(child: SafeArea(child: sidebar)),
                bottomNavigationBar: const _StatusBar(),
                body: wide
                    ? Row(
                        children: [
                          SizedBox(width: sidebarWidth, child: sidebar),
                          const VerticalDivider(width: 1),
                          Expanded(child: _Detail(tab: _tab)),
                        ],
                      )
                    : _Detail(tab: _tab),
              );
            },
          ),
        ),
      ),
    );
  }
}

class _SaveIntent extends Intent {
  const _SaveIntent();
}

class _Sidebar extends StatelessWidget {
  const _Sidebar({
    required this.tab,
    required this.onTabChanged,
    required this.onNew,
  });

  final SidebarTab tab;
  final ValueChanged<SidebarTab> onTabChanged;
  final VoidCallback onNew;

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.all(8),
          child: Row(
            children: [
              Expanded(
                child: SegmentedButton<SidebarTab>(
                  showSelectedIcon: false,
                  style:
                      const ButtonStyle(visualDensity: VisualDensity.compact),
                  segments: [
                    for (final tab in SidebarTab.values)
                      ButtonSegment(value: tab, label: Text(tab.label)),
                  ],
                  selected: {tab},
                  onSelectionChanged: (s) => onTabChanged(s.first),
                ),
              ),
              IconButton(
                tooltip: 'New session',
                icon: const Icon(Icons.edit_note),
                onPressed: onNew,
              ),
            ],
          ),
        ),
        const Divider(height: 1),
        Expanded(
          child: model.files.isEmpty
              ? Center(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Text(
                      model.ready
                          ? 'No sessions here yet.'
                          : 'Opening folder…',
                      textAlign: TextAlign.center,
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                )
              : ListView.builder(
                  itemCount: model.files.length,
                  itemBuilder: (context, index) {
                    final id = model.files[index];
                    final label =
                        id.endsWith('.json') ? id.substring(0, id.length - 5) : id;
                    return ListTile(
                      dense: true,
                      selected: model.selection == id,
                      title: Text(label, overflow: TextOverflow.ellipsis),
                      onTap: () {
                        model.open(id);
                        if (Scaffold.of(context).hasDrawer &&
                            Scaffold.of(context).isDrawerOpen) {
                          Navigator.pop(context);
                        }
                      },
                    );
                  },
                ),
        ),
      ],
    );
  }
}

class _Detail extends StatelessWidget {
  const _Detail({required this.tab});

  final SidebarTab tab;

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    if (tab == SidebarTab.cycle) return const CycleView();
    if (tab == SidebarTab.plan) return const PlanView();

    final session = model.session;
    if (session == null) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.edit_note, size: 40),
            const SizedBox(height: 8),
            Text(
              'Select a session to edit',
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ],
        ),
      );
    }
    return SessionEditor(key: ValueKey(model.selection), session: session);
  }
}

class _StatusBar extends StatelessWidget {
  const _StatusBar();

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    final text = model.status.isEmpty ? model.folderLabel : model.status;
    return Material(
      color: Theme.of(context).colorScheme.surfaceContainerHighest,
      child: SafeArea(
        top: false,
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
          child: Row(
            children: [
              Expanded(
                child: Text(
                  text,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ),
              if (model.isWorkingCopy)
                Tooltip(
                  message:
                      'The browser holds a working copy (ADR-007); export to '
                      'write back to your folder.',
                  child: Text(
                    'working copy',
                    style: Theme.of(context).textTheme.labelSmall,
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }
}
