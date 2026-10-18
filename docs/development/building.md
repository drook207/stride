# Building

## Requirements

- Qt 6.8 or newer, including the `LinguistTools` component for the
    translations
- CMake 3.16 or newer (3.25+ to use the CMake presets)
- A C++17 compiler
- SQLite
- Linux, or Windows with MSVC or MinGW
- Internet access on the first configure - GoogleTest and
    [QtKeychain](https://github.com/frankosterfeld/qtkeychain) are fetched with
    CMake `FetchContent`

## Build with the CMake presets

```bash
git clone https://gitlab.com/drook207/QRedmineTimeTracker.git
cd QRedmineTimeTracker

# Point at your Qt installation, e.g. .../Qt/6.8.0/gcc_64 (Linux),
# .../Qt/6.8.0/msvc2022_64 or .../Qt/6.8.0/mingw_64 (Windows)
export QT_DIR=/path/to/Qt/6.x.y/<kit>

cmake --preset linux-debug
cmake --build --preset linux-debug
```

The app is then at `build/linux-debug/src/RedmineTimeTracker/QRedmineTimeTracker`.

`cmake --list-presets` lists the presets for the current OS:

| Platform       | Presets                                                                        |
| -------------- | ------------------------------------------------------------------------------ |
| Linux          | `linux-debug`, `linux-release`, `linux-relwithdebinfo`                         |
| Windows, MSVC  | `windows-msvc-debug`, `windows-msvc-release`, `windows-msvc-relwithdebinfo`    |
| Windows, MinGW | `windows-mingw-debug`, `windows-mingw-release`, `windows-mingw-relwithdebinfo` |

Run the MSVC presets from an *x64 Native Tools Command Prompt for VS*, or
after calling `vcvarsall.bat x64`, so `cl.exe` is on the `PATH`.

## Where the app keeps its data

- **Settings** - QSettings, under the organization `QRedmine` and the
    application `Stride` (e.g. `~/.config/QRedmine/Stride.conf` on Linux).
- **Time entries** - an SQLite database with versioned schema migrations, in
    the platform's application data directory (e.g.
    `~/.local/share/QRedmine/Stride/timeentries.db` on Linux).
- **API key** - the OS-native secure storage via QtKeychain.
