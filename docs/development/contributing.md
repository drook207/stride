# Contributing

1. Fork the repository and create a feature branch.

2. Install the pre-commit hooks:

    ```bash
    pip install pre-commit
    pre-commit install
    ```

    They run clang-format, clang-tidy, cppcheck, codespell and markdownlint,
    and enforce [Conventional Commits](https://www.conventionalcommits.org)
    for commit messages.

3. Commit your changes, push the branch and open a merge request.

## Working on the documentation

This site is built with [MkDocs](https://www.mkdocs.org) and the
[Material](https://squidfunk.github.io/mkdocs-material/) theme from the
Markdown files in `docs/`; `mkdocs.yml` holds the configuration and the
navigation. Preview it locally with live reload:

```bash
pip install -r docs/requirements.txt
mkdocs serve
```

and open <http://127.0.0.1:8000>. CI builds it with `mkdocs build --strict`,
so broken links and missing pages fail the pipeline.

The screenshots live in `docs/assets/screenshots/`. Take them from a build
running against the [local Redmine](test-environment.md) in English
(e.g. `LANG=en_US.UTF-8`), with the window at 1280×800 for desktop shots.
