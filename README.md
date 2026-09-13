# Redmine Time Tracker

A desktop application built with Qt 6 for tracking time spent on Redmine issues, for Linux and Windows. Features automatic time tracking, offline support, and seamless integration with Redmine's REST API.

## Features

### Time Tracking

- Start/Stop/Pause functionality
- Real-time elapsed time display
- Automatic time calculation
- Pause and resume capabilities
- Visual tracking status indicators

### Redmine Integration

- Full issue and project management (create, update, list, delete)
- Automatic synchronization with Redmine
- Create, update, and delete time entries directly from the app
- View and manage existing time entries
- API key stored in the OS-native secure storage (Windows Credential Manager, Linux Secret Service/KWallet) - never in plain text
- Automatic Redmine REST API version detection, so newer features can be gated on server capability
- Error handling, connection-state indicator, and retry mechanisms

### Offline Support

- SQLite database for local storage
- Automatic sync when online
- Track time without internet connection
- Persistent history of time entries
- Sync status tracking

## Requirements

- Qt 6.8 or higher
- CMake 3.16 or higher (3.23+ to use the provided CMake Presets)
- C++17 compatible compiler
- SQLite
- Linux, or Windows with MSVC or MinGW
- Internet access on first configure - GoogleTest and [QtKeychain](https://github.com/frankosterfeld/qtkeychain) are fetched automatically via CMake `FetchContent`

## Building

Building uses the CMake Presets in `CMakePresets.json`:

```bash
git clone https://gitlab.com/drook207/QRedmineTimeTracker.git
cd QRedmineTimeTracker

# Point at your Qt installation, e.g. .../Qt/6.8.0/gcc_64 (Linux),
# .../Qt/6.8.0/msvc2022_64 or .../Qt/6.8.0/mingw_64 (Windows)
export QT_DIR=/path/to/Qt/6.x.y/<kit>

cmake --preset linux-debug
cmake --build --preset linux-debug
```

Run `cmake --list-presets` for the full list - `linux-{debug,release,relwithdebinfo}`,
`windows-msvc-{debug,release,relwithdebinfo}` (run from an *x64 Native Tools Command
Prompt for VS*, or after calling `vcvarsall.bat x64`, so `cl.exe` is on `PATH`), and
`windows-mingw-{debug,release,relwithdebinfo}`. Only the presets matching your current
OS are shown.

## Testing

Unit and integration tests (GoogleTest) live in `tests/`. Build and run them directly:

```bash
cmake --build --preset linux-debug --target qredminetesting
./build/linux-debug/tests/qredminetesting
```

For manual testing against a real server, a disposable local Redmine instance
(with demo projects/issues/users/time entries) is available via Docker - see
[docker/README.md](docker/README.md).

## Configuration

1. Launch the application
2. Enter your Redmine URL in the settings
3. Add your API key from Redmine - it's written straight to the OS-native secure storage,
   never persisted in plain text
4. (Optional) Configure default activity types

## Usage

1. Enter an issue ID
2. Click "Start Tracking" to begin timing
3. Use Pause/Resume as needed
4. Add comments about the work done
5. Stop tracking when finished
6. Time entry will be saved locally and synced to Redmine

## Database Schema

Time entries are stored in SQLite with the following structure:

- `id`: Primary key
- `issue_id`: Redmine issue identifier
- `hours`: Time spent in decimal hours
- `comments`: Work description
- `spent_on`: Date/time of work
- `sync_status`: Synchronization state
- `redmine_id`: ID from Redmine after sync
- `created_at`: Local creation timestamp
- `updated_at`: Last modification timestamp

## Contributing

1. Fork the repository
2. Create a feature branch
3. Install the pre-commit hooks: `pip install pre-commit && pre-commit install`
   (runs clang-format/clang-tidy/cppcheck/codespell/markdownlint, and enforces
   [Conventional Commits](https://www.conventionalcommits.org) on commit messages)
4. Commit your changes
5. Push to the branch
6. Create a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.
