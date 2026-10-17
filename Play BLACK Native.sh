#!/usr/bin/env bash
# Play the native Linux build (bin/black_xbox_recomp). The Windows launcher
# does not run natively; this exports the same core session variables it
# sets, and video/input settings come from local/video.ini and the
# RECOMP_* variables documented in docs/linux-native.md.
set -euo pipefail
release_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
executable="$release_root/bin/black_xbox_recomp"
[[ -x "$executable" ]] || { echo 'Native build missing. Run: bash scripts/setup-native-linux.sh' >&2; exit 1; }
[[ $(uname -m) == x86_64 ]] || { echo 'This build requires x86_64 Linux.' >&2; exit 1; }
cd -- "$release_root"
export RECOMP_AC97_READY=1
export RECOMP_APU_DSP_ACK=0x83218810
export RECOMP_NV2A_KELVIN=1
export RECOMP_VBLANK=1
export RECOMP_USB=1
export RECOMP_FB_WINDOW=1
export RECOMP_VIDEO_INI="$release_root/local/video.ini"
# The Kelvin GPU backend (RECOMP_KELVIN_GPU) is Direct3D 11-only; the
# native build always uses the Kelvin CPU renderer.
unset RECOMP_KELVIN_GPU || true
exec "$executable" "$@"
