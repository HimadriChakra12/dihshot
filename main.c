#include "config.h"
#include "xutil.h"
#include "capture.h"
#include "select.h"
#include "save.h"
#include "scripts.h"
#include "state.h"

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    (void)argv;

    if (!xutil_init())
        die("Failed to open display / get root window size");

    // ── Full-screen mode (any argument given) ─────────────────────────────────
    if (argc > 1) {
        if (!screenshot()) die("Failed to capture screen");
        XSync(disp, False);
        if (save_image() != 0) goto end;
        // save_image execs xclip on success — only reach here on exec failure
        goto end;
    }

    // ── Interactive mode ──────────────────────────────────────────────────────
    if (!xutil_create_window())     die("Failed to create overlay window");
    if (!xutil_create_gc())         die("Failed to create GC");
    if (!xutil_create_backbuffer()) die("Failed to create backbuffer");
    // Note: pointer grab is done inside run_selection() per mode

    {
        int result = run_selection();
        if (result == SELECT_ERROR)  die("Failed to capture screen");
        if (result == SELECT_CANCEL) goto end;
    }

    // Crop image to selection
    img = XSubImage(img,
                    select_x(), select_y(),
                    select_w(), select_h());
    if (!img) die("XSubImage failed");

    XSync(disp, False);

    // Unmap the overlay before any external tool (e.g. satty) tries to draw
    if (win) XUnmapWindow(disp, win);
    XSync(disp, False);

    {
        // Save to disk first — scripts always receive a file path
        char saved_path[4096];
        if (save_image_path(saved_path, sizeof(saved_path)) != 0)
            die("Failed to save screenshot");

        int script_idx = select_chosen_script();

        if (script_idx >= 0 && script_idx < select_nscripts()) {
            // Run chosen script with rect + file env vars
            Rect r = { select_x(), select_y(), select_w(), select_h() };
            scripts_run(&select_scripts()[script_idx], saved_path, &r);
        } else {
            // Default: just print the path and copy to clipboard
            printf("%s\n", saved_path);
            char *args[] = {
                "xclip", "-selection", "clipboard",
                "-t", "image/png", saved_path, NULL
            };
            execvp(args[0], args);
            // execvp returns only on failure — not fatal, file is already saved
        }
    }

    xutil_cleanup();
    if (img) XDestroyImage(img);
    return 0;

end:
    xutil_cleanup();
    if (img) XDestroyImage(img);
    return 1;
}
