# Translations

UI strings are marked with `qsTr()` in QML and extracted into Qt Linguist
`.ts` files, one per language, compiled to `.qm` at build time and embedded
into the binary under the `:/i18n/` resource prefix. `LanguageManager`
(created in `main.cpp`) installs the `QTranslator` for the language chosen
under _Settings > Appearance > Language_, persisted as `ui/language` in
QSettings, or for `QLocale::system()` when "System default" is selected. It
falls back to the English source strings when no matching catalog is found.
Changing the language retranslates the running UI immediately. The language
list is built from the embedded catalogs, so a newly added language shows up
there automatically.

The `QRedmine::Client` library ships a separate catalog for its own
user-facing strings (e.g. network error messages) in
[src/RedmineClient/i18n](../../RedmineClient/i18n), which `LanguageManager`
installs via `RedmineClient::installTranslations()`.

Building translations requires the `LinguistTools` component of Qt (the
`lupdate`/`lrelease` tools) - on most Linux distributions this is a separate
package, e.g. `qt6-tools` on Arch/CachyOS, `qt6-tools-dev-tools` on Debian/
Ubuntu.

## Supported languages

- `Stride_en.ts` - English (the source language; effectively
    unused at runtime since untranslated strings already fall back to the
    English `qsTr()` source text, but kept for symmetry and so English shows
    up explicitly in tooling like Qt Linguist).
- `Stride_de.ts` - German.

The build also merges in Qt's own upstream translations (`qtbase`,
`qtdeclarative`, ...) for each language via `MERGE_QT_TRANSLATIONS`, so
Qt-provided strings - e.g. the Yes/No buttons on the submit-time-entry
dialog - are localized too. This requires Qt's own translation catalogs to
be installed (`qt6-translations` on Arch/CachyOS, bundled with the Qt
installer on other platforms) - if they're missing, `qt_add_translations`
prints a warning per missing language/catalog and simply skips merging that
one, it doesn't fail the build.

## Updating translations after changing UI text

Whenever `qsTr()` call sites change (new/removed/reworded strings), refresh
the `.ts` files by running:

```bash
cmake --build --preset linux-debug --target update_translations
```

This runs `lupdate` against the QML/C++ sources and updates every `.ts`
file in place - new strings are added as `<translation type="unfinished">`,
removed ones are dropped (`-no-obsolete`), and unchanged ones are left
alone. Then open each `.ts` file (by hand, or with Qt Linguist) and fill in
the `<translation>` for anything still marked `unfinished`. A normal build
afterwards recompiles the updated `.ts` files into `.qm` automatically -
no separate step needed.

## Adding a new language

1. Add `i18n/Stride_<lang>.ts` to the `TS_FILES` list in
    `CMakeLists.txt` (use the two-letter
    [ISO 639-1](https://en.wikipedia.org/wiki/ISO_639-1) code, e.g. `fr`,
    `es`).
2. Run the `update_translations` target (see above) - it creates the new
    file if it doesn't exist yet and populates it with every extracted
    string.
3. Translate it, then build normally.
