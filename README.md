# NextUI Hooks

> [!WARNING]
> This is **not** currently implemented in NextUI.
> This repository simply illustrates the proposal in [LoveRetro/NextUI pull request #690](https://github.com/LoveRetro/NextUI/pull/690).
> The contents of this repository and this documentation are AI-generated and remain pending human verification.

NextUI supports optional hook scripts that run at key lifecycle events: boot, ROM/pak launch, and sleep/resume. This repository contains full documentation and a test pak for validating all hook types.

## Hooks Test Pak

The included **Hooks Test** pak lets you toggle each hook type on/off and view a log of hook events. Use it to verify the hook system is working correctly on your device.

### Building

Requires SDL2, SDL2_ttf, and SDL2_image.

```sh
# macOS (native development)
make native
make mac
make run-native
make run-mac

# Update Apostrophe submodule to the latest pinned origin/main commit
make update-apostrophe

# Cross-compile for device
make tg5040    # or tg5050

# Package builds
make package

# Build, package, and deploy via ADB
make deploy
```

### Usage

1. Launch **Hooks Test** from the Tools menu
2. Select **Configure Hooks** to toggle individual hook types on/off
3. Trigger hooks by booting, launching ROMs, or sleeping the device
4. Select **View Log** to see timestamped hook events

---

## Hook System Documentation

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

### Boot Hooks

Boot hooks run once when the system starts, after platform initialization and after `auto.sh` (if present).

```
$USERDATA_PATH/.hooks/boot.d/
```

#### Relationship to auto.sh

The existing `auto.sh` mechanism (`$USERDATA_PATH/auto.sh`) continues to work unchanged for backward compatibility. If both `auto.sh` and `boot.d/` scripts exist, `auto.sh` runs first.

Unlike `auto.sh` (a single shared file), `boot.d/` is composable: each pak installs its own script with a descriptive filename. Use numeric prefixes to control ordering:

```
boot.d/10-wifi-setup.sh
boot.d/20-led-daemon.sh
boot.d/50-my-pak-init.sh
```

### Launch Hooks

Launch hooks run immediately before and after a ROM or tool pak is launched from the menu.

```
$USERDATA_PATH/.hooks/pre-launch.d/    # before launch
$USERDATA_PATH/.hooks/post-launch.d/   # after launch exits
```

Pre-launch hooks cannot cancel the launch. They are for observation and setup only.

#### Example: log every ROM launch

```sh
#!/bin/sh
# log-launches.sh
[ "$HOOK_TYPE" = "rom" ] || exit 0
echo "$(date): launched $HOOK_ROM_PATH" >> "$LOGS_PATH/launches.log"
```

#### Example: sync after ROM exit

```sh
#!/bin/sh
# shortcuts-resume.sh
[ "$HOOK_TYPE" = "rom" ] || exit 0

SHORTCUTS_PAK="$SDCARD_PATH/Tools/$PLATFORM/Shortcuts.pak"
[ -x "$SHORTCUTS_PAK/shortcuts" ] || exit 0

"$SHORTCUTS_PAK/shortcuts" --resume-sync-hook >> "$LOGS_PATH/shortcuts-resume-sync.txt" 2>&1
```

### Sleep/Resume Hooks

Sleep hooks run when the device enters and exits sleep mode.

```
$USERDATA_PATH/.hooks/pre-sleep.d/     # before sleep
$USERDATA_PATH/.hooks/post-resume.d/   # after wake
```

**Pre-sleep hooks** run before the device pauses audio, stops daemons, and disables the backlight. WiFi, Bluetooth, and audio are still active. All pre-sleep hooks run **synchronously** regardless of naming — they must complete before the device sleeps.

**Post-resume hooks** run after the device has fully woken up (daemons resumed, backlight restored, audio reinitialized). They run in the background by default.

#### Example: save state before sleep

```sh
#!/bin/sh
# save-state.sync.sh
sync
```

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
