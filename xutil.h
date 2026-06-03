#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>

// ── Shared X11 state ──────────────────────────────────────────────────────────
extern Display           *disp;
extern int                scrn;
extern Window             root;
extern Window             win;
extern GC                 gc;
extern Pixmap             backbuffer;
extern XWindowAttributes  _xwa;

#define W _xwa.width
#define H _xwa.height

// ── Lifecycle ─────────────────────────────────────────────────────────────────
int  xutil_init(void);
int  xutil_create_window(void);
int  xutil_create_gc(void);
int  xutil_create_backbuffer(void);
int  xutil_grab_pointer(void);
void xutil_set_cursor(unsigned int shape); // XC_* constant from cursorfont.h
void xutil_cleanup(void);
