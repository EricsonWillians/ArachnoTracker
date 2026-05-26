#include "GUI.h"

#include <cctype>
#include <map>
#include <sstream>
#include <vector>

#include "AppActions.h"
#include "GuiInput.h"

namespace arachno {

namespace {

std::vector<std::string> splitShellLike(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    char quoteChar = '"';

    for (char ch : line) {
        if (inQuotes) {
            if (ch == quoteChar) {
                inQuotes = false;
            } else {
                current.push_back(ch);
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            inQuotes = true;
            quoteChar = ch;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::map<std::string, std::string> parseKeyValueArgs(
    const std::vector<std::string>& args,
    std::size_t startIndex) {
    std::map<std::string, std::string> out;
    for (std::size_t index = startIndex; index < args.size(); ++index) {
        const std::string& token = args[index];
        const std::size_t eq = token.find('=');
        if (eq == std::string::npos || eq == 0) {
            continue;
        }
        out[token.substr(0, eq)] = token.substr(eq + 1);
    }
    return out;
}

void printShellHelp(std::ostream& output) {
    output
        << "ArachnoTracker GUI Shell\n"
        << "Commands:\n"
        << "  help\n"
        << "  status\n"
        << "  palette [query]\n"
        << "  schema <action_id>\n"
        << "  do <action_id> [key=value ...]\n"
        << "  sync [checkpoint_name=name] [mode=...] [stale_policy=...] [max_events=N]\n"
        << "  events [since=N] [drain=true|false] [max_events=N] [include_snapshot=true|false]\n"
        << "  play | pause | stop | preview\n"
        << "  quit | exit\n";
}

void printPrompt(std::ostream& output, const ApplicationSession& session) {
    const EditorCursor& cursor = session.editor().cursor();
    output
        << "gui "
        << (session.dirty() ? "*" : "-")
        << " p" << cursor.pattern
        << " r" << cursor.row
        << " t" << cursor.track
        << "> ";
}

AppActionResult executeAndPrint(
    ApplicationSession& session,
    std::ostream& output,
    const AppActionRequest& request) {
    const AppActionResult result = executeAppAction(session, request);
    output << renderApplicationActionResult(result);
    return result;
}

} // namespace

int runGuiShell(
    ApplicationSession& session,
    std::istream& input,
    std::ostream& output,
    const GuiShellOptions& options) {
    printShellHelp(output);
    AppActionRequest initialSync;
    initialSync.actionId = "session.sync";
    initialSync.parameters = {
        {"checkpoint_name", options.checkpointName},
        {"create_if_missing", options.createCheckpointIfMissing ? "true" : "false"},
        {"mode", "force_snapshot"},
        {"max_events", std::to_string(options.maxEvents)},
        {"snapshot_grid_start_row", std::to_string(options.snapshotGridStartRow)},
        {"snapshot_grid_row_count", std::to_string(options.snapshotGridRowCount)},
        {"update_checkpoint", "true"}};
    (void)executeAndPrint(session, output, initialSync);

    std::string line;
    while (true) {
        printPrompt(output, session);
        if (!std::getline(input, line)) {
            return 0;
        }
        const std::string trimmed = trimCopy(line);
        if (trimmed.empty()) {
            continue;
        }

        const std::vector<std::string> args = splitShellLike(trimmed);
        if (args.empty()) {
            continue;
        }

        const std::string command = lowerCopy(args.front());
        if (command == "quit" || command == "exit") {
            output << "Exiting GUI shell.\n";
            return 0;
        }

        if (command == "help") {
            printShellHelp(output);
            continue;
        }

        if (command == "status") {
            AppActionRequest status;
            status.actionId = "session.snapshot";
            status.parameters = {
                {"grid_start_row", std::to_string(options.snapshotGridStartRow)},
                {"grid_row_count", std::to_string(options.snapshotGridRowCount)}};
            (void)executeAndPrint(session, output, status);
            continue;
        }

        if (command == "palette") {
            const std::string query = args.size() >= 2 ? trimmed.substr(trimmed.find(' ') + 1) : "";
            output << renderApplicationActionPalette(buildApplicationActionPalette(session, query));
            continue;
        }

        if (command == "schema") {
            if (args.size() < 2) {
                output << "error: schema requires an action id\n";
                continue;
            }
            output << renderAppActionSchema(buildAppActionSchema(args[1]));
            continue;
        }

        if (command == "play" || command == "pause" || command == "stop" || command == "preview") {
            AppActionRequest request;
            if (command == "play") {
                request.actionId = "playback.play";
            } else if (command == "pause") {
                request.actionId = "playback.pause";
            } else if (command == "stop") {
                request.actionId = "playback.stop";
            } else {
                request.actionId = "preview.cursor";
            }
            (void)executeAndPrint(session, output, request);
            continue;
        }

        if (command == "events") {
            AppActionRequest request;
            request.actionId = "session.events";
            request.parameters = parseKeyValueArgs(args, 1);
            if (request.parameters.find("max_events") == request.parameters.end()) {
                request.parameters["max_events"] = std::to_string(options.maxEvents);
            }
            (void)executeAndPrint(session, output, request);
            continue;
        }

        if (command == "sync") {
            AppActionRequest request;
            request.actionId = "session.sync";
            request.parameters = parseKeyValueArgs(args, 1);
            if (request.parameters.find("checkpoint_name") == request.parameters.end()) {
                request.parameters["checkpoint_name"] = options.checkpointName;
            }
            if (request.parameters.find("create_if_missing") == request.parameters.end()) {
                request.parameters["create_if_missing"] = options.createCheckpointIfMissing ? "true" : "false";
            }
            if (request.parameters.find("max_events") == request.parameters.end()) {
                request.parameters["max_events"] = std::to_string(options.maxEvents);
            }
            if (request.parameters.find("snapshot_grid_start_row") == request.parameters.end()) {
                request.parameters["snapshot_grid_start_row"] = std::to_string(options.snapshotGridStartRow);
            }
            if (request.parameters.find("snapshot_grid_row_count") == request.parameters.end()) {
                request.parameters["snapshot_grid_row_count"] = std::to_string(options.snapshotGridRowCount);
            }
            (void)executeAndPrint(session, output, request);
            continue;
        }

        if (command == "do") {
            if (args.size() < 2) {
                output << "error: do requires an action id\n";
                continue;
            }
            AppActionRequest request;
            request.actionId = args[1];
            request.parameters = parseKeyValueArgs(args, 2);
            const std::map<std::string, std::string>::const_iterator pathIt = request.parameters.find("path");
            if (pathIt != request.parameters.end()) {
                request.path = pathIt->second;
            }
            (void)executeAndPrint(session, output, request);
            continue;
        }

        output << "error: unknown command\n";
    }
}

} // namespace arachno
