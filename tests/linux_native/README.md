# Native Linux runtime test

Builds the patched runtime and exercises the native Linux port headless
(SDL dummy video/audio drivers): the sigaction VEH (fault, repair,
resume), `pc_input` keyboard merging into the pad report, the SDL audio
backend behind `xa2_*`, and the SDL framebuffer presenter (publish,
present, `RECOMP_FB_DUMP`, close). No retail files are needed.

```bash
bash scripts/setup-native-linux.sh --prepare-only   # fetch pinned deps + patches
cmake -S tests/linux_native -B build-linux-native-test -G Ninja
cmake --build build-linux-native-test
ctest --test-dir build-linux-native-test --output-on-failure
```
