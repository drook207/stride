# Translations

Stride is available in English and German. UI strings are marked with
`qsTr()` and collected in Qt Linguist `.ts` files under
`src/Stride/i18n`, which the build compiles into the binary. The
client library has its own catalog in `src/RedmineClient/i18n`. The details
are in the
[i18n README](https://github.com/drook207/stride/blob/main/src/Stride/i18n/README.md).

## After changing UI text

```bash
cmake --build --preset linux-debug --target update_translations
```

This updates every `.ts` file: new strings are added as unfinished. Translate
them by hand or in Qt Linguist, then build as usual.

## Adding a language

1. Add `i18n/Stride_<lang>.ts` to `TS_FILES` in
    `src/Stride/CMakeLists.txt`, with the two-letter
    [ISO 639-1](https://en.wikipedia.org/wiki/ISO_639-1) code.
2. Run the `update_translations` target to create the file.
3. Translate it and build. The language then shows up under
    *Settings* -> *Appearance* automatically.
