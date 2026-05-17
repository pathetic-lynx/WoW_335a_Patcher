# Agent notes — WoW 3.3.5a Patcher

## Project layout

```
src/
  patches.h      — patch definitions, BytePatch/Patch/PatchStatus types, inline check helpers
  main.cpp       — TUI binary (no external deps beyond stdlib + filesystem)
  gui_main.cpp   — GUI binary (Dear ImGui + SDL2 + OpenGL3)
CMakeLists.txt
README.md
```

## Building for Windows

Windows is **cross-compiled from Linux only** using `mingw-w64`. There is no native Windows build and no dedicated build script — pass the mingw compilers via `-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc ...` (see README). Do not add a `build-windows.sh` or a `cmake/windows-x86_64-toolchain.cmake`.

Two build targets: `WoW_335a_Patcher` (TUI) and `WoW_335a_Patcher_GUI` (GUI).

## Dependencies

Both ImGui and SDL2 are vendored via CMake `FetchContent` — no system installs required. Do not add `find_package(SDL2)` or `find_package(imgui)`. After `FetchContent_MakeAvailable(SDL2)`, link with `SDL2::SDL2` (modern CMake target, carries includes automatically — no `${SDL2_INCLUDE_DIRS}` needed).

## Patch semantics

`patches.h` defines:
- `BytePatch` — one contiguous byte range: `offset`, `original` bytes (unpatched state), `replacement` bytes (patched state)
- `Patch` — named group of one or more `BytePatch` changes + `enabled` flag
- `PatchStatus` — `Applied` / `Unpatched` / `Mismatch`
- `check_patch()` / `check_byte_patch()` — read-only file checks, return `PatchStatus`
- `default_patches()` — returns the full patch list as a fresh vector

Target file is `WoW.exe` build 12340, expected size `kExpectedFileSize = 7704216` bytes.

## Apply/revert logic — critical rules

Apply is **bidirectional**:
- `enabled` patch → write `replacement` bytes
- `disabled` patch where status == `Applied` → write `original` bytes (revert)
- `disabled` patch where status == `Unpatched` → skip
- `disabled` patch where status == `Mismatch` → **skip, never write original bytes** — bytes are unknown, touching them would corrupt the file

After every apply, immediately re-run verify so checkbox state and status badges reflect the new file state.

## Verify / auto-scan

`action_verify()` (GUI) / `check_patch()` (TUI) reads bytes from disk and sets `g_patches[i].enabled` to match what's in the file (`Applied` → `true`, anything else → `false`). Auto-scan is triggered:
- on drag & drop (`SDL_DROPFILE`)
- on Browse button selection
- when the path InputText loses focus after edit (`IsItemDeactivatedAfterEdit`)
- when a path is passed as `argv[1]`

Auto-scan only runs when the file size matches `kExpectedFileSize`.

## GUI specifics

- Root window fills the OS window exactly (`SetNextWindowPos/Size`, `NoDecoration | NoMove`)
- Drag-over overlay drawn via `GetForegroundDrawList()` at 2× font scale — triggered by `SDL_DROPBEGIN` / cleared on `SDL_DROPCOMPLETE` or `SDL_DROPFILE`
- Native file dialog: Linux tries `zenity` → `kdialog` → `yad` via `popen`; exit code 127 means the tool wasn't found (distinguished from user cancel); empty result → don't call `set_path`. Windows uses `GetOpenFileName` (`comdlg32`). macOS uses `osascript`.
- `io.IniFilename = nullptr` — no `imgui.ini` written to disk
- No log panel — removed intentionally. Don't add one back.
- No backup/restore — removed intentionally. Don't add it back.

## TUI specifics

- ANSI colours via `#ifdef _WIN32` / `SetConsoleMode ENABLE_VIRTUAL_TERMINAL_PROCESSING`
- Clear screen via `\033[2J\033[H`
- Input via `std::getline` only (never mix with `>>` to avoid hangs)
- Apply pre-checks status of disabled patches via `check_patch()` before opening the write `fstream`, since both need the file and you can't interleave read/write safely
