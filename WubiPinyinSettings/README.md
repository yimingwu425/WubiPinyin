# WubiPinyinSettings

`WubiPinyinSettings.exe` is an unpackaged, x64-only WinUI 3/C++/WinRT
settings application for the Windows 11 MVP. It is deliberately separate from
the TIP DLL: WinUI 3 and the Windows App SDK are never loaded in the keyboard
input process.

## Build

Requirements:

- Visual Studio 2022 with Desktop development with C++ and Windows App SDK
  tooling.
- Windows SDK 10.0.22000.0 or later.
- NuGet access for the pinned `Microsoft.WindowsAppSDK` `1.6.241114003`
  package.

From a Visual Studio x64 Developer Command Prompt:

```bat
WubiPinyinSettings\build_release.bat
```

The project restores the Windows App SDK package and writes the complete
self-contained Release payload to:

```text
output\settings\Release\x64\
```

The installer must copy that directory recursively, not only
`WubiPinyinSettings.exe`; the adjacent Windows App SDK runtime files are part
of the application. `build_release.bat` is intentionally invoked outside
xmake because the legacy TIP build does not consume NuGet WinUI dependencies.

## Data Boundary

The application never opens `wubipinyin.sqlite3`. `SettingsClient` uses the
shared fixed control frame and payload codecs over the user- and session-bound
Broker pipe:

```text
\\.\pipe\WubiPinyinBrokerControlV1\<SID>\<Windows-session>
```

It applies a total 300 ms connection timeout and 500 ms I/O timeout, validates
the reply frame and request identity, and closes the handle on any timeout or
protocol failure. The Broker remains the only SQLite writer and is responsible
for materializing user dictionaries after successful user-entry changes.

The default launch page is Input. The only supported page-selection argument is
`--page dictionary`, which opens User Dictionary directly for the tray and
Start Menu dictionary entry points.

The XAML pages map to the product contracts as follows:

- Input: `default_route`, full-pinyin fixed MVP rules, candidate page size.
- Appearance: `theme` and `show_source_labels`.
- User Dictionary: `UserEntry` CRUD through the Broker.
- Learning & Privacy: `learning_enabled`, `password_input_protection`, and
  `ResetLearning`.
- About: version, GPL lineage, and the product data location.

The UI uses Segoe UI Variable for system chrome and Microsoft YaHei UI for its
CJK text styles. It does not ship Apple fonts, icons, or other Apple-owned
assets.

## Verification

This macOS workspace cannot compile or run Windows App SDK/XAML output. The
required Windows check is:

```bat
msbuild WubiPinyinSettings\WubiPinyinSettings.vcxproj /t:Restore,Build /p:Configuration=Release /p:Platform=x64
```

Then launch the generated executable with the Broker running and verify the
five pages, dictionary CRUD, both system themes, and a Broker-unavailable
state. The installer build should assert that
`output\settings\Release\x64\WubiPinyinSettings.exe` exists before NSIS
packages the directory.
