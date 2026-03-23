/*
 * main.c — Entry point and Apostrophe initialisation.
 *
 * Apostrophe implementation is included here (exactly once).
 */
#define AP_IMPLEMENTATION
#include "apostrophe.h"
#define AP_WIDGETS_IMPLEMENTATION
#include "apostrophe_widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hooks_test.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    ap_config cfg = {0};
    cfg.window_title = "Hooks Test";
    cfg.font_path    = AP_PLATFORM_IS_DEVICE ? NULL
                       : "third_party/apostrophe/res/font.ttf";
    cfg.log_path     = ap_resolve_log_path("hooks-test");
    cfg.is_nextui    = AP_PLATFORM_IS_DEVICE;
    cfg.cpu_speed    = AP_CPU_SPEED_MENU;

    if (ap_init(&cfg) != AP_OK) {
        fprintf(stderr, "Failed to initialise Apostrophe: %s\n",
                ap_get_error());
        return 1;
    }

    ap_log("startup: platform=%s", AP_PLATFORM_NAME);

    run_app();

    ap_quit();
    return 0;
}
