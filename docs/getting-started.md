# Getting started

## Install

Stride is built from source with CMake and Qt 6.8 or newer - see
[Building](development/building.md) for the requirements and the exact
commands. In short:

```bash
git clone https://gitlab.com/drook207/QRedmineTimeTracker.git
cd QRedmineTimeTracker
export QT_DIR=/path/to/Qt/6.x.y/gcc_64
cmake --preset linux-release
cmake --build --preset linux-release
./build/linux-release/src/RedmineTimeTracker/QRedmineTimeTracker
```

On Windows and Linux, `cpack` can also build an installer - see
[Installer](development/packaging.md).

## Connect to Redmine

Open **Settings** from the sidebar (or press ++ctrl+6++) and fill in
**Connection Settings**:

1. **Redmine URL** - the base URL of your Redmine instance, e.g.
   `https://redmine.example.com/`.
2. **API Key** - found on your Redmine account page under *My account* ->
   *API access key*. If it's missing there, an administrator has to enable
   *Enable REST web service* under *Administration* -> *Settings* -> *API*.
   The key is written straight to the OS-native secure storage (e.g. Windows
   Credential Manager, Secret Service/KWallet on Linux, the Keychain on Apple
   platforms, the Android Keystore) and never stored in plain text.
3. **Use SSL** - enable it if your Redmine URL uses `https://`.

The indicator in the header shows the connection state: green **Online**,
amber **Reconnecting** or red **Offline**.

![Settings view](assets/screenshots/desktop-settings.png)

## Track your first entry

1. Type an issue number or search for an issue in the **Issue** field of the
   bar at the bottom of the window.
2. Press play (or ++ctrl+space++) to start the timer.
3. Add a **Comment**, pick an **Activity**, and press **+** (or
   ++ctrl+enter++) to submit.

The entry appears under **Recent Entries** and is sent to Redmine right away,
or as soon as Redmine is reachable again. The [User guide](user-guide.md)
covers the rest.
