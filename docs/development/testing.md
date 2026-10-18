# Testing

## Unit and integration tests

The GoogleTest suite in `tests/` covers the Redmine client and the app's
storage and tracking logic, on an in-memory database. Build and run it with:

```bash
cmake --build --preset linux-debug --target qredminetesting
./build/linux-debug/tests/qredminetesting
```

## qmllint

The QML must stay free of qmllint warnings:

```bash
cmake --build --preset linux-debug --target all_qmllint
```

## Continuous integration

GitLab CI (`.gitlab-ci.yml`) runs on every merge request and on `main`:

- the pre-commit hooks,
- the build, the tests and qmllint,
- a strict build of this documentation.

On `main`, the `pages` job also publishes this documentation to GitLab Pages.

## Manual testing

To try the app against a real server, start the disposable
[local Redmine](test-environment.md) with demo data.
