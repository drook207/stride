# Installer

On Windows and Linux, `cpack` builds an offline installer with the
[Qt Installer Framework](https://doc.qt.io/qtinstallerframework/) (QtIFW),
styled to match the app's dark theme.

Install QtIFW, e.g. with the Qt Maintenance Tool (*Developer and Designer
Tools* -> *Qt Installer Framework*). CPack finds it in the usual Qt install
locations; otherwise pass `-DCPACK_IFW_ROOT=<dir>` or set `QTIFWDIR`.

```bash
cmake --preset linux-release
cmake --build --preset linux-release
cd build/linux-release && cpack
```

On Linux, `cpack -G TGZ`, `-G DEB` or `-G RPM` builds a plain archive or a
native package instead.

The installer's resources and styling are described in
[packaging/ifw/README.md](https://gitlab.com/drook207/QRedmineTimeTracker/-/blob/main/src/RedmineTimeTracker/packaging/ifw/README.md).
