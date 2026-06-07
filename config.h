#pragma once

#include <stdio.h>
#include <X11/keysym.h>

// ── Output ────────────────────────────────────────────────────────────────────
#define OPTDIR        "~/Pictures/screenshots/"
#define OPTFORMAT     "%d-%02d-%02d_%02d:%02d:%02d.webp"
#define OPTFORMATARGS tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
                      tm.tm_hour, tm.tm_min, tm.tm_sec
#define OPTQUALITY    80

// ── Scripts ───────────────────────────────────────────────────────────────────
// Directory scanned for *.sh files (bound to 1–9 in alphabetical order).
#define OPTSCRIPTDIR "~/.config/screenshot/exec"

// ── Annotation tool ───────────────────────────────────────────────────────────
// Called with the saved file path as the first argument.
// Set to NULL to disable annotation.
#define OPTANNOTATE "satty"

// ── Grab delay ────────────────────────────────────────────────────────────────
// Milliseconds to wait before grabbing pointer/keyboard after launch.
// Needed when triggered via a WM keybind so the trigger key is fully
// released before we grab, preventing it leaking into the tool.
#define OPTGRABDELAY 150
// Press before drawing. Any other key or a left-click starts region mode.
// Escape / right-click always cancels.
#define OPTKEY_FULLSCREEN XK_f

// ── Post-selection action keybinds ────────────────────────────────────────────
#define OPTKEY_SAVE     XK_s   // save to OPTDIR
#define OPTKEY_COPY     XK_y   // copy to clipboard (xclip)
#define OPTKEY_ANNOTATE XK_a   // open OPTANNOTATE

// Additional keybinds that run scripts from OPTSCRIPTDIR by name:
//   { keysym, "script-name-without-.sh" }
// Example:  #define OPTSCRIPTBINDS { { XK_u, "upload" } }
// #define OPTSCRIPTBINDS { { XK_u, "upload" } }

// ── Selection rect appearance ─────────────────────────────────────────────────
#define OPTWIDTH      1           // border line width (px)
#define OPTR          255         // border colour R
#define OPTG          255         // border colour G
#define OPTB          255         // border colour B
#define OPTHANDLESIZE 8           // resize handle square size (px)
#define OPTDIMCOLOR   0x80000000  // overlay colour outside selection

// ── Debug logging ─────────────────────────────────────────────────────────────
#define die(...) { \
    printf("\033[1;31mError:\033[0m "); \
    printf(__VA_ARGS__); \
    putchar('\n'); \
    goto end; \
}

#ifdef DEBUG
    #define debug(...) { \
        printf("\033[1;33mDebug:\033[0m "); \
        printf(__VA_ARGS__); \
        putchar('\n'); \
    }
#else
    #define debug(...) {}
#endif
