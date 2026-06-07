#pragma once

#include <stdio.h>
#include <X11/keysym.h>

#define OPTDIR        "~/Pictures/screenshots/"
#define OPTFORMAT     "%d-%02d-%02d_%02d:%02d:%02d.webp"
#define OPTFORMATARGS tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
                      tm.tm_hour, tm.tm_min, tm.tm_sec
#define OPTQUALITY    80

// ── Scripts ───────────────────────────────────────────────────────────────────
// Directory scanned for *.sh files (bound to 1–9 in alphabetical order).
#define OPTSCRIPTDIR "~/.config/screenshot/exec"

// Set to NULL to disable annotation.
#define OPTANNOTATE "satty"

#define OPTGRABDELAY 150
// Pre Selection - Escape / right-click always cancels.
#define OPTKEY_FULLSCREEN XK_f

// Post-selection action keybinds
#define OPTKEY_SAVE     XK_s
#define OPTKEY_COPY     XK_y
#define OPTKEY_ANNOTATE XK_a

// Additional keybinds that run scripts from OPTSCRIPTDIR by name:
//   { keysym, "script-name-without-.sh" }
// Example:  #define OPTSCRIPTBINDS { { XK_u, "upload" } }

// Selection rect appearance
#define OPTWIDTH      1         
#define OPTR          255       
#define OPTG          255       
#define OPTB          255       
#define OPTHANDLESIZE 8         
#define OPTDIMCOLOR   0x80000000

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
