# NextUI Hooks Test

> [!WARNING]
> This is **not** currently implemented in NextUI.
> This repository illustrates and tests the proposal in [LoveRetro/NextUI pull request #690](https://github.com/LoveRetro/NextUI/pull/690).
> The contents of this repository and this documentation are AI-generated and remain pending human verification.

A test pak and hook infrastructure for validating the NextUI hook system. The pakz includes both the **Hooks Test** tool pak and the modified system scripts from PR #690 (`run_hooks.sh`, `launch.sh`, `suspend`).

## What the Pakz Includes

When extracted to the SD card, the pakz installs:

| Path on SD card | Purpose |
|---|---|
| `Tools/{platform}/Hooks Test.pak/` | The test tool (appears in Tools menu) |
| `.system/{platform}/bin/run_hooks.sh` | Hook runner script from PR #690 |
| `.system/{platform}/bin/suspend` | Modified suspend script with pre-sleep/post-resume hooks |
| `.system/{platform}/paks/MinUI.pak/launch.sh` | Modified launcher with boot, pre-launch, and post-launch hooks |

Both tg5040 and tg5050 platform variants are included.

## What the Test Pak Tests

The Hooks Test pak installs demo scripts into the hook directories and logs their execution. It covers **5 hook types** with **4 script variants** each (20 scripts total).

### Hook Types

| Hook Type | Directory | When It Runs |
|---|---|---|
| Boot | `boot.d/` | Once at system startup, after `auto.sh` |
| Pre-Launch | `pre-launch.d/` | Before every ROM or pak launch |
| Post-Launch | `post-launch.d/` | After every ROM or pak exits |
| Pre-Sleep | `pre-sleep.d/` | Before device suspends (always synchronous) |
| Post-Resume | `post-resume.d/` | After device wakes from sleep |

### Script Variants

Each hook type can have up to 4 scripts installed simultaneously:

| Variant | Filename | Behavior |
|---|---|---|
| Async | `hooks-test.sh` | Runs in background (default) |
| Sync | `hooks-test.sync.sh` | Runs synchronously, blocks until complete |
| Async (Error) | `hooks-test-error.sh` | Runs in background, logs error, exits 1 |
| Sync (Error) | `hooks-test-error.sync.sh` | Runs synchronously, logs error, exits 1 |

### What Each Script Logs

Normal scripts write a timestamped entry to `$LOGS_PATH/hooks-test.log`:

```
[2026-03-26 14:32:10] Boot (async)
[2026-03-26 14:33:45] Pre-Launch (sync) | TYPE=rom EMU=/path/to/emu ROM=/path/to/rom LAST=Genesis/Sonic
[2026-03-26 14:35:22] Post-Launch (async) | TYPE=rom EMU=/path/to/emu ROM=/path/to/rom LAST=Genesis/Sonic
[2026-03-26 14:40:00] Pre-Sleep (sync) | PHASE=pre CATEGORY=pre-sleep.d
[2026-03-26 14:45:00] Post-Resume (async) | PHASE=post CATEGORY=post-resume.d
```

Error scripts additionally write to stderr and log the error before exiting with code 1:

```
[2026-03-26 14:33:46] ERROR Pre-Launch (async, error) exit 1
```

This verifies that a failing hook does not break the launcher or prevent other hooks from running.

### Test Scenarios

The configure screen uses a 4-way toggle (Off / Async / Sync / Both) for each hook type, with separate rows for normal and error variants. This allows testing:

- **Async-only execution** — script runs in background, doesn't block
- **Sync-only execution** — script blocks until complete (`.sync.sh` suffix)
- **Mixed async + sync** — both scripts fire for the same event, in correct order
- **Error handling** — failing scripts (exit 1) don't affect the launcher or other hooks
- **Mixed normal + error** — normal and error scripts coexist in the same directory
- **Pre-sleep forced sync** — pre-sleep hooks always run synchronously regardless of naming

## Usage

1. Extract the `.pakz` to your SD card root
2. Launch **Hooks Test** from the Tools menu
3. Select **Configure Hooks** to toggle individual hook types and variants
4. Trigger hooks: reboot (boot), launch a ROM (pre/post-launch), sleep the device (pre-sleep/post-resume)
5. Select **View Log** to see timestamped hook events
6. Use **Install All** to enable all 20 scripts at once for maximum coverage
7. Use **Clear Log** to reset between test runs

## Building

Requires SDL2, SDL2_ttf, and SDL2_image.

```sh
# macOS (native development)
make mac
make run-mac

# Update Apostrophe submodule to the latest pinned origin/main commit
make update-apostrophe

# Cross-compile for device
make tg5040    # or tg5050

# Package builds (includes hook system scripts from PR #690)
make package

# Build, package, and deploy via ADB
make deploy
```

---

## Hook System Reference

### Directory Layout

Hooks are platform-specific. The launcher reads them from:

```
$USERDATA_PATH/.hooks/
    boot.d/             # scripts run at boot (after auto.sh)
    pre-launch.d/       # scripts run before ROM/pak launch
    post-launch.d/      # scripts run after ROM/pak exits
    pre-sleep.d/        # scripts run before device sleeps
    post-resume.d/      # scripts run after device wakes
```

On device, `USERDATA_PATH` resolves to `/mnt/SDCARD/.userdata/$PLATFORM`, so the actual paths are:

```
/mnt/SDCARD/.userdata/tg5040/.hooks/boot.d/
/mnt/SDCARD/.userdata/tg5040/.hooks/pre-launch.d/
/mnt/SDCARD/.userdata/tg5040/.hooks/post-launch.d/
/mnt/SDCARD/.userdata/tg5040/.hooks/pre-sleep.d/
/mnt/SDCARD/.userdata/tg5040/.hooks/post-resume.d/
```

If a hook directory does not exist, nothing happens and there is no overhead.

### Background Execution

By default, hook scripts run in the **background** (concurrently). This prevents slow hooks from delaying the launcher or blocking the UI.

If a hook **must** run synchronously (for example, it needs to finish before the next step proceeds), name it with a `.sync.sh` suffix:

```
post-launch.d/my-hook.sh        # runs in background (default)
post-launch.d/my-hook.sync.sh   # runs synchronously, blocks until complete
```

**Exception:** Pre-sleep hooks always run synchronously regardless of naming, because they must complete before the device enters sleep and services are stopped.

### Environment Variables

Hook scripts inherit all standard NextUI environment variables plus:

| Variable | Description |
|---|---|
| `HOOK_PHASE` | `pre`, `post`, or `boot` |
| `HOOK_CATEGORY` | Full directory name (e.g. `pre-launch.d`, `boot.d`) |

Launch hooks additionally receive:

| Variable | Description |
|---|---|
| `HOOK_TYPE` | `rom` or `pak` |
| `HOOK_CMD` | The raw launch command |
| `HOOK_EMU_PATH` | Path to the emulator or pak `launch.sh` |
| `HOOK_ROM_PATH` | Path to the ROM file (empty for pak launches) |
| `HOOK_LAST` | Contents of `/tmp/last.txt` (the last selected menu entry) |

Standard NextUI variables available to all hooks:

| Variable | Example Value |
|---|---|
| `SDCARD_PATH` | `/mnt/SDCARD` |
| `PLATFORM` | `tg5040` |
| `USERDATA_PATH` | `/mnt/SDCARD/.userdata/tg5040` |
| `SHARED_USERDATA_PATH` | `/mnt/SDCARD/.userdata/shared` |
| `SYSTEM_PATH` | `/mnt/SDCARD/.system/tg5040` |
| `LOGS_PATH` | `/mnt/SDCARD/.userdata/tg5040/logs` |
| `BIOS_PATH` | `/mnt/SDCARD/Bios` |
| `ROMS_PATH` | `/mnt/SDCARD/Roms` |
| `SAVES_PATH` | `/mnt/SDCARD/Saves` |
| `CORES_PATH` | `/mnt/SDCARD/.system/tg5040/cores` |

### Writing a Hook Script

A hook script is any executable `.sh` file placed in one of the hook directories. Scripts run in alphabetical order.

#### Rules

- Each script runs in a subshell. A crash or non-zero exit will not affect the launcher or other hooks.
- Script output (stdout/stderr) is suppressed. If you need logging, write to your own log file.
- Keep hooks fast. Even backgrounded hooks consume resources.
- Each pak should use a descriptive filename to avoid collisions.

### Installing Hooks from a Pak

A pak can install hooks during its own setup. Since hooks are platform-specific, install to each supported platform:

```sh
for PLAT in tg5040 tg5050; do
    HOOKS_DIR="$SDCARD_PATH/.userdata/$PLAT/.hooks"
    mkdir -p "$HOOKS_DIR/post-launch.d"
    cp "$PAK_DIR/hooks/my-hook.sh" "$HOOKS_DIR/post-launch.d/"
    chmod +x "$HOOKS_DIR/post-launch.d/my-hook.sh"
done
```

Uninstall:

```sh
for PLAT in tg5040 tg5050; do
    rm -f "$SDCARD_PATH/.userdata/$PLAT/.hooks/post-launch.d/my-hook.sh"
done
```

### Comparison

| | auto.sh | boot.d | Launch hooks | Sleep hooks |
|---|---|---|---|---|
| When | Once at boot | Once at boot | Every launch | Every sleep/wake |
| Where | `$USERDATA_PATH/auto.sh` | `.hooks/boot.d/*.sh` | `.hooks/{pre,post}-launch.d/*.sh` | `.hooks/pre-sleep.d/*.sh`, `.hooks/post-resume.d/*.sh` |
| Composability | Single shared file | One file per pak | One file per pak | One file per pak |
| Execution | Synchronous | Background (default) | Background (default) | Pre-sleep: always sync. Post-resume: background (default) |
