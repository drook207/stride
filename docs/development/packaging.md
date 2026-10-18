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

## Releases

Pushing a version tag such as `v0.1.0` releases that version from GitLab CI:

1. `installer-linux` and `installer-windows` build the installers and upload
    them to the project's package registry.
2. `release-notes` writes the release notes with
    [git-cliff](https://git-cliff.org) from the commit messages since the
    previous tag: `feat` commits become *Features*, `fix` commits *Bug fixes*,
    other types are left out (see `cliff.toml`). The notes are only as good as
    the commit subjects, so write them for users.
3. `release` checks that the tag matches the version in the root
    `CMakeLists.txt` and creates the GitLab release with the notes and the
    installers as downloads.

To release a version:

1. Set the version in the root `CMakeLists.txt` (`project(QRedmine VERSION …)`).

2. Update `CHANGELOG.md` with the same notes:

    ```bash
    pip install git-cliff
    git cliff --tag v0.1.0 --output CHANGELOG.md
    ```

3. Merge both to `main`, then tag that commit and push the tag:

    ```bash
    git tag -a v0.1.0 -m "Stride 0.1.0"
    git push origin v0.1.0
    ```

On merge requests, both installer jobs can be started by hand to try
packaging changes; their installers are kept as job artifacts for a week.

The installer's resources and styling are described in
[packaging/ifw/README.md](https://gitlab.com/drook207/QRedmineTimeTracker/-/blob/main/src/RedmineTimeTracker/packaging/ifw/README.md).
