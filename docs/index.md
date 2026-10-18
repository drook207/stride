# Stride

Stride (QRedmineTimeTracker) is a Qt 6 app for tracking the time you spend on
[Redmine](https://www.redmine.org) issues. It runs on Linux and Windows, with a
dedicated portrait UI for Android and iOS, and keeps working while Redmine is
unreachable.

![Recent entries on the desktop](assets/screenshots/desktop-recent-entries.png)

## Highlights

- **Start, pause and submit** with one bar that's always on screen - or with
    ++ctrl+space++ and ++ctrl+enter++.
- **Nothing gets lost**: the running timer survives closing the app or a crash,
    and entries are saved locally first, then sent to Redmine once it's reachable.
- **Week overview** of hours per day against your daily target, with the hours
    still missing and the hours per project.
- **Export** the time entries of any period as CSV or PDF.
- **Continue earlier work** with one click, and pin favorite issues.
- **Set the issue status** (e.g. to *Resolved*) when submitting time.
- **System tray** with pause/resume and favorites, plus reminders of unbooked
    workdays and long-running timers.
- **Secure**: the API key is kept in the OS keychain, never in plain text.
- **English and German**, picked from the system locale or chosen in Settings.

| Overview                                             | Add time entry                                                   |
| ---------------------------------------------------- | ---------------------------------------------------------------- |
| ![Overview](assets/screenshots/desktop-overview.png) | ![Add time entry](assets/screenshots/desktop-add-time-entry.png) |

| Mobile                                                                                | Mobile, tracking panel expanded                                                          |
| ------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| ![Mobile recent entries](assets/screenshots/mobile-recent-entries.png){ width="300" } | ![Mobile tracking panel](assets/screenshots/mobile-tracking-expanded.png){ width="300" } |

## Where to go next

- [Getting started](getting-started.md) - build the app and connect it to Redmine.
- [User guide](user-guide.md) - every view, shortcut and setting.
- [Development](development/building.md) - building, testing and contributing.
