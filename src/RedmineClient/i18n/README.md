# Translations

The `QRedmine::Client` library's own user-facing strings - currently the
messages emitted via `RedmineClient::errorOccurred()` - are marked with
`tr()` and extracted into these Qt Linguist `.ts` files, one per language.
They are compiled to `.qm` at build time and embedded into the library
under the `:/i18n/` resource prefix, next to (but separate from) the
application's own `QRedmineTimeTracker_<lang>.qm` catalogs.

Applications using the library load the catalog matching a locale with:

```cpp
QRedmineClient::RedmineClient::installTranslations(QLocale::system());
```

Log output (`qDebug`/`qWarning`) is intentionally _not_ translated.

## Supported languages

- `QRedmineClient_en.ts` - English (the source language).
- `QRedmineClient_de.ts` - German.

## Updating translations

The workflow is the same as for the application, see
[src/RedmineTimeTracker/i18n/README.md](../../RedmineTimeTracker/i18n/README.md):
the `update_translations` target refreshes the `.ts` files of both the
library and the application. To add a language, add
`i18n/QRedmineClient_<lang>.ts` to the `TS_FILES` list in this library's
`CMakeLists.txt` as well.
