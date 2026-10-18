# Changelog

All notable changes to Stride are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [0.1.0] - 2026-10-18

The first release of Stride, with installers for Windows and Linux.

### Added

- Time tracking from a bar that stays on screen: start, pause, resume and
    submit, also with Ctrl+Space and Ctrl+Enter. The running timer survives
    closing the app, a crash or the computer going to sleep.
- Offline support: time entries are saved locally first and sent to Redmine
    once it's reachable. Entries Redmine rejects are marked with the reason and
    sent again once changed.
- Recent entries, filtered or grouped by project, with continuing earlier work
    in one click, pinned favorite issues, and editing and deleting entries.
- Adding time entries by hand, looking up the time entries of an issue, and
    creating issues in the projects you may add issues to.
- Setting the issue status when submitting time.
- Week overview of the hours per day against a daily target and per project,
    and export of a period's time entries as CSV or PDF.
- System tray icon, keyboard shortcuts, and reminders of unbooked workdays and
    long-running timers.
- The Redmine API key is kept in the OS keychain, never in plain text.
- English and German user interface.
- Portrait user interface for Android and iOS.
- Qt Installer Framework installers for Windows and Linux.

[0.1.0]: https://gitlab.com/drook207/QRedmineTimeTracker/-/releases/v0.1.0
