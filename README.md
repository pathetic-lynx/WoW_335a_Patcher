# WoW 3.3.5a Patcher

Binary patcher for the World of Warcraft 3.3.5a client. Continuation of [robinsch/WoWFix335](https://github.com/robinsch/WoWFix335).

Targets `WoW.exe` build 12340 (file size 7 704 216 bytes).

---

## Patches

| Patch | Description |
|---|---|
| RCE exploit fix | Blocks a remote code execution vulnerability in the client |
| Windowed mode to fullscreen | Allows switching to true fullscreen from windowed mode |
| Melee swing on right-click | NOPs the broken melee-swing call triggered by right-click |
| NPC attack animation while turning | Fixes NPC attack animation being skipped when the NPC turns |
| Ghost attack on NPC evade | Prevents phantom attacks when an NPC resets and evades combat |
| Pre-cast animation fix | Restores missing spell pre-cast animations |
| Mail timeout fix | Reduces the excessive mail delivery timeout |
| Area trigger timer precision | Halves area trigger timer granularity for more accurate events |
| Blue Moon fix | Restores the Blue Moon event flag |
| Mouse flicker fix | Fixes camera snapping and mouse flickering at high polling rates |
| 4GB address space (LAA flag) | Sets the LARGE_ADDRESS_AWARE PE flag so the client can use >2 GB RAM |
| AwesomeWoTLK support | Hooks required for the AwesomeWoTLK addon framework |
| Signature bypass | NOPs signature-check calls that break with modified clients |
| Disable WDB cache | Disables the WDB item/creature cache to force server-side data |
| Allow interface edits | Removes restrictions on loading modified interface files |

Checking/unchecking a patch and clicking **Apply** will apply or revert it in-place. The patcher verifies the expected byte patterns before writing and reports `MISMATCH` for bytes it does not recognise, leaving those untouched.

---

## Building

CMake 3.20+ and a C++23 compiler are required for both targets.

The **TUI** (`WoW_335a_Patcher`) has no external dependencies.  
The **GUI** (`WoW_335a_Patcher_GUI`) requires SDL2 and OpenGL; Dear ImGui is fetched automatically at configure time.

### Linux

```sh
# Arch
sudo pacman -S sdl2

# Ubuntu / Debian
sudo apt install libsdl2-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binaries are written to `build/`.  
The file-browser button uses `zenity`, `kdialog`, or `yad` if available.

### Windows

Install [SDL2](https://github.com/libsdl-org/SDL/releases) and point CMake at it:

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSDL2_DIR=C:\path\to\SDL2\cmake
cmake --build build --config Release
```

Visual Studio 2022 (v17+) and MinGW-w64 13+ both work.  
The GUI uses the native `GetOpenFileName` dialog — no extra tools needed.

### macOS

```sh
brew install sdl2

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The file-browser button uses `osascript` (built-in).

---

## Usage

```sh
# GUI (recommended)
./build/WoW_335a_Patcher_GUI /path/to/WoW.exe

# TUI
./build/WoW_335a_Patcher /path/to/WoW.exe
```

The path can also be set at runtime via the file field, the **Browse** button, or by dragging the executable onto the window (GUI only).
