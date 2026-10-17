#!/usr/bin/env bash
# Build the native Linux game: no Windows binaries, no Wine, no xwin/MSVC.
# The renderer is the Kelvin CPU renderer with the SDL2 presenter; see
# docs/linux-native.md for the design and the differences from the Wine build.
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
mode=${1:-build}
case "$mode" in
    build|--prepare-only|--skip-generate) ;;
    *) printf 'Usage: %s [--prepare-only|--skip-generate]\n' "$0" >&2; exit 1 ;;
esac
[[ $# -le 1 ]] || { echo 'Unexpected arguments.' >&2; exit 1; }
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || { echo 'Use an x86_64 Linux host.' >&2; exit 1; }
grep -q avx2 /proc/cpuinfo || { echo 'This build requires an AVX2-capable CPU (same requirement as the Windows build).' >&2; exit 1; }

# Native dependencies (Debian/Ubuntu names; see docs/linux-native.md for
# Arch/Fedora equivalents): a C toolchain, CMake, SDL2, libepoxy and OpenSSL
# development files, Rust for the DSP56300 core, Python for code generation.
for tool in python3 git cmake cargo ${CC:-cc} pkg-config ninja; do
    command -v "$tool" >/dev/null || { echo "Missing $tool; see docs/linux-native.md for native build dependencies." >&2; exit 1; }
done
for module in sdl2 epoxy openssl; do
    pkg-config --exists "$module" || { echo "Missing development files for $module (pkg-config); see docs/linux-native.md." >&2; exit 1; }
done

if [[ "$mode" != --prepare-only ]]; then
    [[ -f "$project_root/game/default.xbe" ]] || { echo 'Supply your original Xbox files under game/ first.' >&2; exit 1; }
    printf '%s  %s\n' df2739c372d254a90aeeccf5971d12097f22d89fbb90db021fda96dd4deb68f6 "$project_root/game/default.xbe" | sha256sum --check
fi

toolkit="$project_root/third_party/xboxrecomp"
dsp="$project_root/.work/err009-dsp56300"
if [[ ! -e "$toolkit" ]]; then
    mkdir -p "$project_root/third_party"
    git clone https://github.com/sp00nznet/xboxrecomp.git "$toolkit"
    git -C "$toolkit" checkout --detach 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b
fi
[[ $(git -C "$toolkit" rev-parse HEAD) == 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b ]] || { echo 'Runtime checkout differs from the pinned revision; preserve it separately.' >&2; exit 1; }
for patch in xboxrecomp.patch xboxrecomp-clang.patch xboxrecomp-linux.patch; do
    if git -C "$toolkit" apply --check "$project_root/patches/$patch" 2>/dev/null; then
        git -C "$toolkit" apply "$project_root/patches/$patch"
    else
        git -C "$toolkit" apply --reverse --check "$project_root/patches/$patch" || { echo "Conflicting $patch; preserve dependency edits before retrying." >&2; exit 1; }
    fi
done
if [[ ! -e "$dsp" ]]; then
    git clone --depth 1 --branch v1.0.0 https://github.com/mborgerson/dsp56300.git "$dsp"
fi
[[ $(git -C "$dsp" rev-parse HEAD) == aab3649fc452daf03da8b5e36fdc0c308d874101 ]] || { echo 'DSP source differs from the pinned revision; preserve it separately.' >&2; exit 1; }

if [[ ! -x "$project_root/.venv/bin/python" ]]; then python3 -m venv "$project_root/.venv"; fi
python="$project_root/.venv/bin/python"
"$python" -m pip install capstone==5.0.7

if [[ "$mode" == --prepare-only ]]; then
    echo 'Dependencies prepared; game code not generated.'
    exit 0
fi
if [[ "$mode" != --skip-generate ]]; then
    (cd "$project_root" && "$python" scripts/generate.py)
fi

build="$project_root/build-linux-native"
cmake -S "$project_root" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DDSP56300_SOURCE_DIR="$dsp"
jobs=${BLACK_BUILD_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'BLACK_BUILD_JOBS must be a positive integer.' >&2; exit 1; }
cmake --build "$build" --target black_xbox_recomp --parallel "$jobs"
mkdir -p "$project_root/bin"
cp "$build/black_xbox_recomp" "$project_root/bin/black_xbox_recomp"
printf 'Ready: %s/Play BLACK Native.sh\n' "$project_root"
