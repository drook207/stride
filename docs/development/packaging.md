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

Pushing a version tag such as `v0.1.0`, or `v0.2.0-beta.1` for a
pre-release, releases that version with GitHub Actions
(`.github/workflows/release.yml`):

1. `check-version` checks that the tag matches the version in
    `cmake/version.cmake` (`v<STRIDE_VERSION_NUMBER>`, or
    `v<STRIDE_VERSION_NUMBER>-<STRIDE_VERSION_SUFFIX>` for a pre-release)
    before anything is built.
2. `installer-linux` and `installer-windows` build the installers
    (`Stride-<version>-linux-x64.run` and `Stride-<version>-windows-x64.exe`).
3. `release-notes` writes the release notes with
    [git-cliff](https://git-cliff.org) from the commit messages: breaking
    changes (any `<type>!:`, or a `BREAKING CHANGE:` footer) come first, `feat`
    commits become *Features*, `fix` commits *Bug fixes*, other types are left
    out (see `cliff.toml`). A pre-release covers the commits since the previous
    tag, a final release those since the previous final release, so `v0.2.0`
    also lists what its betas brought. The notes are only as good as the commit
    subjects, so write them for users.
4. `release` creates the GitHub release named after the version (e.g.
    *Stride 0.2.0 Beta 1*) with the notes, with both installers attached.

A pre-release is a normal release by default, shown as the latest one that the
download links point to. To mark pre-releases as such on GitHub instead, set
the repository variable `PRERELEASES_AS_LATEST` to `false` (*Settings* ->
*Secrets and variables* -> *Actions* -> *Variables*); unset or any other value
means `true`. Final releases are always the latest.

To release a version:

1. Set the version in `cmake/version.cmake`: `STRIDE_VERSION_NUMBER` (e.g.
    `0.2.0`) and `STRIDE_VERSION_SUFFIX` (e.g. `beta.1`, empty for a final
    release). CMake, the app, the Android version and the release check all
    take it from there.

2. Update `CHANGELOG.md` with the same notes:

    ```bash
    pip install git-cliff
    git cliff --tag v0.2.0-beta.1 --output CHANGELOG.md
    ```

    For a final release, count only final tags, the same as the release
    notes; the sections of its pre-releases then merge into the final one:

    ```bash
    GIT_CLIFF__GIT__TAG_PATTERN='^v[0-9]+\.[0-9]+\.[0-9]+$' \
      git cliff --tag v0.2.0 --output CHANGELOG.md
    ```

3. Merge both to `main`, then tag that commit and push the tag:

    ```bash
    git tag -a v0.2.0 -m "Stride 0.2.0"
    git push origin v0.2.0
    ```

To try packaging changes, start the *Release* workflow by hand on a branch
(*Actions* -> *Release* -> *Run workflow*): it only builds both installers,
which are kept as artifacts of the run for a week.

The installer's resources and styling are described in
[packaging/ifw/README.md](https://github.com/drook207/stride/blob/main/src/Stride/packaging/ifw/README.md).
