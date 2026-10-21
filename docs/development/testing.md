# Testing

## Unit and integration tests

The GoogleTest suite in `tests/` covers the Redmine client and the app's
storage and tracking logic, on an in-memory database. Build and run it with:

```bash
cmake --build --preset linux-debug --target stride-tests
./build/linux-debug/tests/stride-tests
```

## qmllint

The QML must stay free of qmllint warnings:

```bash
cmake --build --preset linux-debug --target all_qmllint
```

## Continuous integration

GitHub Actions (`.github/workflows/ci.yml`) runs on every pull request and
on `main`:

- the pre-commit hooks,
- the build, the tests and qmllint, on Ubuntu 24.04 with Qt 6.11,
- a strict build of this documentation.

On pull requests, the build only runs when code (`src/`, `tests/`, the CMake
files or the workflows) changed, and the documentation only when `docs/`,
`mkdocs.yml` or the workflows changed. The test results and the qmllint log
are kept as artifacts of the run for a week.

On `main`, the `deploy-pages` job also publishes this documentation to
GitHub Pages.

## Manual testing

To try the app against a real server, start the disposable
[local Redmine](test-environment.md) with demo data.
