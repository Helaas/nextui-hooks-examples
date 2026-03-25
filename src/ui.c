/*
 * ui.c — All Apostrophe UI flows: main menu, hook configuration,
 *         log viewer, and log management.
 */
#include "apostrophe.h"
#include "apostrophe_widgets.h"
#include "hooks_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Utility screens ────────────────────────────────────────── */

static void show_error(const char *message)
{
    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Back" },
    };
    ap_message_opts opts = {
        .message = message,
        .footer = footer,
        .footer_count = 1,
    };
    ap_confirm_result result;
    (void)ap_confirmation(&opts, &result);
}

static void show_info(const char *message)
{
    ap_footer_item footer[] = {
        { .button = AP_BTN_A, .label = "OK", .is_confirm = true },
    };
    ap_message_opts opts = {
        .message = message,
        .footer = footer,
        .footer_count = 1,
    };
    ap_confirm_result result;
    (void)ap_confirmation(&opts, &result);
}

static bool show_confirm(const char *message, const char *confirm_label)
{
    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Cancel" },
        { .button = AP_BTN_A, .label = confirm_label, .is_confirm = true },
    };
    ap_message_opts opts = {
        .message = message,
        .footer = footer,
        .footer_count = 2,
    };
    ap_confirm_result result = {0};
    int rc = ap_confirmation(&opts, &result);
    return rc == AP_OK && result.confirmed;
}

/* ── Toggle index helpers ──────────────────────────────────── */

/* Map (async_enabled, sync_enabled) → toggle index:
 *   0 = Off, 1 = Async, 2 = Sync, 3 = Both */
static int settings_to_toggle(bool async_on, bool sync_on)
{
    if (async_on && sync_on) return 3;
    if (sync_on)             return 2;
    if (async_on)            return 1;
    return 0;
}

static void toggle_to_settings(int idx, bool *async_on, bool *sync_on)
{
    *async_on = (idx == 1 || idx == 3);
    *sync_on  = (idx == 2 || idx == 3);
}

/* ── Configure hooks screen ─────────────────────────────────── */

void show_configure_screen(void)
{
    hook_settings settings = hooks_load_settings();

    /* Sync with what's actually on disk */
    for (int i = 0; i < HOOK_COUNT; i++) {
        settings.async_enabled[i] = hooks_is_async_installed((hook_type)i);
        settings.sync_enabled[i]  = hooks_is_sync_installed((hook_type)i);
    }

    ap_option toggle_opts[] = {
        { .label = "Off",   .value = "0" },
        { .label = "Async", .value = "1" },
        { .label = "Sync",  .value = "2" },
        { .label = "Both",  .value = "3" },
    };

    ap_options_item items[HOOK_COUNT];
    for (int i = 0; i < HOOK_COUNT; i++) {
        items[i] = (ap_options_item){
            .label = hooks_type_label((hook_type)i),
            .options = toggle_opts,
            .option_count = 4,
            .selected_option = settings_to_toggle(
                settings.async_enabled[i], settings.sync_enabled[i]),
        };
    }

    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Back" },
        { .button = AP_BTN_LEFT, .label = "Toggle", .button_text = "\xe2\x86\x90/\xe2\x86\x92" },
        { .button = AP_BTN_A, .label = "Save", .is_confirm = true },
    };

    ap_options_list_opts opts = {
        .title = "Configure Hooks",
        .items = items,
        .item_count = HOOK_COUNT,
        .footer = footer,
        .footer_count = 3,
        .confirm_button = AP_BTN_A,
        .help_text = "Off: no script. Async: runs in background.\n"
                     "Sync: blocks until complete (.sync.sh).\n"
                     "Both: installs async and sync scripts.\n"
                     "Pre-Sleep always runs sync regardless.",
        .label_font = ap_get_font(AP_FONT_MEDIUM),
    };

    ap_options_list_result result = {0};
    int rc = ap_options_list(&opts, &result);
    if (rc != AP_OK) return;

    /* Extract new settings */
    for (int i = 0; i < HOOK_COUNT; i++)
        toggle_to_settings(result.items[i].selected_option,
                           &settings.async_enabled[i],
                           &settings.sync_enabled[i]);

    /* Apply: install or uninstall hooks */
    if (hooks_apply_settings(&settings) != 0)
        ap_log("ui: some hooks failed to apply");

    if (hooks_save_settings(&settings) != 0) {
        show_error("Could not save settings.");
        return;
    }

    /* Count enabled */
    int count = 0;
    for (int i = 0; i < HOOK_COUNT; i++)
        if (settings.async_enabled[i] || settings.sync_enabled[i]) count++;

    char msg[128];
    snprintf(msg, sizeof(msg), "%d hook%s enabled.",
             count, count == 1 ? "" : "s");
    show_info(msg);
}

/* ── Log viewer ─────────────────────────────────────────────── */

#define LOG_BUF_SIZE (16 * 1024)

void show_log_viewer(void)
{
    char *log_buf = malloc(LOG_BUF_SIZE);
    if (!log_buf) {
        show_error("Out of memory.");
        return;
    }

    hooks_read_log(log_buf, LOG_BUF_SIZE);

    ap_detail_section sections[] = {
        {
            .type = AP_SECTION_DESCRIPTION,
            .title = NULL,
            .description = log_buf,
        },
    };

    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Back" },
    };

    ap_detail_opts opts = {
        .title = "Hook Log",
        .sections = sections,
        .section_count = 1,
        .footer = footer,
        .footer_count = 1,
    };

    ap_detail_result result = {0};
    (void)ap_detail_screen(&opts, &result);

    free(log_buf);
}

/* ── Main menu ──────────────────────────────────────────────── */

main_action show_main_menu(void)
{
    /* Build status suffix for configure item */
    int count = 0;
    for (int i = 0; i < HOOK_COUNT; i++)
        if (hooks_is_async_installed((hook_type)i) ||
            hooks_is_sync_installed((hook_type)i))
            count++;

    char configure_desc[64];
    snprintf(configure_desc, sizeof(configure_desc),
             "%d of %d enabled", count, HOOK_COUNT);

    ap_list_item items[] = {
        AP_LIST_ITEM("Configure Hooks",  configure_desc),
        AP_LIST_ITEM("View Log",         NULL),
        AP_LIST_ITEM("Clear Log",        NULL),
        AP_LIST_ITEM("Install All",      NULL),
        AP_LIST_ITEM("Uninstall All",    NULL),
    };

    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Quit" },
        { .button = AP_BTN_A, .label = "Select", .is_confirm = true },
    };

    ap_list_opts opts = ap_list_default_opts("Hooks Test", items, 5);
    opts.footer = footer;
    opts.footer_count = 2;

    ap_list_result result = {0};
    int rc = ap_list(&opts, &result);
    if (rc != AP_OK)
        return MAIN_ACTION_QUIT;

    switch (result.selected_index) {
    case 0: return MAIN_ACTION_CONFIGURE;
    case 1: return MAIN_ACTION_VIEW_LOG;
    case 2: return MAIN_ACTION_CLEAR_LOG;
    case 3: return MAIN_ACTION_INSTALL_ALL;
    case 4: return MAIN_ACTION_UNINSTALL_ALL;
    default: return MAIN_ACTION_QUIT;
    }
}

/* ── App loop ───────────────────────────────────────────────── */

void run_app(void)
{
    for (;;) {
        main_action action = show_main_menu();
        switch (action) {
        case MAIN_ACTION_CONFIGURE:
            show_configure_screen();
            break;
        case MAIN_ACTION_VIEW_LOG:
            show_log_viewer();
            break;
        case MAIN_ACTION_CLEAR_LOG:
            if (show_confirm("Clear the hook log?", "Clear")) {
                hooks_clear_log();
                show_info("Log cleared.");
            }
            break;
        case MAIN_ACTION_INSTALL_ALL: {
            hook_settings s;
            for (int i = 0; i < HOOK_COUNT; i++) {
                s.async_enabled[i] = true;
                s.sync_enabled[i] = true;
            }
            hooks_apply_settings(&s);
            hooks_save_settings(&s);
            show_info("All hooks installed (async + sync).");
            break;
        }
        case MAIN_ACTION_UNINSTALL_ALL: {
            hook_settings s = {0};
            hooks_apply_settings(&s);
            hooks_save_settings(&s);
            show_info("All hooks uninstalled.");
            break;
        }
        case MAIN_ACTION_QUIT:
            return;
        }
    }
}
