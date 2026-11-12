@AGENTS.md

# Fork notes: PJB (OrcaSlicer + Pawel Jarczak's Bambu network-plugin bridge)

This repo is janno's fork build. **PJB = original OrcaSlicer + the bridge** that lets Bambu cloud printers work. Releases are named `vX.Y.Z-PJB.N` (older `bmcu.*` tags are prereleases kept as history) and we plan to rebuild for every official OrcaSlicer release: merge upstream, keep only the cloud-compat pieces.

## Remotes and release process
- `houdini` = jannoke/orcaslicer-houdini (push here). **Never push to `origin`** (FULU-Foundation). `upstream` = SoftFever/OrcaSlicer, reference only.
- Flow: branch from `houdini/main` -> commit -> push to `houdini` -> PR -> merge -> tag -> CI (`My Build All`) publishes a full release. Ask the user before pushing the tag or triggering any CI run.
- **Bump `FORK_RELEASE_LABEL` in `version.inc` before every release tag.** It feeds the splash/About text and `my_build_all.yml` rejects a tag that is not `v<SoftFever_VERSION>-<label>`.
- A tag push needs `.github/release-notes/<tag>.md`. Notes are for end users only (what's new, known issues, downloads); no CI/infra items; anything not verified on hardware goes under Known issues; show the draft to the user before publishing.
- The Windows CI job runs on the self-hosted runner `gh-runnerwin-01`; Linux jobs on `gh-runner-01..05`. If the Windows runner service is stopped, start it (`Start-Service actions.runner.*`) before tagging. `gh api` works in the sandbox as `GH_TOKEN=dummy gh ...` (the proxy injects the real token); plain `git push` uses proxy-injected HTTPS credentials. SSH remotes do not work from the sandbox.
- Check CI results by job: the build scripts used to package stale binaries after compile errors, so verify `error C`/`error LNK` counts and DLL timestamps before trusting a package.

## How the Windows bridge works
Orca on Windows loads `pjarczak_bambu_networking_bridge.dll` (forwarder) from `%APPDATA%\OrcaSlicer\plugins`. It starts a Linux host inside a WSL2 distro (`PJARCZAK-BAMBU`, imported from `windows-wsl2-rootfs.tar`) that runs the vendor's Linux `libbambu_networking.so`. Files of interest:
- `src/slic3r/Utils/BBLNetworkPlugin.cpp` (`initialize`: bridge setup, runtime sync, env vars)
- `src/slic3r/Utils/PJarczakLinuxBridge/` (`PJarczakLinuxBridgeConfig.*`, forwarder exports, `PJarczakLinuxSoBridgeLauncher_win.cpp` = WSL launch/repair, `PJarczakRuntimeSync.*` = refresh from install dir)
- `tools/pjarczak_bambu_runtime/wsl/` (`install_runtime.ps1`, `verify_runtime.ps1`, `pjarczak_wsl_run_host.sh`), `tools/pjarczak_bambu_linux_host/` (Linux host)
- `.github/workflows/build_windows_bridge.yml` assembles the runtime bundle into `build/OrcaSlicer`; `src/slic3r/Utils/PJarczakLinuxBridge/CMakeLists.txt` installs those files into the NSIS installer (the installer once lacked them, which left users on a stale bridge).

Things that bit us, don't repeat them:
- **The OS-header spoof is required**: the forwarder rewrites `X-BBL-OS-Type` to `linux` (`override_client_os_header`, opt-out env `PJARCZAK_BRIDGE_KEEP_OS_HEADERS=1`). Without it Bambu's cloud answers 403 (`[1:403]` camera, print jobs). Don't remove it.
- **The bridge only runs the newest plug-in series** (`NetworkAbi::Current`, 02.08.04 as of 2.5.0). Orca picks call layouts per plug-in version (`PrintParams_0203`, bind without `dev_model`, ...) but the forwarder/host pass the Current layout only; `is_bridge_supported_network_version()` gates startup, the version picker and the host. On every upstream merge diff `BBLNetworkPlugin.hpp` typedefs against the forwarder exports and the host `net<...>` casts, `PrintParams` against `BridgeCoreJson.hpp`, and `BBLModelTask` (`libslic3r/ProjectTask.hpp`) against the lightweight copy in `PJarczakLinuxBridgeCompat.hpp` that the forwarder and host compile with (the plug-in fills it in place, so a shorter copy gets overwritten).
- Shell scripts must be LF (`.gitattributes` pins `*.sh`); a CRLF `pjarczak_wsl_run_host.sh` makes the bridge return an empty version and fail to load.
- On startup `PJarczakRuntimeSync` copies the runtime shipped next to `orca-slicer.exe` into `plugins`. A release label marker (`plugins\pjarczak_runtime_release.txt`, label passed as `PJARCZAK_RELEASE_LABEL`) triggers a silent destroy + re-import of the WSL distro on every new release (takes minutes; a PowerShell window shows progress).
- Camera live view (2.5.0+, `wxMediaCtrl3`) runs through the bridge: `get_source_module()` returns the forwarder, so `Bambu_Open`/`Bambu_ReadSample` execute in the Linux `libBambuSource.so` inside WSL. Agora streams (X2D and other printers with the agora flag) make it `dlopen` `libagora_rtc_sdk.so`, which needs X11/libdrm libraries that `build_windows_wsl_rootfs.sh` installs into the rootfs (`RUNTIME_PACKAGES`); on a new plug-in release `ldd` its `.so` files inside the rootfs. The Windows `BambuSource.dll` + `live555.dll` (`download_plugin()` also fetches the Windows zip, `install_plugin()` extracts those two) are now only used by the virtual-camera tool; 2.4.2 played the live view through them as a DirectShow filter.
- The bridge host's stderr (`[PJBRIDGE]` logs) is discarded on Windows and the vendor plugin's own log is encrypted; there is no HTTP-level logging. Debugging a 403 means comparing behaviour, not reading headers.
- A P1S can refuse camera connections ([2:1]/[2:-1]) when several Orca/Studio clients are open; that is the printer's limit, not a bug here.

## Testing and building Windows changes
- The WSL distro is re-imported only when the release label changes, so a rebuilt runtime (new rootfs) under the same `FORK_RELEASE_LABEL` keeps the old distro: delete `%APPDATA%\OrcaSlicer\plugins\pjarczak_runtime_release.txt` before launching a test build.
- You cannot build Windows in the sandbox. Use the Windows runner workspace (`C:\actions-runner\_work\orcaslicer-houdini\orcaslicer-houdini`, VS 2022 BuildTools, NSIS, cmake from the runner's `_work\_tool`) for incremental builds (a full rebuild after a `version.inc` change is ~25 min), then run the CI "assemble runtime bundle" step and `cpack -G NSIS`. Processes started over SSH on Windows die with the session; start long jobs with `Invoke-CimMethod Win32_Process Create`.
- The runner is a Proxmox VM without a GPU: the 3D view is broken there, so print jobs, GUI behaviour and upgrade scenarios must be verified on the user's real PC. **Never touch the user's personal PC**; ask them to run read-only diagnostic commands and paste the output (blank out tokens).
- Never put Bambu tokens or secrets in the repo, CI logs or artifacts; the repo is public and Actions masking only hides exact strings.

## Open ideas
- Code signing for the Windows installer (SmartScreen warns about an unsigned installer): SignPath Foundation, Azure Trusted Signing or a commercial certificate; winget manifest as a partial fix.
- Optional bridge debug logging (WSL host stderr to a file, redacted headers/results), the "refreshed ..." sync log lines did not show up in Orca's log, macOS bridge has no self-hosted runner.
