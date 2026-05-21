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

## Building for Windows (cross-compile from Linux)

Requires `mingw-w64` (`pacman -S mingw-w64-gcc` / `apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64`).

```sh
cmake -S . -B build-windows \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --parallel
```

Binaries (`*.exe`) are written to `build-windows/`.
