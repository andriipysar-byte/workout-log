// Scriptable editor for cycles.json (workoutlog::cycle_plan, core/include/workoutlog/cycle_plan.hpp)
// -- exercises the cycle CRUD API on the command line, ahead of either UI editor.
// A session or block is edited as a JSON fragment (read from a file or stdin via
// "-"), not through per-field flags: cycles.json's block shape has nine optional
// fields, and a flag surface that size would be harder to use correctly than
// "dump it, edit the JSON, feed it back" -- the same loop wl_fmt already assumes
// for the files themselves.
//
// Usage:
//   wl_cycle list [--cycle <id>]
//   wl_cycle show-session --cycle <id> --index <n>
//   wl_cycle show-block --cycle <id> --session <n> --index <n>
//   wl_cycle insert-session --cycle <id> --index <n> --json <path|->
//   wl_cycle replace-session --cycle <id> --index <n> --json <path|->
//   wl_cycle remove-session --cycle <id> --index <n>
//   wl_cycle move-session --cycle <id> --from <n> --to <n>
//   wl_cycle insert-block --cycle <id> --session <n> --index <n> --json <path|->
//   wl_cycle replace-block --cycle <id> --session <n> --index <n> --json <path|->
//   wl_cycle remove-block --cycle <id> --session <n> --index <n>
//   wl_cycle move-block --cycle <id> --session <n> --from <n> --to <n>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "workoutlog/cycle_plan.hpp"
#include "workoutlog/json.hpp"
#include "workoutlog/paths.hpp"

namespace {

using workoutlog::cycle_plan::Cycle;
using workoutlog::cycle_plan::File;

std::string read_stream(std::istream& in) {
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string read_json_arg(const std::string& path_or_dash) {
    if (path_or_dash == "-") return read_stream(std::cin);
    std::ifstream in(path_or_dash, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path_or_dash);
    return read_stream(in);
}

std::size_t parse_index(const std::string& raw, const char* flag) {
    try {
        std::size_t pos = 0;
        long v = std::stol(raw, &pos);
        if (pos != raw.size() || v < 0) throw std::invalid_argument("");
        return static_cast<std::size_t>(v);
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("--") + flag + " must be a non-negative integer, got \"" + raw + "\"");
    }
}

// Simple `--flag value` parser; every flag is looked up by name so a missing one
// reads as absent rather than shifting the rest of the args (CLI input is untrusted
// like any other boundary -- AGENTS.md 1.2.1).
std::map<std::string, std::string> parse_flags(int argc, char** argv, int start) {
    std::map<std::string, std::string> flags;
    for (int i = start; i < argc; i += 2) {
        std::string key = argv[i];
        if (key.size() < 3 || key[0] != '-' || key[1] != '-')
            throw std::runtime_error("expected a --flag, got \"" + key + "\"");
        if (i + 1 >= argc) throw std::runtime_error("\"" + key + "\" is missing its value");
        flags[key.substr(2)] = argv[i + 1];
    }
    return flags;
}

std::string require_flag(const std::map<std::string, std::string>& flags, const char* name) {
    auto it = flags.find(name);
    if (it == flags.end()) throw std::runtime_error(std::string("missing --") + name);
    return it->second;
}

Cycle& require_cycle(File& file, const std::string& id) {
    if (auto* c = workoutlog::cycle_plan::find_cycle(file, id)) return *c;
    throw std::runtime_error("no cycle with id \"" + id + "\"");
}

void print_usage() {
    std::cerr << "usage: wl_cycle <command> [--flag value ...]\n"
                  "commands:\n"
                  "  list [--cycle <id>]\n"
                  "  show-session --cycle <id> --index <n>\n"
                  "  show-block --cycle <id> --session <n> --index <n>\n"
                  "  insert-session --cycle <id> --index <n> --json <path|->\n"
                  "  replace-session --cycle <id> --index <n> --json <path|->\n"
                  "  remove-session --cycle <id> --index <n>\n"
                  "  move-session --cycle <id> --from <n> --to <n>\n"
                  "  insert-block --cycle <id> --session <n> --index <n> --json <path|->\n"
                  "  replace-block --cycle <id> --session <n> --index <n> --json <path|->\n"
                  "  remove-block --cycle <id> --session <n> --index <n>\n"
                  "  move-block --cycle <id> --session <n> --from <n> --to <n>\n";
}

void cmd_list(File& file, const std::map<std::string, std::string>& flags) {
    auto it = flags.find("cycle");
    if (it == flags.end()) {
        for (const auto& c : file.cycles)
            std::cout << c.id << "  \"" << c.name << "\"  (" << c.sessions.size() << " session(s))\n";
        return;
    }
    const auto& sessions = require_cycle(file, it->second).sessions;
    for (std::size_t i = 0; i < sessions.size(); i++) {
        const auto& s = sessions[i];
        std::cout << i << "  " << s.cycle_day << "  " << workoutlog::cycle_plan::to_string(s.type);
        if (s.title) std::cout << "  \"" << *s.title << "\"";
        std::cout << "\n";
    }
}

void cmd_show_session(File& file, const std::map<std::string, std::string>& flags) {
    auto& cycle = require_cycle(file, require_flag(flags, "cycle"));
    auto index = parse_index(require_flag(flags, "index"), "index");
    if (index >= cycle.sessions.size())
        throw std::runtime_error("session index " + std::to_string(index) + " out of range (size " +
                                  std::to_string(cycle.sessions.size()) + ")");
    std::cout << workoutlog::json::encode_cycle_plan_session(cycle.sessions[index]);
}

void cmd_show_block(File& file, const std::map<std::string, std::string>& flags) {
    auto& cycle = require_cycle(file, require_flag(flags, "cycle"));
    auto session_index = parse_index(require_flag(flags, "session"), "session");
    if (session_index >= cycle.sessions.size())
        throw std::runtime_error("session index " + std::to_string(session_index) + " out of range (size " +
                                  std::to_string(cycle.sessions.size()) + ")");
    const auto& blocks = cycle.sessions[session_index].blocks;
    auto index = parse_index(require_flag(flags, "index"), "index");
    if (index >= blocks.size())
        throw std::runtime_error("block index " + std::to_string(index) + " out of range (size " +
                                  std::to_string(blocks.size()) + ")");
    std::cout << workoutlog::json::encode_cycle_plan_block(blocks[index]);
}

// Dispatches every session/block-mutating command, or throws -- including for an
// unrecognised command name, checked up front so a typo reports itself instead of
// first demanding whatever flag the block commands below it happen to require.
void run_mutation(const std::string& command, File& file, const std::map<std::string, std::string>& flags) {
    static const std::vector<std::string> kSessionCommands = {"insert-session", "replace-session", "remove-session",
                                                                "move-session"};
    static const std::vector<std::string> kBlockCommands = {"insert-block", "replace-block", "remove-block",
                                                              "move-block"};
    auto is = [&command](const std::vector<std::string>& names) {
        return std::find(names.begin(), names.end(), command) != names.end();
    };
    if (!is(kSessionCommands) && !is(kBlockCommands)) throw std::runtime_error("unknown command \"" + command + "\"");

    auto& cycle = require_cycle(file, require_flag(flags, "cycle"));

    if (command == "insert-session") {
        auto index = parse_index(require_flag(flags, "index"), "index");
        auto session = workoutlog::json::decode_cycle_plan_session(read_json_arg(require_flag(flags, "json")));
        workoutlog::cycle_plan::insert_session(cycle, index, std::move(session));
        return;
    }
    if (command == "replace-session") {
        auto index = parse_index(require_flag(flags, "index"), "index");
        if (index >= cycle.sessions.size())
            throw std::runtime_error("session index " + std::to_string(index) + " out of range (size " +
                                      std::to_string(cycle.sessions.size()) + ")");
        cycle.sessions[index] = workoutlog::json::decode_cycle_plan_session(read_json_arg(require_flag(flags, "json")));
        return;
    }
    if (command == "remove-session") {
        workoutlog::cycle_plan::remove_session(cycle, parse_index(require_flag(flags, "index"), "index"));
        return;
    }
    if (command == "move-session") {
        workoutlog::cycle_plan::move_session(cycle, parse_index(require_flag(flags, "from"), "from"),
                                              parse_index(require_flag(flags, "to"), "to"));
        return;
    }

    auto session_index = parse_index(require_flag(flags, "session"), "session");
    if (session_index >= cycle.sessions.size())
        throw std::runtime_error("session index " + std::to_string(session_index) + " out of range (size " +
                                  std::to_string(cycle.sessions.size()) + ")");
    auto& session = cycle.sessions[session_index];

    if (command == "insert-block") {
        auto index = parse_index(require_flag(flags, "index"), "index");
        auto block = workoutlog::json::decode_cycle_plan_block(read_json_arg(require_flag(flags, "json")));
        workoutlog::cycle_plan::insert_block(session, index, std::move(block));
        return;
    }
    if (command == "replace-block") {
        auto index = parse_index(require_flag(flags, "index"), "index");
        if (index >= session.blocks.size())
            throw std::runtime_error("block index " + std::to_string(index) + " out of range (size " +
                                      std::to_string(session.blocks.size()) + ")");
        session.blocks[index] = workoutlog::json::decode_cycle_plan_block(read_json_arg(require_flag(flags, "json")));
        return;
    }
    if (command == "remove-block") {
        workoutlog::cycle_plan::remove_block(session, parse_index(require_flag(flags, "index"), "index"));
        return;
    }
    // command == "move-block": the only member of kBlockCommands not handled above.
    workoutlog::cycle_plan::move_block(session, parse_index(require_flag(flags, "from"), "from"),
                                        parse_index(require_flag(flags, "to"), "to"));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 2;
    }
    std::string command = argv[1];
    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }

    try {
        auto repo_root = workoutlog::paths::resolve_repo_root();
        workoutlog::cycle_plan::CyclePlanStore store(workoutlog::paths::cycles_path(repo_root));
        auto flags = parse_flags(argc, argv, 2);
        auto file = store.load();

        if (command == "list") {
            cmd_list(file, flags);
            return 0;
        }
        if (command == "show-session") {
            cmd_show_session(file, flags);
            return 0;
        }
        if (command == "show-block") {
            cmd_show_block(file, flags);
            return 0;
        }

        run_mutation(command, file, flags);
        store.save(file);
        std::cout << "wrote " << store.path().string() << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
