# Translations

All user-facing strings in `src/` use `tr()`. English is the source language,
so no catalogue is needed by default.

To add a language:

1. Extract strings (from the build environment with Qt's `lupdate`):

   ```powershell
   lupdate ..\src -ts copper_de.ts
   ```

2. Translate the `.ts` file.
3. Rebuild — every `i18n/*.ts` file is compiled to a `.qm` automatically and
   loaded at startup from the saved `language` setting (or the system locale).

Add new language codes to the combo box in `src/ui/SettingsDialog.cpp` if you
want them selectable in the UI.
