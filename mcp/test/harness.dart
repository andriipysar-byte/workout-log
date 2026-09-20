import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:dart_mcp/client.dart';
import 'package:stream_channel/stream_channel.dart';
import 'package:test/test.dart';
import 'package:workout_log_mcp/workout_log_mcp.dart';

/// A throwaway repository: the real reference files (they are the source of
/// truth, so a fixture copy of the catalogue would only test the copy) over a
/// temporary session folder, so no test can write into `data/`.
class TestRepo {
  TestRepo() {
    root = Directory.systemTemp.createTempSync('workout_log_mcp');
    Directory('${root.path}/data').createSync();
    for (final name in const ['exercises.json', 'cycles.json']) {
      File('${_repoRoot.path}/$name').copySync('${root.path}/$name');
    }
    addTearDown(() => root.deleteSync(recursive: true));
  }

  static final Directory _repoRoot = _findRepoRoot();

  static Directory _findRepoRoot() {
    var dir = Directory.current.absolute;
    while (true) {
      if (File('${dir.path}/cycles.json').existsSync()) return dir;
      final parent = dir.parent;
      if (parent.path == dir.path) {
        throw StateError('repo root not found from ${Directory.current.path}');
      }
      dir = parent;
    }
  }

  late final Directory root;

  Workspace get workspace => Workspace.open(root: root.path);

  File sessionFile(String id) => File('${root.path}/data/$id');

  void writeSession(String id, Map<String, dynamic> session) =>
      sessionFile(id).writeAsStringSync(jsonEncode(session));

  Map<String, dynamic> readSession(String id) =>
      jsonDecode(sessionFile(id).readAsStringSync()) as Map<String, dynamic>;

  List<String> get sessionIds =>
      (Directory('${root.path}/data').listSync().whereType<File>().toList()
            ..sort((a, b) => a.path.compareTo(b.path)))
          .map((f) => f.uri.pathSegments.last)
          .toList();
}

/// A client talking to the server over an in-memory channel pair — the same
/// shape as stdio, without a process.
class TestClient {
  TestClient(TestRepo repo) {
    final client = _Client();
    final toClient = StreamController<String>();
    final toServer = StreamController<String>();
    _server = WorkoutLogServer(
      StreamChannel.withCloseGuarantee(toServer.stream, toClient.sink),
      workspace: repo.workspace,
    );
    _connection = client.connectServer(
      StreamChannel.withCloseGuarantee(toClient.stream, toServer.sink),
    );
    _ready = _connection
        .initialize(InitializeRequest(
          protocolVersion: ProtocolVersion.latestSupported,
          capabilities: client.capabilities,
          clientInfo: client.implementation,
        ))
        .then((_) => _connection.notifyInitialized(InitializedNotification()));
    addTearDown(() async {
      await client.shutdown();
      await _server.shutdown();
    });
  }

  late final WorkoutLogServer _server;
  late final ServerConnection _connection;
  late final Future<void> _ready;

  Future<List<Tool>> tools() async {
    await _ready;
    return (await _connection.listTools()).tools;
  }

  /// The tool's JSON payload, with a failure surfaced as a test failure: a tool
  /// that reports an error returns text, not JSON, and silently decoding it
  /// would hide the message.
  Future<Map<String, dynamic>> call(
    String name, [
    Map<String, Object?> arguments = const {},
  ]) async {
    final result = await raw(name, arguments);
    final text = (result.content.single as TextContent).text;
    if (result.isError ?? false) fail('$name failed: $text');
    return jsonDecode(text) as Map<String, dynamic>;
  }

  Future<String> error(
    String name, [
    Map<String, Object?> arguments = const {},
  ]) async {
    final result = await raw(name, arguments);
    final text = (result.content.single as TextContent).text;
    expect(result.isError, isTrue, reason: 'expected $name to fail: $text');
    return text;
  }

  Future<CallToolResult> raw(
    String name,
    Map<String, Object?> arguments,
  ) async {
    await _ready;
    return _connection.callTool(
      CallToolRequest(name: name, arguments: arguments),
    );
  }
}

base class _Client extends MCPClient {
  _Client() : super(Implementation(name: 'test client', version: '1.0.0'));
}
