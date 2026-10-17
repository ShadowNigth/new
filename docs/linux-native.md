# Native Linux build

The game also builds and runs as a **native Linux x86_64 executable** — no
Windows binary, no Wine/Proton, no Microsoft SDK. It needs an AVX2 CPU, the
same requirement as the Windows build.

```bash
# Ubuntu 24.04 packages (Arch/Fedora equivalents in parentheses)
sudo apt-get install -y build-essential cmake ninja-build pkg-config git \
  libsdl2-dev libepoxy-dev libssl-dev python3-venv curl
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh   # DSP core (pacman: rustup / dnf: rustup)

cd '/path/to/BLACK'
bash scripts/setup-native-linux.sh
./'Play BLACK Native.sh'
```

(Arch: `base-devel cmake ninja sdl2 libepoxy openssl python`; Fedora:
`gcc cmake ninja pkgconf SDL2-devel libepoxy-devel openssl-devel python3`.)

Supply your supported original Xbox files in `game/` first (same SHA-256
check as every other build). The script clones the pinned XboxRecomp and
DSP56300 revisions, applies `patches/xboxrecomp.patch`,
`patches/xboxrecomp-clang.patch` and `patches/xboxrecomp-linux.patch`,
generates the game code from your XBE with Python, and builds
`bin/black_xbox_recomp` with the system compiler.

## How it works

The Windows renderer's rasterizer is Kelvin, a CPU implementation of the
NV2A, so the heavy machinery was already portable; what was missing was
everything around it. The Linux patch to the runtime supplies it:

- **Presentation** — `fb_present.c` gains an SDL2 presenter next to the
  D3D11 one: the same one-deep frame mailbox and NV097 pixel conversion,
  a streaming texture scaled with the shared `fb_present_rect()` policy
  (fit/stretch/integer, nearest/linear, vsync, FPS limit), F11 fullscreen,
  and the movie publication path. The D3D11 Kelvin GPU backend
  (`RECOMP_KELVIN_GPU`) stays Windows-only and reports itself unavailable,
  so the CPU renderer — the semantic reference for the GPU path — renders.
- **Crash/MMIO traps** — `win32_compat.c` implements
  `AddVectoredExceptionHandler` over `sigaction` (SIGSEGV/SIGBUS/SIGFPE/
  SIGILL/SIGTRAP), building Win32-layout `CONTEXT`/`EXCEPTION_RECORD`
  values from `ucontext_t` and writing them back on
  `EXCEPTION_CONTINUE_EXECUTION`. The APU and NV2A instruction decoders
  therefore compile and run unchanged, and `src/main.c` keeps its VEH
  handlers; symbol names in crash reports come from `dladdr()`.
- **Audio** — the `xa2_*` backend is SDL2: 48 kHz stereo, three
  1024-sample packets, start after two are queued, backpressure instead of
  drops, the same stats contract. The waveOut fallback stays inert.
- **Input** — `pc_input.c` compiles on POSIX (its key ids were always
  PS/2 set-1 scancodes); the SDL presenter translates SDL scancodes to
  set-1 and feeds the same `pc_input_*_event()` functions as Windows raw
  input, including relative mouse capture. Gamepads go through the
  existing SDL GameController backend.
- **Video settings/menus** — `pc_video` already parsed
  `local/video.ini` and the `RECOMP_*` variables on POSIX. The native
  Video Settings page overlay is not drawn by the SDL presenter yet, so
  the game's own menus are used; settings are edited in
  `local/video.ini` or through the variables.
- **Thread placement** — explicit `RECOMP_PIN_<ROLE>` masks apply through
  `sched_setaffinity`; automatic hybrid-core placement stays Windows-only.
- **FMV** — movies play through the game's own XMV decoder and the
  presenter's movie path, as on Windows. The Media Foundation container
  player remains Windows-only and reports itself unavailable.
- `RECOMP_WATCH` (guard-page memory watch) remains Windows-only.

## Differences from the Wine build

- The .NET launchers do not run natively. **Play BLACK Native.sh** exports
  the same core session variables the launcher sets; language banks are
  not installed by it, so the game starts in the language of the bank in
  `game/language/strings/MainUS.bin` (English on a fresh extract).
- No `RECOMP_KELVIN_GPU`, ambient-occlusion GPU path, or menu overlay:
  expect CPU-renderer performance characteristics (it is the same
  renderer the Windows build falls back to without the GPU backend).

## Validation

`tests/linux_native/` builds the runtime and runs headless on SDL's dummy
drivers: it checks VEH fault repair-and-resume, keyboard-to-pad merging,
the SDL audio submit contract, and frame publication/presentation
(including the `RECOMP_FB_DUMP` BMP). The real game binary cannot be
exercised without retail files, so — as with the Wine build — full-mission
stability on native Linux is unverified; the first-mission route is the
one to smoke-test first.
