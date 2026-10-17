# Changelog

## Unreleased

Native Linux port: the game now builds and runs as a native x86_64 Linux
executable (`scripts/setup-native-linux.sh`, **Play BLACK Native.sh**) with no
Windows binaries or Wine. `patches/xboxrecomp-linux.patch` adds the runtime
pieces: an SDL2 framebuffer presenter beside the D3D11 one (Kelvin CPU
renderer; the D3D11 GPU backend reports unavailable), a sigaction-based
vectored exception handler with a real `CONTEXT` so the APU/NV2A MMIO
instruction decoders run unchanged, an SDL2 audio backend behind the `xa2_*`
contract, POSIX `pc_input` fed by SDL scancode translation, and Linux thread
pinning. `src/main.c`/`src/recomp_manual.c` and the game CMake build are
cross-platform. Validated headlessly by `tests/linux_native/` (VEH fault
repair, keyboard-to-pad merging, audio submit contract, presentation);
full-mission native play is unverified. The .NET launchers, native menu
overlay and Media Foundation FMV player remain Windows-side.

## 0.1.0-alpha.2 — 2026-10-14

**Build BLACK.sh** now generates and compiles the game entirely on x86_64 Linux
without a Windows installation. The build uses native Linux Python, LLVM,
Rust and Mono, with a pinned Microsoft SDK/CRT downloaded by xwin. It outputs the
Windows x64 game executable for Wine/Proton and builds both host launchers.

Added the CMake cross toolchain, compiler compatibility fixes, local language-bank
generation, build provenance, and an asset-free Linux runtime/Rust ABI CI check.
Retail files and generated game code remain local. Windows building stays supported.

## 0.1.0-alpha.1 — 2026-10-14

First public starter release. Includes compiled one-click **BLACK PC Launcher.exe**
and separate **BLACK PC Settings.exe**, Windows build shortcut, Linux Wine/Proton
entry points, host source, licenses, and archive/file SHA-256 checksums.

Portable folders may keep the locally generated game executable in
`bin/black_xbox_recomp.exe`; existing `build-vs2022/Release/` projects also work.
Play uses saved preferences and settings opens separately. Both share saves,
language selection and diagnostics. The latest audio completion freeze,
weapon pickups, save/profile, ambient occlusion and localized bank fixes are included.

The public starter requires your own supported Xbox files and a one-time Windows
build. Game executables, retail data, generated game code, language banks and saves
are excluded. Linux runs the resulting Windows build through Wine/Proton.
This remains experimental; full later-mission playthroughs are unverified.
