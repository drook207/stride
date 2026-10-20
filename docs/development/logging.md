# Logging

Stride logs through Qt's logging categories. `LogManager` (`src/Utils`)
installs a message handler that passes every message on to the console as
before, writes it to a log file, and keeps the latest 5000 in memory for the
in-app log view.

## Files

The log files are in the app's data directory:

| Platform | Directory                             |
| -------- | ------------------------------------- |
| Linux    | `~/.local/share/QRedmine/Stride/logs` |
| Windows  | `%LOCALAPPDATA%\QRedmine\Stride\logs` |

`stride.log` is the current file. At 5 MB it becomes `stride.1.log` and so
on; the 5 newest files are kept. **Export logs…** in the settings joins them,
oldest first, into one file.

## Format

One JSON object per line ([JSON Lines](https://jsonlines.org)):

```json
{"ts":"2026-10-20T20:29:47.327+02:00","level":"info","cat":"qredmine.client.network","msg":"GET /enumerations/issue_priorities.json -> 200 (24 ms)","fields":{"bytes":309,"durationMs":24,"id":3,"method":"GET","path":"/enumerations/issue_priorities.json","status":200}}
```

| Key            | Content                                                           |
| -------------- | ----------------------------------------------------------------- |
| `ts`           | Local time with milliseconds and UTC offset                       |
| `level`        | `debug`, `info`, `warning`, `critical` or `fatal`                 |
| `cat`          | Logging category, see below                                       |
| `msg`          | The message                                                       |
| `fields`       | Structured details, if any (e.g. a request's status and duration) |
| `file`, `line` | Source location, for warnings and errors only                     |

## Categories

| Category                      | What                                                                                        |
| ----------------------------- | ------------------------------------------------------------------------------------------- |
| `qredmine.app`                | Start (version, Qt, OS, locale), log level changes, quit                                    |
| `qredmine.client.network`     | One line per Redmine request: method, path, query, status, duration, size, error            |
| `qredmine.client`             | Connection state, API version, connection check and its backoff, errors shown to the user   |
| `qredmine.timetracker`        | Tracking, sleep and interruption, sending time entries, their failures and backoff, linking |
| `qredmine.content`            | Time entry comments (only at the *Debug* level)                                             |
| `qredmine.timeentry.database` | Database migrations and errors                                                              |
| `qredmine.reminders`          | Reminders shown                                                                             |
| `qml`, `js`                   | `console.*` output of the QML code (at the *Debug* level)                                   |

## Levels

The level in the settings (stored as `logging/level`) sets the category
filter rules:

| Level    | Logs                                                                        |
| -------- | --------------------------------------------------------------------------- |
| Normal   | `info` and up of `qredmine.*`, warnings and up of everything else           |
| Detailed | Normal plus `debug` of `qredmine.client.network` and `qredmine.timetracker` |
| Debug    | `debug` of all `qredmine.*` categories plus `qml` and `js`                  |

The `QT_LOGGING_RULES` environment variable is applied after the level, so
it always wins - e.g. to see a single category in detail while developing:

```bash
QT_LOGGING_RULES="qredmine.client.debug=true" ./QRedmineTimeTracker
```

The API key and request headers are never logged, nor are request or
response bodies except an error's body at the *Debug* level.

## Adding log messages

Use a category of the class and the macros from `src/Utils/logging.h`, which
attach structured fields:

```cpp
QR_INFO(lcTimeTracker, "Time entry sent", {{"localId", localId}, {"issueId", issueId}});
QR_DEBUG(lcTimeTracker, QStringLiteral("Next attempt in %1 s").arg(seconds));
```

Message and fields are only built if the category is enabled. Plain
`qCInfo()` and friends work too, without fields.

## Analyzing a log

With [jq](https://jqlang.org), e.g. the slowest requests, or how often each
path was requested:

```bash
jq -c 'select(.cat == "qredmine.client.network") | [.fields.durationMs, .msg]' stride.log | sort -rn | head
jq -r 'select(.cat == "qredmine.client.network") | .fields.path' stride.log | sort | uniq -c | sort -rn
```
