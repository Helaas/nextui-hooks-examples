/*
 * hooks.c — Hook install/uninstall, settings persistence, and log management.
 */
#include "apostrophe.h"
#include "hooks_test.h"
#include "cjson/cjson.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ── Constants ──────────────────────────────────────────────── */

#define MAX_PATH   512
#define LOG_NAME   "hooks-test.log"
#define SETTINGS_NAME "hooks-test-settings.json"
#define HOOK_SCRIPT_NAME      "hooks-test.sh"
#define HOOK_SYNC_SCRIPT_NAME "hooks-test.sync.sh"

/* ── Hook metadata ──────────────────────────────────────────── */

static const char *hook_labels[HOOK_COUNT] = {
    [HOOK_BOOT]        = "Boot",
    [HOOK_PRE_LAUNCH]  = "Pre-Launch",
    [HOOK_POST_LAUNCH] = "Post-Launch",
    [HOOK_PRE_SLEEP]   = "Pre-Sleep",
    [HOOK_POST_RESUME] = "Post-Resume",
};

static const char *hook_dirs[HOOK_COUNT] = {
    [HOOK_BOOT]        = "boot.d",
    [HOOK_PRE_LAUNCH]  = "pre-launch.d",
    [HOOK_POST_LAUNCH] = "post-launch.d",
    [HOOK_PRE_SLEEP]   = "pre-sleep.d",
    [HOOK_POST_RESUME] = "post-resume.d",
};

const char *hooks_type_label(hook_type type)
{
    if (type < 0 || type >= HOOK_COUNT) return "Unknown";
    return hook_labels[type];
}

const char *hooks_type_dir(hook_type type)
{
    if (type < 0 || type >= HOOK_COUNT) return "";
    return hook_dirs[type];
}

/* ── Path helpers ───────────────────────────────────────────── */

static bool path_concat(char *out, size_t size,
                        const char *a,
                        const char *b,
                        const char *c,
                        const char *d)
{
    const char *parts[] = { a, b, c, d };
    char *dst = out;
    size_t remaining = size;

    if (!out || size == 0) return false;
    out[0] = '\0';

    for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
        const char *part = parts[i];
        if (!part) continue;

        size_t len = strlen(part);
        if (len >= remaining) {
            out[0] = '\0';
            return false;
        }

        memcpy(dst, part, len);
        dst += len;
        remaining -= len;
        *dst = '\0';
    }

    return true;
}

static bool get_userdata_path(char *out, int size)
{
    const char *p = getenv("USERDATA_PATH");
    if (p) {
        return path_concat(out, (size_t)size, p, NULL, NULL, NULL);
    }
#if AP_PLATFORM_IS_DEVICE
    const char *sd = getenv("SDCARD_PATH");
    if (!sd) sd = "/mnt/SDCARD";
    return path_concat(out, (size_t)size, sd, "/.userdata/", AP_PLATFORM_NAME, NULL);
#else
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    return path_concat(out, (size_t)size, home, "/.userdata/desktop", NULL, NULL);
#endif
}

static bool get_shared_userdata_path(char *out, int size)
{
    const char *p = getenv("SHARED_USERDATA_PATH");
    if (p) {
        return path_concat(out, (size_t)size, p, NULL, NULL, NULL);
    }
#if AP_PLATFORM_IS_DEVICE
    const char *sd = getenv("SDCARD_PATH");
    if (!sd) sd = "/mnt/SDCARD";
    return path_concat(out, (size_t)size, sd, "/.userdata/shared", NULL, NULL);
#else
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    return path_concat(out, (size_t)size, home, "/.userdata/shared", NULL, NULL);
#endif
}

static bool get_logs_path(char *out, int size)
{
    const char *p = getenv("LOGS_PATH");
    if (p) {
        return path_concat(out, (size_t)size, p, NULL, NULL, NULL);
    }
    char ud[MAX_PATH];
    if (!get_userdata_path(ud, sizeof(ud))) return false;
    return path_concat(out, (size_t)size, ud, "/logs", NULL, NULL);
}

static bool get_log_file_path(char *out, int size)
{
    char logs[MAX_PATH];
    if (!get_logs_path(logs, sizeof(logs))) return false;
    return path_concat(out, (size_t)size, logs, "/", LOG_NAME, NULL);
}

static bool get_settings_path(char *out, int size)
{
    char shared[MAX_PATH];
    if (!get_shared_userdata_path(shared, sizeof(shared))) return false;
    return path_concat(out, (size_t)size, shared, "/", SETTINGS_NAME, NULL);
}

static bool get_hook_script_path(hook_type type, char *out, int size)
{
    const char *dir = hooks_type_dir(type);
    char ud[MAX_PATH];
    char dir_path[MAX_PATH];
    if (dir[0] == '\0') return false;
    if (!get_userdata_path(ud, sizeof(ud))) return false;
    if (!path_concat(dir_path, sizeof(dir_path), ud, "/.hooks/", dir, NULL)) return false;
    return path_concat(out, (size_t)size, dir_path, "/", HOOK_SCRIPT_NAME, NULL);
}

static bool get_hook_sync_script_path(hook_type type, char *out, int size)
{
    const char *dir = hooks_type_dir(type);
    char ud[MAX_PATH];
    char dir_path[MAX_PATH];
    if (dir[0] == '\0') return false;
    if (!get_userdata_path(ud, sizeof(ud))) return false;
    if (!path_concat(dir_path, sizeof(dir_path), ud, "/.hooks/", dir, NULL)) return false;
    return path_concat(out, (size_t)size, dir_path, "/", HOOK_SYNC_SCRIPT_NAME, NULL);
}

static bool get_hook_dir_path(hook_type type, char *out, int size)
{
    const char *dir = hooks_type_dir(type);
    char ud[MAX_PATH];
    if (dir[0] == '\0') return false;
    if (!get_userdata_path(ud, sizeof(ud))) return false;
    return path_concat(out, (size_t)size, ud, "/.hooks/", dir, NULL);
}

/* Recursive mkdir -p */
static int mkdirp(const char *path)
{
    char tmp[MAX_PATH];
    if (!path_concat(tmp, sizeof(tmp), path, NULL, NULL, NULL)) return -1;
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

/* ── Settings ───────────────────────────────────────────────── */

static const char *settings_keys[HOOK_COUNT] = {
    "boot", "pre_launch", "post_launch", "pre_sleep", "post_resume"
};

hook_settings hooks_load_settings(void)
{
    hook_settings s = {0};
    char path[MAX_PATH];
    if (!get_settings_path(path, sizeof(path))) {
        ap_log("hooks: settings path exceeds %d bytes", MAX_PATH);
        return s;
    }

    FILE *f = fopen(path, "r");
    if (!f) return s;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 4096) { fclose(f); return s; }

    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return s; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return s;

    for (int i = 0; i < HOOK_COUNT; i++) {
        cJSON *item = cJSON_GetObjectItem(root, settings_keys[i]);
        if (cJSON_IsString(item)) {
            const char *val = cJSON_GetStringValue(item);
            if (strcmp(val, "async") == 0) s.mode[i] = HOOK_MODE_ASYNC;
            else if (strcmp(val, "sync") == 0) s.mode[i] = HOOK_MODE_SYNC;
            else s.mode[i] = HOOK_MODE_OFF;
        } else if (cJSON_IsBool(item)) {
            /* backwards compat: old bool format */
            s.mode[i] = cJSON_IsTrue(item) ? HOOK_MODE_ASYNC : HOOK_MODE_OFF;
        }
    }

    cJSON_Delete(root);
    return s;
}

int hooks_save_settings(const hook_settings *s)
{
    char path[MAX_PATH];
    if (!get_settings_path(path, sizeof(path))) {
        ap_log("hooks: settings path exceeds %d bytes", MAX_PATH);
        return -1;
    }

    /* Ensure parent directory exists */
    char dir[MAX_PATH];
    if (!get_shared_userdata_path(dir, sizeof(dir))) {
        ap_log("hooks: shared userdata path exceeds %d bytes", MAX_PATH);
        return -1;
    }
    if (mkdirp(dir) != 0) {
        ap_log("hooks: failed to create settings dir %s", dir);
        return -1;
    }

    static const char *mode_strings[] = { "off", "async", "sync" };

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    for (int i = 0; i < HOOK_COUNT; i++)
        cJSON_AddStringToObject(root, settings_keys[i], mode_strings[s->mode[i]]);

    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json) return -1;

    FILE *f = fopen(path, "w");
    if (!f) { free(json); return -1; }
    fputs(json, f);
    fclose(f);
    free(json);

    ap_log("hooks: settings saved to %s", path);
    return 0;
}

/* ── Hook script generation ─────────────────────────────────── */

static const char *boot_script_template =
    "#!/bin/sh\n"
    "LOG=\"${LOGS_PATH:-/mnt/SDCARD/.userdata/${PLATFORM:-unknown}/logs}/%s\"\n"
    "echo \"[$(date '+%%Y-%%m-%%d %%H:%%M:%%S')] %s\""
    " >> \"$LOG\"\n";

static const char *launch_script_template =
    "#!/bin/sh\n"
    "LOG=\"${LOGS_PATH:-/mnt/SDCARD/.userdata/${PLATFORM:-unknown}/logs}/%s\"\n"
    "echo \"[$(date '+%%Y-%%m-%%d %%H:%%M:%%S')] %s"
    " | TYPE=$HOOK_TYPE EMU=$HOOK_EMU_PATH ROM=$HOOK_ROM_PATH LAST=$HOOK_LAST\""
    " >> \"$LOG\"\n";

static const char *sleep_script_template =
    "#!/bin/sh\n"
    "LOG=\"${LOGS_PATH:-/mnt/SDCARD/.userdata/${PLATFORM:-unknown}/logs}/%s\"\n"
    "echo \"[$(date '+%%Y-%%m-%%d %%H:%%M:%%S')] %s"
    " | PHASE=$HOOK_PHASE CATEGORY=$HOOK_CATEGORY\""
    " >> \"$LOG\"\n";

static int write_hook_script(hook_type type, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    const char *label = hook_labels[type];

    switch (type) {
    case HOOK_BOOT:
        fprintf(f, boot_script_template, LOG_NAME, label);
        break;
    case HOOK_PRE_LAUNCH:
    case HOOK_POST_LAUNCH:
        fprintf(f, launch_script_template, LOG_NAME, label);
        break;
    case HOOK_PRE_SLEEP:
    case HOOK_POST_RESUME:
        fprintf(f, sleep_script_template, LOG_NAME, label);
        break;
    default:
        fclose(f);
        return -1;
    }

    fclose(f);
    chmod(path, 0755);
    return 0;
}

/* ── Install / Uninstall ────────────────────────────────────── */

int hooks_install(hook_type type, hook_mode mode)
{
    if (type < 0 || type >= HOOK_COUNT) return -1;
    if (mode == HOOK_MODE_OFF) return hooks_uninstall(type);

    bool use_sync = (mode == HOOK_MODE_SYNC);

    char dir[MAX_PATH], script[MAX_PATH];
    if (!get_hook_dir_path(type, dir, sizeof(dir))) {
        ap_log("hooks: hook dir path exceeds %d bytes", MAX_PATH);
        return -1;
    }

    bool ok = use_sync
        ? get_hook_sync_script_path(type, script, sizeof(script))
        : get_hook_script_path(type, script, sizeof(script));
    if (!ok) {
        ap_log("hooks: hook script path exceeds %d bytes", MAX_PATH);
        return -1;
    }

    if (mkdirp(dir) != 0) {
        ap_log("hooks: failed to create dir %s", dir);
        return -1;
    }

    if (write_hook_script(type, script) != 0) {
        ap_log("hooks: failed to write script %s", script);
        return -1;
    }

    ap_log("hooks: installed %s (%s) -> %s", hook_labels[type],
           use_sync ? "sync" : "async", script);
    return 0;
}

int hooks_uninstall(hook_type type)
{
    if (type < 0 || type >= HOOK_COUNT) return -1;

    int errors = 0;

    char script[MAX_PATH];
    if (!get_hook_script_path(type, script, sizeof(script))) {
        ap_log("hooks: hook script path exceeds %d bytes", MAX_PATH);
        errors++;
    } else if (unlink(script) != 0 && errno != ENOENT) {
        ap_log("hooks: failed to remove %s: %s", script, strerror(errno));
        errors++;
    }

    char sync_script[MAX_PATH];
    if (!get_hook_sync_script_path(type, sync_script, sizeof(sync_script))) {
        ap_log("hooks: hook sync script path exceeds %d bytes", MAX_PATH);
        errors++;
    } else if (unlink(sync_script) != 0 && errno != ENOENT) {
        ap_log("hooks: failed to remove %s: %s", sync_script, strerror(errno));
        errors++;
    }

    ap_log("hooks: uninstalled %s", hook_labels[type]);
    return errors > 0 ? -1 : 0;
}

int hooks_apply_settings(const hook_settings *s)
{
    int errors = 0;
    for (int i = 0; i < HOOK_COUNT; i++) {
        if (s->mode[i] != HOOK_MODE_OFF) {
            if (hooks_install((hook_type)i, s->mode[i]) != 0) errors++;
        } else {
            if (hooks_uninstall((hook_type)i) != 0) errors++;
        }
    }
    return errors > 0 ? -1 : 0;
}

hook_mode hooks_get_mode(hook_type type)
{
    if (type < 0 || type >= HOOK_COUNT) return HOOK_MODE_OFF;

    char sync_script[MAX_PATH];
    if (get_hook_sync_script_path(type, sync_script, sizeof(sync_script)) &&
        access(sync_script, F_OK) == 0)
        return HOOK_MODE_SYNC;

    char script[MAX_PATH];
    if (get_hook_script_path(type, script, sizeof(script)) &&
        access(script, F_OK) == 0)
        return HOOK_MODE_ASYNC;

    return HOOK_MODE_OFF;
}

/* ── Log management ─────────────────────────────────────────── */

int hooks_read_log(char *buf, int max_len)
{
    char path[MAX_PATH];
    if (!get_log_file_path(path, sizeof(path))) {
        ap_log("hooks: log path exceeds %d bytes", MAX_PATH);
        return -1;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(buf, max_len, "No log entries yet.\n\n"
                 "Enable some hooks, then trigger them by:\n"
                 "  - Rebooting (boot hook)\n"
                 "  - Launching a ROM (pre/post-launch hooks)\n"
                 "  - Sleeping the device (pre-sleep/post-resume hooks)");
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fclose(f);
        snprintf(buf, max_len, "Log file is empty.\n\n"
                 "Hooks have not fired yet. Try triggering one.");
        return 0;
    }

    /* Read last portion if file is larger than buffer */
    if (len >= max_len - 1) {
        fseek(f, len - (max_len - 2), SEEK_SET);
        int n = fread(buf, 1, max_len - 2, f);
        buf[n] = '\0';
        /* Skip to first complete line */
        char *nl = strchr(buf, '\n');
        if (nl) {
            memmove(buf, nl + 1, strlen(nl + 1) + 1);
        }
    } else {
        int n = fread(buf, 1, len, f);
        buf[n] = '\0';
    }

    fclose(f);
    return 0;
}

int hooks_clear_log(void)
{
    char path[MAX_PATH];
    if (!get_log_file_path(path, sizeof(path))) {
        ap_log("hooks: log path exceeds %d bytes", MAX_PATH);
        return -1;
    }

    if (unlink(path) != 0 && errno != ENOENT) {
        ap_log("hooks: failed to clear log: %s", strerror(errno));
        return -1;
    }

    ap_log("hooks: log cleared");
    return 0;
}
