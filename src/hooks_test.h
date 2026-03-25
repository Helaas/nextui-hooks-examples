/*
 * hooks_test.h — Shared types and declarations for the Hooks Test pak.
 */
#ifndef HOOKS_TEST_H
#define HOOKS_TEST_H

#include <stdbool.h>

/* ── Hook types ─────────────────────────────────────────────── */

typedef enum {
    HOOK_BOOT = 0,
    HOOK_PRE_LAUNCH,
    HOOK_POST_LAUNCH,
    HOOK_PRE_SLEEP,
    HOOK_POST_RESUME,
    HOOK_COUNT
} hook_type;

/* ── Settings ───────────────────────────────────────────────── */

typedef struct {
    bool async_enabled[HOOK_COUNT];
    bool sync_enabled[HOOK_COUNT];
} hook_settings;

/* ── hooks.c ────────────────────────────────────────────────── */

/* Settings persistence */
hook_settings hooks_load_settings(void);
int           hooks_save_settings(const hook_settings *s);

/* Hook install/uninstall */
int  hooks_install_script(hook_type type, bool sync);
int  hooks_uninstall_script(hook_type type, bool sync);
int  hooks_apply_settings(const hook_settings *s);
bool hooks_is_async_installed(hook_type type);
bool hooks_is_sync_installed(hook_type type);

/* Log management */
int  hooks_read_log(char *buf, int max_len);
int  hooks_clear_log(void);

/* Helpers */
const char *hooks_type_label(hook_type type);
const char *hooks_type_dir(hook_type type);

/* ── ui.c ───────────────────────────────────────────────────── */

typedef enum {
    MAIN_ACTION_CONFIGURE = 0,
    MAIN_ACTION_VIEW_LOG,
    MAIN_ACTION_CLEAR_LOG,
    MAIN_ACTION_INSTALL_ALL,
    MAIN_ACTION_UNINSTALL_ALL,
    MAIN_ACTION_QUIT,
} main_action;

main_action show_main_menu(void);
void        show_configure_screen(void);
void        show_log_viewer(void);
void        run_app(void);

#endif /* HOOKS_TEST_H */
