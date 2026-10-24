# SlowMorph firmware for the Mvave FM1

This folder holds the FM1 firmware side of SlowMorph. The design is in the [SlowMorph design doc](https://claude.ai/artifact/2zncCrwePAsYTLTqeV6dTW).

- `kit/` is the M-VAVE FM1 Open V14 development kit, unchanged, so the build can check its SDK hashes. Its licences are in `kit/LICENSE.md` (app, GPLv3 with the SDK linking exception) and `kit/THIRD_PARTY.md` (binary SDK, Apache-2.0 boot files, third-party notices).
- `.gitattributes` stops Git from changing line endings in `kit/`, because the SDK build rejects any file whose hash differs from `kit/sdk/manifest.json`.

## Cloud builds

The JieLi PI32v2 compiler runs only on Windows, so `.github/workflows/firmware.yml` builds on a GitHub Windows runner. It runs on every push that touches `firmware/`, and it can be started by hand from the Actions tab.

The workflow:

1. Downloads the JieLi Windows toolchain and unpacks it, caching it between runs.
2. Picks the toolchain copy whose files match the hashes in `kit/sdk/manifest.json` (clang 4.0.1 r3).
3. Runs `kit/sdk/Build.ps1 -FirmwareOnly`.
4. Reports the linker's free heap (`MALLOC_SIZE`) in the run summary. That is the free RAM figure the design's memory budget depends on.
5. Uploads the `.fwsc` firmware, `sdk.elf` and the map file as a build artifact.

Nothing is flashed. A build artifact goes on a device only after Roy reviews it, using the FM1 web workbench in Chrome. Back up projects, samples and DX7 banks before flashing.
