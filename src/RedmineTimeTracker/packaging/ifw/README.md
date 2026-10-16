# Qt Installer Framework installer

On Windows and Linux, `cpack` builds an offline installer with the
[Qt Installer Framework](https://doc.qt.io/qtinstallerframework/) (QtIFW).
The configuration lives in the CPack section of
`src/RedmineTimeTracker/CMakeLists.txt`. This directory holds its resources.

## Building the installer

QtIFW must be installed, e.g. with the Qt Maintenance Tool (component
*Qt Installer Framework* under *Developer and Designer Tools*). CPack finds
it under the usual Qt install locations. Otherwise, point it at the QtIFW
directory (the one containing `bin`) via `-DCPACK_IFW_ROOT=<dir>` or the
`QTIFWDIR` environment variable.

```bash
cmake --preset linux-release
cmake --build --preset linux-release
cd build/linux-release && cpack
```

On Linux, `cpack -G TGZ` (or `-G DEB` / `-G RPM`) still builds a plain
archive or native package instead.

## Look & feel

The installer matches the application's Qt Quick Controls Material Dark
theme and accent color (see `qml/Main.qml`):

- `style.qss` - Qt style sheet recoloring every wizard widget. Its header
  comment lists the palette and where each color comes from.
- `banner.png` - plain dark header background, used as the `Modern` wizard
  style's banner. The thin separator lines below the header and above the
  buttons are painted by Qt from fixed palette colors and can't be styled.
- `resources.qrc`, `check.png` - images referenced by the style sheet
  (`url(:/qredmine/...)`), compiled into the installer binary.
- `controlscript.qs` - hides the component selection page, since the
  installer contains a single mandatory component.
- `logo.png`, `window-icon.png` - rendered from
  `qml/Assets/Icons/app-icon.svg`.

To regenerate the images after changing the app icon or palette:

```bash
rsvg-convert -w 40 -h 40 ../../qml/Assets/Icons/app-icon.svg -o logo.png
rsvg-convert -w 256 -h 256 ../../qml/Assets/Icons/app-icon.svg -o window-icon.png
# check.png: a #1C1B1F check mark on transparent background, 28x28
magick -size 1200x120 xc:'#1C1B1F' banner.png
```

## Shortcuts

`installscript.qs` is the component script of the single
`QRedmineTimeTracker` package. It creates a Start menu entry (plus an
uninstall entry) on Windows and a per-user desktop entry in
`~/.local/share/applications` on Linux. The uninstaller removes both again.

## Install locations

- Windows: `C:\Program Files\QRedmineTimeTracker` (requires elevation, the
  installer asks for it).
- Linux: `~/QRedmineTimeTracker` (per user, no root privileges needed).
