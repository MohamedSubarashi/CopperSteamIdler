# Translations

All user-facing strings in `src/` use `tr()`; English is the source language.

## Shipped catalogues

| File | Language |
| --- | --- |
| `copper_en.ts` | English (identity — the sources themselves) |
| `copper_fr.ts` | Français |
| `copper_es.ts` | Español |
| `copper_pt_BR.ts` | Português (Brasil) |
| `copper_ar.ts` | العربية (right-to-left) |

These are picked up by `qt_add_translations()` in the top-level `CMakeLists.txt`
and embedded into the executable under the Qt resource prefix `:/i18n`. The
`.qm` files are build artifacts and are therefore ignored by git.

`copper_en.ts` exists so that forcing `en` in Settings yields identical text
even on a differently-located OS, and so `lupdate` keeps a complete English
reference catalogue.

## Qt's own catalogue

`windeployqt --no-translations` strips Qt's catalogues, so the release ships a
`translations/` folder next to the executable containing
`qtbase_{en,es,fr,pt_BR,ar}.qm` (copied from `C:\Qt\6.12.0\mingw_64\translations\`).
`src/main.cpp` loads them after the app catalogue so standard dialog buttons
and text-field context menus are translated too.

## Adding a language

1. Extract strings:

   ```powershell
   lupdate src -ts i18n/copper_de.ts
   ```

2. Translate each `<translation>` in `copper_de.ts` and remove
   `type="unfinished"` (Qt Linguist does this for you).

3. Add the entry to the language combo box in `src/ui/SettingsDialog.cpp`
   (`m_cboLanguage->addItem(...)`), keeping the native name untranslated.

4. Copy `qtbase_de.qm` from `<Qt>/translations/` into the shipped
   `translations/` folder.

5. Rebuild — `release_translations` compiles every `i18n/*.ts` file and
   embeds it; no extra wiring is needed.

## Refreshing existing catalogues after a code change

```powershell
lupdate src -no-obsolete -ts i18n/copper_en.ts i18n/copper_fr.ts i18n/copper_es.ts i18n/copper_pt_BR.ts i18n/copper_ar.ts
```

New strings appear as `<translation type="unfinished"></translation>`; fill
them in before releasing. `-no-obsolete` drops strings no longer in the code.

## Conventions

- No plural forms are used yet — everything is phrased so a single form works
  with `%1`.
- `&` marks the mnemonic in menu/button labels; keep exactly one per label.
- `\n` inside a source string is a literal newline in the `.ts` text.
- Language display names in the combo box stay native (`QStringLiteral`), so
  they are never translated. Only *System default* and the restart hint go
  through `tr()`.
